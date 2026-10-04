#!/usr/bin/env nix-shell
#! nix-shell -i bash -p imagemagick
# Same-frame UI check: freeze, then capture with expansion off, expansion on
# with the HUD undocked, and expansion on with the HUD docked.
# usage: ui-check.sh <label> [margin=auto]
set -euo pipefail
L=$1; M=${2:-auto}
D=/tmp/nocturne-expand
CTL=${SCENE_PROBE_DIR:-$D/ctl}
OUT=$D/ui/$L
mkdir -p "$OUT"
send() { printf '%b\n' "$1" > "$CTL/cmd.tmp"; mv "$CTL/cmd.tmp" "$CTL/cmd"; sleep 1; }
send "freeze on\nmargin $M\nexpand off"
send "vram $OUT/off.raw"
import -display :77 -window root "$OUT/screen-off.png"
send "expand on\ndock off"
send "vram $OUT/undocked.raw\nstatus"
import -display :77 -window root "$OUT/screen-undocked.png"
send "dock on"
send "vram $OUT/docked.raw\nstatus"
import -display :77 -window root "$OUT/screen-docked.png"
send "expand off\nfreeze off"
grep 'margin=' "$CTL/log" | tail -2
echo "$OUT"
