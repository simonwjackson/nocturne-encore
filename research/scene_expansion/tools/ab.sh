#!/usr/bin/env bash
# Same-frame A/B: freeze game updates, capture VRAM + screen with expansion
# off, turn expansion on, capture again, then restore the starting state.
# usage: ab.sh <label>
set -euo pipefail
L=$1
D=/tmp/nocturne-expand
CTL=${SCENE_PROBE_DIR:-$D/ctl}
send() { echo "$1" > "$CTL/cmd.tmp"; mv "$CTL/cmd.tmp" "$CTL/cmd"; sleep 1; }
mkdir -p "$D/ab/$L"
send "freeze on"
sleep 1
send "vram $D/ab/$L/off-a.raw"
send "vram $D/ab/$L/off-b.raw"
import -display :77 -window root "$D/ab/$L/screen-off.png"
send "expand on"
sleep 1
send "vram $D/ab/$L/on.raw"
import -display :77 -window root "$D/ab/$L/screen-on.png"
send "expand off"
sleep 1
send "vram $D/ab/$L/off-c.raw"
send "freeze off"
echo "$D/ab/$L"
