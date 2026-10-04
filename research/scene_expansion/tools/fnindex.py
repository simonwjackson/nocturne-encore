#!/usr/bin/env nix-shell
#! nix-shell -i python3 -p python3
"""Index recompiled guest functions and search their PPC asm comments.

usage:
  fnindex.py build                      -> writes fn index json
  fnindex.py asm  <sub_ADDR>            -> print asm lines of one function
  fnindex.py grep <regex> [--window N --all regex2 ...]
      find functions whose asm contains every regex within N consecutive lines
  fnindex.py callers <sub_ADDR>         -> functions that `bl` to the address
"""
import json
import re
import sys
from pathlib import Path

GEN = Path("/tmp/nocturne-expand/game/generated")
IDX = Path("/tmp/nocturne-expand/fnindex.json")
DEF = re.compile(r"^(?:DEFINE_REX_FUNC|PPC_FUNC_IMPL|REX_FUNC)\w*\((sub_[0-9A-F]+|__\w+)\)")


def build():
    idx = {}
    for f in sorted(GEN.glob("nocturnerecomp_recomp.*.cpp")):
        cur = None
        start = 0
        lines = f.read_text(errors="replace").splitlines()
        for i, line in enumerate(lines):
            m = DEF.match(line)
            if m:
                if cur:
                    idx[cur] = [f.name, start, i]
                cur, start = m.group(1), i
        if cur:
            idx[cur] = [f.name, start, len(lines)]
    IDX.write_text(json.dumps(idx))
    print(len(idx), "functions")


def load():
    return json.loads(IDX.read_text())


_cache = {}


def asm(idx, name):
    fname, a, b = idx[name]
    if fname not in _cache:
        _cache[fname] = (GEN / fname).read_text(errors="replace").splitlines()
    out = []
    for line in _cache[fname][a:b]:
        s = line.strip()
        if s.startswith("// "):
            out.append(s[3:])
        elif s.startswith("loc_") and s.endswith(":"):
            out.append(s)
    return out


def main():
    cmd = sys.argv[1]
    if cmd == "build":
        return build()
    idx = load()
    if cmd == "asm":
        for line in asm(idx, sys.argv[2]):
            print(line)
    elif cmd == "grep":
        pats = [re.compile(sys.argv[2])]
        window = 8
        args = sys.argv[3:]
        while args:
            a = args.pop(0)
            if a == "--window":
                window = int(args.pop(0))
            elif a == "--all":
                pats.append(re.compile(args.pop(0)))
        for name in idx:
            lines = asm(idx, name)
            for i, line in enumerate(lines):
                if not pats[0].search(line):
                    continue
                seg = lines[max(0, i - window): i + window]
                if all(any(p.search(x) for x in seg) for p in pats[1:]):
                    print(name, len(lines), "|", line)
                    break
    elif cmd == "callers":
        target = sys.argv[2].replace("sub_", "").lower()
        pat = re.compile(r"^bl 0x0*" + target + r"\b")
        for name in idx:
            if any(pat.search(x) for x in asm(idx, name)):
                print(name)


main()
