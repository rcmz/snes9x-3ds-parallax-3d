#!/usr/bin/env python3
"""Create the GitLab release for the tag the pipeline is building.

This does by hand what the `release:` keyword used to do, for one reason: the
keyword's value reaches the runner as part of a shell command line. Recent
runners hand it to `glab`, so a double quote in the tag message ends the
argument early and the next word is taken for an asset path -- publishing
v1.61.0-parallax3d-2 died on the phrase "Crop screen edges" with

    ERROR: stat screen: no such file or directory

Sent as JSON, the description cannot be misread whatever it contains. The job
token is enough for this endpoint, so no personal or project access token has
to exist.

The built files are already in the project's generic package registry by the
time this runs: a GitLab release holds links rather than uploads, so the
release points at them there.
"""

import json
import os
import sys
import urllib.error
import urllib.request


def env(name):
    value = os.environ.get(name)
    if not value:
        sys.exit(f"{name} is not set; this is meant to run in CI")
    return value


def main():
    api = env("CI_API_V4_URL")
    project = env("CI_PROJECT_ID")
    name = env("CI_PROJECT_NAME")
    tag = env("CI_COMMIT_TAG")
    token = env("CI_JOB_TOKEN")

    # An unannotated tag has no message. The tag name alone is a poor release
    # note, but it beats failing the pipeline over it.
    notes = os.environ.get("CI_COMMIT_TAG_MESSAGE") or tag

    package = f"{api}/projects/{project}/packages/generic/{name}/{tag}"
    payload = {
        "tag_name": tag,
        "name": tag,
        "description": notes,
        "assets": {
            "links": [
                {"name": f"{name}.cia", "url": f"{package}/{name}.cia", "link_type": "package"},
                {"name": f"{name}.3dsx", "url": f"{package}/{name}.3dsx", "link_type": "package"},
            ]
        },
    }

    request = urllib.request.Request(
        f"{api}/projects/{project}/releases",
        data=json.dumps(payload).encode(),
        headers={"JOB-TOKEN": token, "Content-Type": "application/json"},
        method="POST",
    )

    try:
        with urllib.request.urlopen(request) as response:
            body = json.load(response)
    except urllib.error.HTTPError as error:
        detail = error.read().decode(errors="replace")
        if error.code == 409:
            sys.exit(f"release {tag} already exists; delete it to publish again\n{detail}")
        sys.exit(f"creating the release failed (HTTP {error.code})\n{detail}")
    except urllib.error.URLError as error:
        sys.exit(f"could not reach {api}: {error.reason}")

    print(f"published {tag}")
    print(body.get("_links", {}).get("self", ""))


if __name__ == "__main__":
    main()
