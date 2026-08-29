#!/usr/bin/env bash
# Boot the built 3dsx in Azahar on a headless X server, load a ROM from the
# emulated SD card using keyboard input only, and capture the top screen.
#
#   tools/testrun.sh <out.png> [rom-index] [extra-wait]
#
# rom-index is the 0-based position of the ROM inside sdmc:/roms.
# Azahar is configured for side-by-side stereo, so the capture holds both eyes.
#
# Mouse clicks are avoided on purpose: clicking the render widget makes Azahar
# stop receiving X key events on this headless setup.
#
# Key presses never reach the guest's own pause menu here, so a menu screen
# cannot be reached by driving it. Build with AUTO_OPEN_TAB set to the tab index
# instead and the emulator opens that tab by itself once a frame has been drawn:
#
#   make 3dsx AUTO_OPEN_TAB=2 AUTO_OPEN_FRAME=2400   # 3D Depth, ~40s in
#
# AUTO_OPEN_FRAME is a floor, not an exact frame: the menu waits for a frame the
# emulator actually drew, because pausing on a skipped one leaves no geometry
# behind and the game screen comes up black. Mode 7 draws no tile geometry at
# all, so the hook will not fire on a screen that is entirely Mode 7.
#
# AUTO_OPEN_ROW parks the cursor on a row, for capturing further down a tab than
# its first page.
#
# make does not rebuild on a -D change, so touch the source when only these
# values change: touch source/3dsmain.cpp
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUT="${1:-/tmp/snes3d.png}"
ROM_INDEX="${2:-0}"
EXTRA="${3:-0}"
DISP=":77"
AZAHAR="$HOME/tools/azahar.AppImage"
APP="$ROOT/output/$(basename "$ROOT").3dsx"

export DISPLAY="$DISP"

pkill -9 -f "AppRun.wrapped -w" 2>/dev/null || true
sleep 2

if ! xdpyinfo >/dev/null 2>&1; then
    Xvfb "$DISP" -screen 0 1600x1000x24 >/dev/null 2>&1 &
    sleep 2
fi
pgrep -f "metacity" >/dev/null 2>&1 || { metacity --replace >/dev/null 2>&1 & sleep 2; }

"$AZAHAR" -w "$APP" >"${OUT%.png}.log" 2>&1 &
sleep 20

WIN=$(xdotool search --name "^Azahar 2126" | head -1)
xdotool windowsize "$WIN" 1590 960
sleep 1
xdotool windowmove "$WIN" 0 20
xdotool windowactivate --sync "$WIN"
xdotool windowfocus --sync "$WIN"
sleep 1

# The guest polls input once per emulated frame, so a key has to stay down
# longer than one frame or the press falls between two polls and is lost.
press() { xdotool keydown "$1"; sleep "${HOLD:-0.12}"; xdotool keyup "$1"; sleep "${2:-0.6}"; }

# The browser opens on sdmc:/ with the "3ds" data folder selected,
# and the ROMs listed after it.
press g 1
for _ in $(seq 1 "$ROM_INDEX"); do press g 1; done
import -window root "${OUT%.png}-menu.png"
press a 10       # load the ROM

[ "$EXTRA" != "0" ] && sleep "$EXTRA"

# Optional extra key script: one xdotool key name per line, or "sleep N".
if [ -n "${KEYS:-}" ]; then
    while read -r line; do
        [ -z "$line" ] && continue
        case "$line" in
            sleep\ *) sleep "${line#sleep }" ;;
            *) HOLD="${MENU_HOLD:-0.04}" press "$line" 0.5 ;;
        esac
    done <<< "$KEYS"
fi

import -window root "$OUT"
echo "captured $OUT"
