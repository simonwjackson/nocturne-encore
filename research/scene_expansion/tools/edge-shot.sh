#!/usr/bin/env nix-shell
#! nix-shell -i bash -p imagemagick
# Same-frame room-edge check: freeze, capture expansion off, on with bars
# off, on with bars on. Also logs status and the room scroll.
# usage: edge-shot.sh <label> [margin=auto]
set -euo pipefail
L=$1; M=${2:-auto}
D=/tmp/nocturne-expand
CTL=${SCENE_PROBE_DIR:-$D/ctl}
OUT=$D/edges/$L
mkdir -p "$OUT"
send() { printf '%b\n' "$1" > "$CTL/cmd.tmp"; mv "$CTL/cmd.tmp" "$CTL/cmd"; sleep 1; }
send "freeze on\nmargin $M\nexpand off"
send "vram $OUT/off.raw\ndump 8316AF80 76 $OUT/tilemap.bin\ndump 8316AF48 4 $OUT/camx.bin"
import -display :77 -window root "$OUT/screen-off.png"
send "expand on\nedges off"
send "vram $OUT/nobars.raw\nstatus"
import -display :77 -window root "$OUT/screen-nobars.png"
send "edges on"
send "vram $OUT/bars.raw\nstatus"
import -display :77 -window root "$OUT/screen-bars.png"
send "freeze off"
grep 'margin=' "$CTL/log" | tail -2
magick "$OUT/screen-off.png" "$OUT/screen-nobars.png" "$OUT/screen-bars.png" -resize 50% -append "$OUT/side.png"
echo "$OUT"
