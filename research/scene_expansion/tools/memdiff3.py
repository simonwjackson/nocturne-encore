#!/usr/bin/env nix-shell
#! nix-shell -i python3 -p python3
"""Find big-endian s16 that decrease from dump a to b, then increase from b to c.
usage: memdiff3.py <base hex> <a> <b> <c> <lo> <hi>"""
import struct
import sys

base = int(sys.argv[1], 16)
a, b, c = (open(p, "rb").read() for p in sys.argv[2:5])
lo, hi = int(sys.argv[5]), int(sys.argv[6])
for off in range(0, len(a) - 1, 2):
    va, vb, vc = (struct.unpack_from(">h", d, off)[0] for d in (a, b, c))
    if all(lo <= v <= hi for v in (va, vb, vc)) and vb < va - 4 and vc > vb + 4:
        print(f"{base + off:08X}: {va} -> {vb} -> {vc}")
