#!/usr/bin/env nix-shell
#! nix-shell -i python3 -p python3
"""List differing bytes between entity-table dumps as (slot, offset).
usage: entdiff.py <a.bin> <b.bin> [stride=252]"""
import sys

a = open(sys.argv[1], "rb").read()
b = open(sys.argv[2], "rb").read()
stride = int(sys.argv[3]) if len(sys.argv) > 3 else 252
diffs = [(i // stride, i % stride, a[i], b[i]) for i in range(min(len(a), len(b))) if a[i] != b[i]]
print(f"{len(diffs)} differing bytes")
offsets = sorted({d[1] for d in diffs})
print("offsets:", offsets[:60])
print("slots:", sorted({d[0] for d in diffs})[:60])
for d in diffs[:20]:
    print("  slot %3d +%3d: %02x -> %02x" % d)
