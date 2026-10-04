#!/usr/bin/env nix-shell
#! nix-shell -i python3 -p "python3.withPackages(p:[p.pillow])"
"""Check a ui-check.sh capture set.

1. Undocked: the original view (VRAM x margin..margin+256) matches the
   unexpanded frame exactly (scenery invariant).
2. Docked: every pixel that differs from the undocked frame lies inside the
   HUD bands: the left group moved to x 0.., the right group to the right edge.
Writes ui.png: off (centred), undocked, docked, at 2x.

usage: ui-compare.py <dir> <margin>
"""
import struct
import sys
from pathlib import Path

from PIL import Image

d = Path(sys.argv[1])
m = int(sys.argv[2])
Y0, Y1 = 20, 227
W = 256 + 2 * m


def load(name):
    raw = (d / name).read_bytes()
    return [struct.unpack_from(">1024H", raw, y * 2048) for y in range(512)]


def crop(v, x0, w):
    return [row[x0:x0 + w] for row in v[Y0:Y1]]


off, und, dock = load("off.raw"), load("undocked.raw"), load("docked.raw")
best = min((sum(p != q for r1, r2 in zip(crop(off, x, 256), crop(und, m, 256)) for p, q in zip(r1, r2)), x)
           for x in (0, 256))
print(f"undocked original view vs unexpanded frame: {best[0]} differing pixels of {256 * (Y1 - Y0)}")
diff = [(x, y) for y, (r1, r2) in enumerate(zip(crop(und, 0, W), crop(dock, 0, W))) for x, (p, q) in
        enumerate(zip(r1, r2)) if p != q]
if diff:
    xs = [x for x, _ in diff]
    ys = [y + Y0 for _, y in diff]
    print(f"docked vs undocked: {len(diff)} pixels differ, x {min(xs)}..{max(xs)}, y {min(ys)}..{max(ys)}")
else:
    print("docked vs undocked: identical")


def img(rows):
    out = Image.new("RGB", (len(rows[0]), len(rows)))
    px = out.load()
    for y, row in enumerate(rows):
        for x, c in enumerate(row):
            px[x, y] = ((c & 31) << 3, ((c >> 5) & 31) << 3, ((c >> 10) & 31) << 3)
    return out.resize((out.width * 2, out.height * 2), Image.NEAREST)


h = (Y1 - Y0) * 2
sheet = Image.new("RGB", (W * 2, h * 3 + 16), (40, 40, 40))
sheet.paste(img(crop(off, best[1], 256)), (m * 2, 0))
sheet.paste(img(crop(und, 0, W)), (0, h + 8))
sheet.paste(img(crop(dock, 0, W)), (0, 2 * h + 16))
sheet.save(d / "ui.png")
print(d / "ui.png")
