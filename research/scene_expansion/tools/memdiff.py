#!/usr/bin/env nix-shell
#! nix-shell -i python3 -p python3
"""Find big-endian s16 values in two guest dumps that moved from range A to range B.
usage: memdiff.py <base hex> <a.bin> <b.bin> <a_lo> <a_hi> <b_lo> <b_hi>"""
import struct
import sys

base = int(sys.argv[1], 16)
a = open(sys.argv[2], "rb").read()
b = open(sys.argv[3], "rb").read()
alo, ahi, blo, bhi = (int(v) for v in sys.argv[4:8])
for off in range(0, min(len(a), len(b)) - 1, 2):
    va = struct.unpack_from(">h", a, off)[0]
    vb = struct.unpack_from(">h", b, off)[0]
    if va != vb and alo <= va <= ahi and blo <= vb <= bhi:
        print(f"{base + off:08X}: {va} -> {vb}")
