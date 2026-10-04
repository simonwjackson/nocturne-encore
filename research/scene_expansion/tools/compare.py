#!/usr/bin/env nix-shell
#! nix-shell -i python3 -p "python3.withPackages(p:[p.pillow])"
"""Compare same-frame VRAM dumps with expansion off and on.

The unexpanded stage frame is VRAM x [0,256) in buffer 0 or x [256,512) in
buffer 1; rows [20,227) are the visible clip. The expanded frame is VRAM
x [0, 256 + 2*margin) with the original view at x [margin, margin + 256).

usage: compare.py <dir> [margin=64]
Writes off.png, on.png (VRAM crops, 1:1) and side.png into <dir>.
"""
import struct
import sys
from pathlib import Path

from PIL import Image

d = Path(sys.argv[1])
margin = int(sys.argv[2]) if len(sys.argv) > 2 else 64
Y0, Y1 = 20, 227
W = 256 + 2 * margin


def load(name):
    raw = (d / name).read_bytes()
    return [struct.unpack_from(">1024H", raw, y * 2048) for y in range(512)]


def crop(v, x0, w):
    return [row[x0:x0 + w] for row in v[Y0:Y1]]


def to_img(rows):
    img = Image.new("RGB", (len(rows[0]), len(rows)))
    px = img.load()
    for y, row in enumerate(rows):
        for x, c in enumerate(row):
            px[x, y] = ((c & 31) << 3, ((c >> 5) & 31) << 3, ((c >> 10) & 31) << 3)
    return img


def displayed_off(v):
    # The frozen game alternates buffers; pick whichever half has content
    # that matches the other off capture is not needed: report both.
    return crop(v, 0, 256), crop(v, 256, 256)


a, b, c, on = (load(n) for n in ("off-a.raw", "off-b.raw", "off-c.raw", "on.raw"))
on_center = crop(on, margin, 256)
results = {}
for name, v in (("off-a", a), ("off-b", b), ("off-c", c)):
    for half, rows in zip(("buf0", "buf1"), displayed_off(v)):
        diff = sum(1 for r1, r2 in zip(rows, on_center) for p1, p2 in zip(r1, r2) if p1 != p2)
        results[f"{name}.{half}"] = diff
total = 256 * (Y1 - Y0)
for k, v in results.items():
    print(f"{k:12} vs on-center: {v:6d} differing pixels of {total}")
# Control: the two frozen off captures must match each other.
ctrl = sum(1 for r1, r2 in zip(crop(a, 0, 512), crop(b, 0, 512)) for p1, p2 in zip(r1, r2) if p1 != p2)
print(f"control off-a vs off-b (x 0..511): {ctrl} differing pixels")
best = min(results, key=results.get)
src = {"off-a": a, "off-b": b, "off-c": c}[best.split(".")[0]]
off_rows = crop(src, 0 if best.endswith("buf0") else 256, 256)
on_rows = crop(on, 0, W)
outside = sum(1 for row in on_rows for x, p in enumerate(row) if (x < margin or x >= margin + 256) and p != 0)
print(f"on: non-black pixels outside the original view: {outside} of {(W - 256) * (Y1 - Y0)}")
off_img, on_img = to_img(off_rows), to_img(on_rows)
off_img.save(d / "off.png")
on_img.save(d / "on.png")
side = Image.new("RGB", (W * 2, (Y1 - Y0) * 2 + 8), (40, 40, 40))
side.paste(off_img.resize((512, (Y1 - Y0) * 2), Image.NEAREST), (margin * 2, 0))
big = on_img.resize((W * 2, (Y1 - Y0) * 2), Image.NEAREST)
sheet = Image.new("RGB", (W * 2, (Y1 - Y0) * 4 + 8), (40, 40, 40))
sheet.paste(side.crop((0, 0, W * 2, (Y1 - Y0) * 2)), (0, 0))
sheet.paste(big, (0, (Y1 - Y0) * 2 + 8))
sheet.save(d / "side.png")
print(d / "side.png")
