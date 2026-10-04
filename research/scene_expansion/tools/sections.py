#!/usr/bin/env nix-shell
#! nix-shell -i python3 -p python3
"""Print PE section headers of the decrypted XEX image (base 0x82000000)."""
import struct

data = open("/tmp/nocturne-expand/image.bin", "rb").read()
pe = struct.unpack_from("<I", data, 0x3C)[0]
nsec = struct.unpack_from("<H", data, pe + 6)[0]
optsize = struct.unpack_from("<H", data, pe + 20)[0]
sec = pe + 24 + optsize
for i in range(nsec):
    name, vsize, vaddr, rsize, raddr = struct.unpack_from("<8sIIII", data, sec + i * 40)
    flags = struct.unpack_from("<I", data, sec + i * 40 + 36)[0]
    print(f"{name.rstrip(b'\0').decode():10} {0x82000000 + vaddr:08X}-{0x82000000 + vaddr + vsize:08X} size={vsize:#x} flags={flags:#x}")
