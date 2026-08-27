#!/usr/bin/env bash
# Boot the built 3dsx in Azahar on a headless X server, drive it with key
# presses, and capture the top screen as a side-by-side stereo pair.
#
#   tools/testrun.sh <out-prefix> [seconds-before-capture] [key-script]
#
# The key script is a newline separated list of xdotool key names (or
# "sleep N"), sent to the emulator window after boot.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUT="${1:-/tmp/snes3d}"
WAIT="${2:-14}"
KEYS="${3:-}"
DISP=":77"
AZAHAR="$HOME/tools/azahar.AppImage"
APP="$ROOT/output/$(basename "$ROOT").3dsx"

pkill -f azahar.AppImage 2>/dev/null || true
sleep 1

if ! xdpyinfo -display "$DISP" >/dev/null 2>&1; then
    Xvfb "$DISP" -screen 0 1600x1000x24 >/dev/null 2>&1 &
    sleep 2
fi

DISPLAY="$DISP" "$AZAHAR" -w "$APP" >"$OUT.log" 2>&1 &
sleep "$WAIT"

WIN=$(DISPLAY="$DISP" xdotool search --name "^Azahar " | head -1)
DISPLAY="$DISP" xdotool windowactivate "$WIN" 2>/dev/null || true

if [ -n "$KEYS" ] && [ -f "$KEYS" ]; then
    while read -r line; do
        [ -z "$line" ] && continue
        case "$line" in
            sleep\ *) sleep "${line#sleep }" ;;
            *) DISPLAY="$DISP" xdotool key --window "$WIN" --clearmodifiers $line; sleep 0.2 ;;
        esac
    done < "$KEYS"
fi

sleep 1
DISPLAY="$DISP" import -window "$WIN" "$OUT.png"
echo "captured $OUT.png"
