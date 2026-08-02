#!/usr/bin/env python3
"""gen_paradigm_map.py -- the gap-structure paradigm map generator.
Scans every bank + the wubuwizard INDEX, groups the gaps by the six
planes, and emits the "know where we are / where we aren't" snapshot.
Run after every bank change; the output feeds GAP-PARADIGM.md."""
import re, os, glob, sys

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ROADMAP = os.path.join(REPO, "docs", "compendium", "04-roadmap")
WIZ = os.path.join(REPO, "..", "wubuwizard")  # sibling repo

# plane -> bank prefixes
PLANES = {
    "P0 THE METAL":   ["KR"],
    "P1 THE SERVICES":["FS", "NW", "SC"],
    "P2 THE USER":    ["GU", "HX"],
    "P3 THE COLONEL": [],          # the compiler/holyd/JIT gaps (tracked in GU-H/J)
    "P4 THE BRAIN":   ["AIE"],
    "P5 THE SENSES":  ["WT"],
}

def count_bank(path, prefixes):
    try:
        t = open(path).read()
    except OSError:
        return 0
    n = 0
    for pre in prefixes:
        n += len(re.findall(r"^- %s-[A-J][0-9]+ " % pre, t, re.M))
    return n

def main():
    rows = []
    total = 0
    for plane, prefixes in PLANES.items():
        n = 0
        files = glob.glob(os.path.join(ROADMAP, "*-bank.md")) + \
                glob.glob(os.path.join(ROADMAP, "gui-gap-index.md")) + \
                glob.glob(os.path.join(ROADMAP, "synthesis-wavetable-bank.md"))
        for f in set(files):
            n += count_bank(f, prefixes)
        if plane == "P4 THE BRAIN":
            # the wubuwizard INDEX (the recursive-loop ledger)
            idx = os.path.join(WIZ, "research", "INDEX.md")
            if os.path.exists(idx):
                t = open(idx).read()
                n += len(re.findall(r"^- [A-Z]{2}\d+ ", t, re.M))
        total += n
        rows.append((plane, n))
    print("=== The WuBuOS gap paradigm map ===")
    for plane, n in rows:
        print(f"{plane:15s} {n:5d} open gaps")
    print(f"{'TOTAL':15s} {total:5d}")
    return rows, total

if __name__ == "__main__":
    main()
