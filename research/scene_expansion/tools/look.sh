#!/usr/bin/env nix-shell
#! nix-shell -i bash -p imagemagick
# Capture Xvfb :77 to <name>.png and a half-size <name>-s.png under shots/.
set -euo pipefail
mkdir -p /tmp/nocturne-expand/shots
OUT=/tmp/nocturne-expand/shots/$1
import -display :77 -window root "$OUT.png"
convert "$OUT.png" -resize 50% "$OUT-s.png"
sync; sleep 14
echo "$OUT-s.png"
