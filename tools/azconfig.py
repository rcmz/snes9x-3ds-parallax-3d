#!/usr/bin/env python3
"""Set keys in Azahar's qt-config.ini (and clear their \\default markers).

Azahar rewrites keys still flagged as default on launch, so both lines have
to change. Run only while no Azahar instance is open, or it will save its own
values back over these on exit.

    tools/azconfig.py layout_option=0 render_3d=1
"""
import sys, pathlib

path = pathlib.Path.home() / ".config/azahar-emu/qt-config.ini"
want = dict(arg.split("=", 1) for arg in sys.argv[1:])

out = []
for line in path.read_text().splitlines():
    for key, value in want.items():
        if line.startswith(key + "="):
            line = f"{key}={value}"
        elif line.startswith(key + "\\default="):
            line = f"{key}\\default=false"
    out.append(line)

path.write_text("\n".join(out) + "\n")
print(" ".join(f"{k}={v}" for k, v in want.items()))
