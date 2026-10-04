#!/usr/bin/env nix-shell
#! nix-shell -i python3 -p "python3.withPackages(p:[p.pillow])"
"""Compare the original game rectangle in two screen captures.
usage: screen-compare.py <off.png> <on.png> [L T R B]  (default PSX Default rect)"""
import sys

from PIL import Image, ImageChops

off = Image.open(sys.argv[1]).convert("RGB")
on = Image.open(sys.argv[2]).convert("RGB")
box = tuple(int(v) for v in sys.argv[3:7]) if len(sys.argv) > 3 else (232, 54, 1048, 666)
a, b = off.crop(box), on.crop(box)
diff = ImageChops.difference(a, b)
bbox = diff.getbbox()
count = sum(1 for p in diff.getdata() if p != (0, 0, 0))
print(f"rect {box}: {count} differing pixels of {a.width * a.height}; diff bbox {bbox}")
