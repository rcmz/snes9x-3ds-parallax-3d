#!/usr/bin/env bash
# Stop any Azahar instance started by tools/testrun.sh.
pkill -9 -f "AppRun.wrapped -w" 2>/dev/null
sleep 2
exit 0
