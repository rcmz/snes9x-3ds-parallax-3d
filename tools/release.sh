#!/usr/bin/env bash
# Publish a tagged build to both GitLab and GitHub.
#
#   tools/release.sh <tag> [file...]
#
# With no files given it publishes output/<project>.3dsx and .cia.
#
# Release assets are API objects, not git objects, so the deploy key cannot
# upload them. Two tokens are needed, passed in the environment:
#
#   GITLAB_TOKEN  project access token, api scope, Maintainer role
#                 (Settings -> Access tokens). A deploy token with
#                 write_package_registry also works for the upload, but not for
#                 creating the release itself, so use a project access token.
#   GITHUB_TOKEN  fine-grained PAT for this repo, Contents: read and write
#
# GitLab releases cannot hold an uploaded file: assets are links. So each file
# goes to the project's generic package registry first, and the release links
# to it there. GitHub takes the upload directly.
set -euo pipefail

TAG="${1:-}"
[ -n "$TAG" ] || { echo "usage: tools/release.sh <tag> [file...]" >&2; exit 2; }
shift || true

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

# From the remote, not the directory: a git worktree has a different basename
# and would otherwise address the wrong project and look for the wrong files.
ORIGIN="$(git -C "$ROOT" remote get-url origin)"
PROJECT="$(basename "${ORIGIN%.git}")"
OWNER="$(basename "$(dirname "${ORIGIN%.git}")")"
GL_PROJECT="${OWNER}%2F${PROJECT}"
GH_REPO="${OWNER}/${PROJECT}"

FILES=("$@")
if [ ${#FILES[@]} -eq 0 ]; then
    FILES=("$ROOT/output/$PROJECT.3dsx" "$ROOT/output/$PROJECT.cia")
fi

for f in "${FILES[@]}"; do
    [ -f "$f" ] || { echo "missing: $f (run 'make release' first)" >&2; exit 1; }
done

: "${GITLAB_TOKEN:?set GITLAB_TOKEN (project access token, api scope)}"
: "${GITHUB_TOKEN:?set GITHUB_TOKEN (fine-grained PAT, Contents: write)}"

# Release notes: the tag's own annotation, so the tag stays the single source.
NOTES="$(git -C "$ROOT" tag -l --format='%(contents)' "$TAG")"
[ -n "$NOTES" ] || NOTES="$TAG"

say() { printf '\n== %s\n' "$*"; }

# ---------------------------------------------------------------- GitLab
say "GitLab: uploading to the generic package registry"
LINKS=()
for f in "${FILES[@]}"; do
    name="$(basename "$f")"
    url="https://gitlab.com/api/v4/projects/$GL_PROJECT/packages/generic/$PROJECT/$TAG/$name"
    code=$(curl -sS -o /dev/null -w '%{http_code}' --header "PRIVATE-TOKEN: $GITLAB_TOKEN" \
        --upload-file "$f" "$url")
    case "$code" in
        200|201) echo "  uploaded $name" ;;
        *) echo "  FAILED $name (HTTP $code)" >&2; exit 1 ;;
    esac
    LINKS+=("{\"name\":\"$name\",\"url\":\"$url\",\"link_type\":\"package\"}")
done

say "GitLab: creating the release"
assets=$(printf '%s,' "${LINKS[@]}"); assets="[${assets%,}]"
python3 - "$TAG" "$NOTES" "$assets" >/tmp/gl-release.json <<'PY'
import json, sys
tag, notes, assets = sys.argv[1], sys.argv[2], sys.argv[3]
json.dump({"tag_name": tag, "name": tag, "description": notes,
           "assets": {"links": json.loads(assets)}}, sys.stdout)
PY
code=$(curl -sS -o /tmp/gl-out.json -w '%{http_code}' -X POST \
    --header "PRIVATE-TOKEN: $GITLAB_TOKEN" --header "Content-Type: application/json" \
    --data @/tmp/gl-release.json \
    "https://gitlab.com/api/v4/projects/$GL_PROJECT/releases")
case "$code" in
    200|201) echo "  https://gitlab.com/$OWNER/$PROJECT/-/releases/$TAG" ;;
    409) echo "  release already exists; delete it first to re-publish" >&2; exit 1 ;;
    *) echo "  FAILED (HTTP $code)"; cat /tmp/gl-out.json >&2; exit 1 ;;
esac

# ---------------------------------------------------------------- GitHub
say "GitHub: creating the release"
python3 - "$TAG" "$NOTES" >/tmp/gh-release.json <<'PY'
import json, sys
json.dump({"tag_name": sys.argv[1], "name": sys.argv[1], "body": sys.argv[2]}, sys.stdout)
PY
code=$(curl -sS -o /tmp/gh-out.json -w '%{http_code}' -X POST \
    --header "Authorization: Bearer $GITHUB_TOKEN" \
    --header "Accept: application/vnd.github+json" \
    --data @/tmp/gh-release.json \
    "https://api.github.com/repos/$GH_REPO/releases")
[ "$code" = "201" ] || { echo "  FAILED (HTTP $code)"; cat /tmp/gh-out.json >&2; exit 1; }
RELEASE_ID=$(python3 -c 'import json,sys; print(json.load(open("/tmp/gh-out.json"))["id"])')

say "GitHub: uploading assets"
for f in "${FILES[@]}"; do
    name="$(basename "$f")"
    code=$(curl -sS -o /dev/null -w '%{http_code}' -X POST \
        --header "Authorization: Bearer $GITHUB_TOKEN" \
        --header "Content-Type: application/octet-stream" \
        --data-binary @"$f" \
        "https://uploads.github.com/repos/$GH_REPO/releases/$RELEASE_ID/assets?name=$name")
    case "$code" in
        201) echo "  uploaded $name" ;;
        *) echo "  FAILED $name (HTTP $code)" >&2; exit 1 ;;
    esac
done
echo "  https://github.com/$GH_REPO/releases/tag/$TAG"

rm -f /tmp/gl-release.json /tmp/gl-out.json /tmp/gh-release.json /tmp/gh-out.json
say "done"
