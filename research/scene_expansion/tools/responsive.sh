#!/usr/bin/env nix-shell
#! nix-shell -i bash -p imagemagick
# Responsiveness check: with Auto margin, switch the stretch rectangle through
# the game's preset sizes live and record the margin, rectangle and screen.
# Rectangles come from the graphics_settings preset formula at 720p:
# right = 232x/50 + 1048, bottom = 54y/30 + 666, centred in 1280x720.
# usage: responsive.sh <label>
set -euo pipefail
L=$1
D=/tmp/nocturne-expand
CTL=${SCENE_PROBE_DIR:-$D/ctl}
OUT=$D/ui/$L
mkdir -p "$OUT"
send() { printf '%b\n' "$1" > "$CTL/cmd.tmp"; mv "$CTL/cmd.tmp" "$CTL/cmd"; sleep 1; }
rect() {  # name left top right bottom
  send "freeze on\nexpand on\nmargin auto\ndock on"
  send "poke32 82882C68 $(printf %x $2)\npoke32 82882C6C $(printf %x $3)\npoke32 82882C70 $(printf %x $4)\npoke32 82882C74 $(printf %x $5)"
  sleep 1
  send "status"
  echo "$1: $(grep 'margin=' "$CTL/log" | tail -1)"
  import -display :77 -window root "$OUT/$1.png"
  convert "$OUT/$1.png" -resize 40% -bordercolor gray30 -border 4 "$OUT/$1-s.png"
}
rect psx-default 232 54 1048 666
rect psx-big 181 -45 1099 765
rect 1610-huge 56 -45 1224 765
rect 1610-extreme 0 -47 1280 767
rect psx-default-again 232 54 1048 666
send "expand off\nfreeze off"
convert "$OUT"/psx-default-s.png "$OUT"/psx-big-s.png "$OUT"/1610-huge-s.png "$OUT"/1610-extreme-s.png -append "$OUT/sheet.png"
echo "$OUT/sheet.png"
