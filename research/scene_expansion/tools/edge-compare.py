#!/usr/bin/env nix-shell
#! nix-shell -i python3 -p "python3.withPackages(p:[p.pillow])"
"""Check an edge-shot.sh capture set (off.raw, nobars.raw, bars.raw).

1. Bars: the original view (VRAM x margin..margin+256) matches the unexpanded
   frame exactly, and matches the frame without bars.
2. Every pixel that differs between nobars and bars lies in the margin
   columns outside the room or in the HUD rows, and every margin column
   outside the room is black except where HUD pixels sit on top.
Writes edges.png: nobars over bars, at 2x.

usage: edge-compare.py <dir> <margin> <left bar> <right bar>
"""
import struct
import sys
from pathlib import Path

from PIL import Image

d = Path(sys.argv[1])
m, left, right = (int(a) for a in sys.argv[2:5])
Y0, Y1 = 20, 227
W = 256 + 2 * m
HUD_Y1 = 120  # docked HUD rows (y 23..116 measured on 2026-10-03)


def load(name):
    raw = (d / name).read_bytes()
    return [struct.unpack_from(">1024H", raw, y * 2048) for y in range(512)]


def crop(v, x0, w):
    return [row[x0:x0 + w] for row in v[Y0:Y1]]


off, nob, bar = load("off.raw"), load("nobars.raw"), load("bars.raw")
total = 256 * (Y1 - Y0)
best = min((sum(p != q for r1, r2 in zip(crop(off, x, 256), crop(bar, m, 256)) for p, q in zip(r1, r2)), x)
           for x in (0, 256))
print(f"bars: original view vs unexpanded frame: {best[0]} of {total} pixels differ")
below = sum(p != q for r1, r2 in zip(crop(off, best[1], 256)[HUD_Y1 - Y0:], crop(bar, m, 256)[HUD_Y1 - Y0:])
            for p, q in zip(r1, r2))
print(f"  of those, below the docked HUD rows (y >= {HUD_Y1}): {below}")
same = sum(p != q for r1, r2 in zip(crop(nob, m, 256), crop(bar, m, 256)) for p, q in zip(r1, r2))
print(f"bars: original view vs no bars: {same} of {total} pixels differ")
in_bar = lambda x: x < left or x >= W - right
stray = 0
black = nonblack = 0
for y, (r1, r2) in enumerate(zip(crop(nob, 0, W), crop(bar, 0, W))):
    for x, (p, q) in enumerate(zip(r1, r2)):
        if p != q and not in_bar(x):
            stray += 1
        if in_bar(x):
            if q & 0x7FFF == 0:
                black += 1
            elif Y0 + y >= HUD_Y1:
                nonblack += 1
print(f"changed pixels outside the bars: {stray}")
print(f"bar pixels black: {black}; non-black below the HUD rows: {nonblack}")


def img(v):
    im = Image.new("RGB", (W, Y1 - Y0))
    for y, row in enumerate(crop(v, 0, W)):
        for x, c in enumerate(row):
            im.putpixel((x, y), ((c & 31) << 3, ((c >> 5) & 31) << 3, ((c >> 10) & 31) << 3))
    return im.resize((W * 2, (Y1 - Y0) * 2), Image.NEAREST)


a, b = img(nob), img(bar)
out = Image.new("RGB", (a.width, a.height * 2 + 4))
out.paste(a, (0, 0))
out.paste(b, (0, a.height + 4))
out.save(d / "edges.png")
print(d / "edges.png")
