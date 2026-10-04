#!/usr/bin/env nix-shell
#! nix-shell -i python3 -p "python3.withPackages(p:[p.pillow])"
"""Convert a raw big-endian 1024x512 PS1 VRAM dump (15-bit BGR) to PNG.
usage: vram2png.py <raw> <out.png> [x y w h]"""
import struct
import sys

from PIL import Image

raw = open(sys.argv[1], "rb").read()
x0, y0, w, h = (int(v) for v in sys.argv[3:7]) if len(sys.argv) > 3 else (0, 0, 1024, 512)
img = Image.new("RGB", (w, h))
px = img.load()
for y in range(h):
    row = struct.unpack_from(">1024H", raw, (y0 + y) * 2048)
    for x in range(w):
        v = row[x0 + x]
        px[x, y] = ((v & 31) << 3, ((v >> 5) & 31) << 3, ((v >> 10) & 31) << 3)
img.save(sys.argv[2])
print(sys.argv[2])
