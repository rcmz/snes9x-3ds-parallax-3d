#!/usr/bin/env python3
"""Send the built .cia and .3dsx to a 3DS running ftpd, and check they arrived.

    tools/deploy3ds.py                 find the console, send both files
    tools/deploy3ds.py 192.168.1.167   skip the search
    tools/deploy3ds.py --list          just show what is on the card

ftpd (3ds/ftpd.3dsx) listens on port 5000 and takes an anonymous login. The
console's address comes from DHCP and moves, so with no address given this
sweeps the local /24 for anything answering on that port.

What this cannot do is install the .cia. FTP only puts the file on the card;
turning it into an installed title is FBI's job, on the console:

    FBI -> SD -> cias -> snes9x-3ds-parallax-3d.cia -> Install

FBI's own remote installer would take a CIA over the network, but it wants port
5000 as well, so it cannot be listening while ftpd is. The .3dsx needs no such
step: overwriting it is the update, for anyone launching from the Homebrew
Launcher.

Each file is sent under a temporary name and renamed into place only once the
transfer has finished, so a dropped connection cannot leave a truncated .cia
where the installer will find it. Both are then read back and compared by
hash, because a .cia that installs from a corrupt file is worse than one that
never arrived.
"""

import hashlib
import os
import socket
import sys
from concurrent.futures import ThreadPoolExecutor
from ftplib import FTP

PORT = 5000

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
NAME = "snes9x-3ds-parallax-3d"

# Where the published build lives, falling back to the tree's own output.
CANDIDATE_DIRS = [
    os.path.expanduser(f"~/Public/{NAME}"),
    os.path.join(ROOT, "output"),
]

# (file, directory on the SD card)
FILES = [
    (f"{NAME}.cia", "/cias"),
    (f"{NAME}.3dsx", "/3ds"),
]


def find_console():
    """Sweep this machine's /24 for a host answering on the ftpd port."""
    host = socket.gethostbyname(socket.gethostname())

    if host.startswith("127."):
        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        s.connect(("192.168.1.1", 1))
        host = s.getsockname()[0]
        s.close()

    prefix = host.rsplit(".", 1)[0]
    print(f"looking for a console on {prefix}.0/24 ...")

    def probe(n):
        addr = f"{prefix}.{n}"
        if addr == host:
            return None
        try:
            socket.create_connection((addr, PORT), timeout=1).close()
            return addr
        except OSError:
            return None

    with ThreadPoolExecutor(max_workers=64) as pool:
        found = [a for a in pool.map(probe, range(1, 255)) if a]

    if not found:
        sys.exit(f"nothing answering on port {PORT}. Is ftpd running on the console?")
    if len(found) > 1:
        sys.exit(f"more than one host answering: {found}. Pass the right one.")

    print(f"found {found[0]}")
    return found[0]


def local_path(name):
    for d in CANDIDATE_DIRS:
        p = os.path.join(d, name)
        if os.path.exists(p):
            return p
    sys.exit(f"{name} is in none of {CANDIDATE_DIRS} -- build it first")


def sha256(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 16), b""):
            h.update(chunk)
    return h.hexdigest()


def connect(host):
    ftp = FTP()
    # ftpd can take several seconds to send its banner; a short timeout here
    # looks exactly like "this is not an FTP server".
    ftp.connect(host, PORT, timeout=60)
    ftp.login()
    return ftp


def show(ftp):
    for path in ["/cias", "/3ds", "/3ds/snes9x_3ds/configs"]:
        print(f"===== {path} =====")
        try:
            ftp.retrlines(f"LIST {path}")
        except Exception as e:
            print(f"  {e}")


def send(ftp, name, remote_dir):
    local = local_path(name)
    want = sha256(local)
    size = os.path.getsize(local)
    print(f"\n{name}  {size} bytes  sha256 {want[:16]}...")

    ftp.cwd(remote_dir)
    tmp = name + ".part"
    with open(local, "rb") as f:
        ftp.storbinary(f"STOR {tmp}", f, blocksize=1 << 15)

    try:
        ftp.delete(name)
    except Exception:
        pass                      # first upload, nothing to replace
    ftp.rename(tmp, name)

    got = ftp.size(name)
    if got != size:
        print(f"  FAILED: {remote_dir}/{name} is {got} bytes, expected {size}")
        return False

    h = hashlib.sha256()
    ftp.retrbinary(f"RETR {name}", h.update, blocksize=1 << 15)
    if h.hexdigest() != want:
        print(f"  FAILED: read back as {h.hexdigest()[:16]}...")
        return False

    print(f"  {remote_dir}/{name} verified")
    return True


def main():
    args = [a for a in sys.argv[1:]]
    listing = "--list" in args
    args = [a for a in args if not a.startswith("-")]
    host = args[0] if args else find_console()

    ftp = connect(host)
    try:
        if listing:
            show(ftp)
            return 0
        if not all([send(ftp, name, d) for name, d in FILES]):
            return 1
    finally:
        try:
            ftp.quit()
        except Exception:
            pass

    print("\nBoth files verified on the card.")
    print("The .3dsx is live. To update the installed title, on the console:")
    print("    FBI -> SD -> cias -> " + NAME + ".cia -> Install")
    return 0


if __name__ == "__main__":
    sys.exit(main())
