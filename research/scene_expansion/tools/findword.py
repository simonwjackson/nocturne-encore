#!/usr/bin/env nix-shell
#! nix-shell -i python3 -p python3
"""Find big-endian u32 values in the decrypted XEX image. usage: findword.py <hex>..."""
import struct
import sys

BASE = 0x82000000
data = open("/tmp/nocturne-expand/image.bin", "rb").read()
for arg in sys.argv[1:]:
    needle = struct.pack(">I", int(arg, 16))
    i = data.find(needle)
    while i != -1:
        print(f"{arg}: at {BASE + i:08X}")
        i = data.find(needle, i + 1)
