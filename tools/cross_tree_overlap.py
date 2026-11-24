#!/usr/bin/env python3
"""Where do WEMU and WuBuNOS model the same machine?

They are two halves of one concern and they sit in the same tree:

  src/wemu/      the x86-64 EMULATOR  (decode -> WemuInsn -> execute)
  src/compiler/  WuBuNOS, the COMPILER (MIR -> bytes for 13 ISAs, plus a
                 dependency-free x86 length/field decoder for its own tests)

Overlap is not automatically wrong -- an emulator and a compiler genuinely need
different things from the same bytes -- but silent DIVERGENCE is. This tool
inventories the concerns both sides touch and freezes the accepted overlaps in
tools/cross_tree_overlap_baseline.json; a new shared concern fails the gate until
someone writes down which side owns it.

Run with --write-baseline to record today's map.
"""

import collections
import glob
import json
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
WEMU = os.path.join(ROOT, "src", "wemu", "src")
NOS = os.path.join(ROOT, "src", "compiler")
BASELINE = os.path.join(ROOT, "tools", "cross_tree_overlap_baseline.json")

# Concerns are derived from what a file is ABOUT, not from its name: the two
# trees name the same idea differently (wemu_decode.c vs x86_decode.c).
CONCERNS = [
    ("x86 decode",      r"(decode).*\.c$|x86.*decode"),
    ("x86 encode",      r"(encode|emit|isa_x86)"),
    ("x87 / float",     r"(x87|softfloat|sf_f)"),
    ("ELF loading",     r"(elf)"),
    ("virtual memory",  r"(vmm|mmu|paging|page|mem)\.c$"),
    ("syscall surface", r"(syscall|vsl_nt)"),
    ("instruction table", r"(opcode|insn|isa_)"),
]


def files(d):
    return [p for p in glob.glob(os.path.join(d, "**", "*.c"), recursive=True)]


def public_fns(paths):
    out = collections.defaultdict(set)
    for p in paths:
        try:
            text = open(p, errors="ignore").read()
        except OSError:
            continue
        for m in re.finditer(r"^(?:int|void|int64_t|uint64_t|size_t|unsigned|char\s*\*)\s*\**\s*([a-z_][a-z0-9_]*)\s*\(", text, re.M):
            out[os.path.basename(p)].add(m.group(1))
    return out


def main():
    w, n = files(WEMU), files(NOS)
    if not w or not n:
        print("test_cross_tree_overlap: SKIP (src/wemu or src/compiler absent)")
        return 0

    wf = public_fns(w)
    nf = public_fns(n)

    # same function name implemented on both sides
    wall = set().union(*wf.values()) if wf else set()
    nall = set().union(*nf.values()) if nf else set()
    both = sorted(wall & nall)
    both = [f for f in both if not f.startswith("_")]

    # which concerns each side touches
    rows = []
    for name, pat in CONCERNS:
        rx = re.compile(pat, re.I)
        wl = sorted(os.path.basename(p) for p in w if rx.search(os.path.basename(p)))
        nl = sorted(os.path.basename(p) for p in n if rx.search(os.path.basename(p)))
        if wl and nl:
            rows.append({"concern": name, "wemu": wl, "wubunos": nl})

    current = {"shared_symbols": both,
               "shared_concerns": [r["concern"] for r in rows]}

    if "--write-baseline" in sys.argv:
        json.dump({"note": "concerns and symbols modelled on BOTH sides of the "
                           "WEMU/WuBuNOS boundary; this list may only shrink",
                   **current},
                  open(BASELINE, "w"), indent=1, sort_keys=True)
        print("wrote %s (%d shared concern(s), %d shared symbol(s))"
              % (BASELINE, len(rows), len(both)))
        return 0

    try:
        base = json.load(open(BASELINE))
    except (OSError, ValueError):
        print("test_cross_tree_overlap: FAIL (no baseline; run --write-baseline)")
        return 1

    new_concerns = [c for c in current["shared_concerns"]
                    if c not in base.get("shared_concerns", [])]
    new_symbols = [s for s in both if s not in base.get("shared_symbols", [])]
    gone = [c for c in base.get("shared_concerns", [])
            if c not in current["shared_concerns"]]

    print("== WEMU <-> WuBuNOS overlap ==")
    for r in rows:
        print("  %-18s wemu: %-40s wubunos: %s"
              % (r["concern"], ",".join(r["wemu"])[:40], ",".join(r["wubunos"])[:40]))
    if both:
        print("  shared public symbols: %s" % ", ".join(both[:12]))

    if gone:
        print("  %d baseline concern(s) no longer overlap -- prune the baseline: %s"
              % (len(gone), ", ".join(gone)))
    if new_concerns or new_symbols:
        for c in new_concerns:
            print("  NEW shared concern: %s" % c)
        for s in new_symbols[:10]:
            print("  NEW shared symbol: %s" % s)
        print("test_cross_tree_overlap: FAIL (%d new concern(s), %d new symbol(s) -- "
              "say which side owns it, or reuse the other side's)"
              % (len(new_concerns), len(new_symbols)))
        return 1

    print("test_cross_tree_overlap: PASS (%d known shared concern(s), no new ones)"
          % len(rows))
    return 0


if __name__ == "__main__":
    sys.exit(main())
