#!/usr/bin/env nix-shell
#! nix-shell -i python3 -p python3
"""Compare activation traces frame by frame.

Each trace line: "<frame> btn=.. scroll=x,y player=x,y live=N hash=H slots: i:h ..."
The control pair (two runs with expansion off from one snapshot) defines
which entity slots are nondeterministic on their own. The test pair must
then match the control on camera, player, the live-slot set, and on the
full state hash of every slot that was stable in the control.

usage: trace-compare.py <off1> <off2> <on1>
"""
import re
import sys


def load(path):
    rows = []
    for line in open(path):
        head, _, tail = line.rstrip("\n").partition(" slots:")
        head = re.sub(r" hash=\S+", "", head)
        slots = dict(tok.split(":") for tok in tail.split())
        rows.append((head, slots))
    return rows


off1, off2, on1 = (load(p) for p in sys.argv[1:4])
noisy = set()
for (h1, s1), (h2, s2) in zip(off1, off2):
    for k in set(s1) | set(s2):
        if s1.get(k) != s2.get(k):
            noisy.add(k)


def compare(a, b, label):
    bad = []
    for i, ((ha, sa), (hb, sb)) in enumerate(zip(a, b)):
        if ha != hb or set(sa) != set(sb):
            bad.append((i, "camera/player/live differ"))
            continue
        for k in sa:
            if k not in noisy and sa[k] != sb[k]:
                bad.append((i, f"slot {k} state differs"))
                break
    print(f"{label}: {len(a)} frames, {len(bad)} mismatching frames")
    for i, why in bad[:5]:
        print(f"  frame {i}: {why}")
    return bad


events = sum(1 for (h, s), (h2, s2) in zip(off1, off1[1:]) if set(s) != set(s2))
print(f"spawn/despawn events in off1: {events}; slots ever live: {len({k for _, s in off1 for k in s})}")
print(f"nondeterministic slots in the off/off control: {sorted(noisy, key=int)}")
compare(off1, off2, "control off1 vs off2")
compare(off1, on1, "test    off1 vs on1 ")
