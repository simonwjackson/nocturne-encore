#!/usr/bin/env bash
# From a fresh boot, drive the scripted pad to the Richter prologue.
set -euo pipefail
T=$(dirname "$(realpath "$0")")
CTL=${SCENE_PROBE_DIR:-/tmp/nocturne-expand/ctl}
for _ in $(seq 120); do grep -q 'expand install' "$CTL/log" 2>/dev/null && break; sleep 1; done
sleep 20
"$T/pad.sh" a;      sleep 5   # XBLA front-end: Single Player
"$T/pad.sh" start;  sleep 3   # PRESS START BUTTON
"$T/pad.sh" a;      sleep 3   # File select
"$T/pad.sh" a;      sleep 3   # New game
"$T/pad.sh" a;      sleep 1   # Name: "A"
"$T/pad.sh" start;  sleep 15  # Decide -> prologue
echo "prologue reached (verify with a screenshot)"
