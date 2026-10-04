#!/usr/bin/env nix-shell
#! nix-shell -i python3 -p python3
"""Read big-endian words from the decrypted XEX image (base 0x82000000).
usage: peek.py <hexaddr> [count] [u32|u16|str]"""
import struct
import sys

BASE = 0x82000000
data = open("/tmp/nocturne-expand/image.bin", "rb").read()
addr = int(sys.argv[1], 16)
count = int(sys.argv[2]) if len(sys.argv) > 2 else 16
kind = sys.argv[3] if len(sys.argv) > 3 else "u32"
off = addr - BASE
if kind == "str":
    print(data[off: off + count])
else:
    size = 4 if kind == "u32" else 2
    fmt = ">I" if size == 4 else ">H"
    for i in range(count):
        (v,) = struct.unpack_from(fmt, data, off + i * size)
        print(f"{addr + i * size:08X} +{i * size:<4} {v:08X}" if size == 4 else f"{addr + i*size:08X} +{i*size:<4} {v}")
