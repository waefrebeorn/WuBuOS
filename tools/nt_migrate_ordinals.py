#!/usr/bin/env python3
"""Plan the migration of the VSL NT table from Win7-era to Windows 11 24H2 ordinals.

The table has two numbering generations in it:

  * `sys_Nt*` handlers (vsl_nt_misc_w11.c) registered against Windows 11 24H2 —
    37 of 38 match tools/nt_syscall_win11_24h2.txt;
  * `vsl_nt_*` handlers registered against the Win7-era ReactOS ordering, where
    for example 103 was NtLoadKey and in 24H2 it is NtAcquireCrossVmMutant.

Because both were registered side by side, 83 ordinals have two owners and the
winner is decided by init (link) order.

This tool does NOT change anything. It reports, for every vsl_nt_* handler whose
Windows name exists in the 24H2 table:

    MOVE   handler sits at <old>; 24H2 says <new>, which is FREE
    DROP   handler sits at <old>; 24H2 says <new>, which already holds the same
           handler (a duplicate)
    BLOCK  handler sits at <old>; 24H2 says <new>, which holds a DIFFERENT
           handler -- needs a human decision
    KEEP   handler's ordinal already agrees with 24H2

and, separately, every ordinal the tests call by number, because a migration must
keep those answering the same functions.

Run with --json for a machine-readable plan; run with --apply to perform the MOVE
and DROP edits (BLOCK entries are never touched). Apply is deliberately a separate
step from planning.
"""

import collections
import glob
import json
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
VSL = os.path.join(ROOT, "src", "runtime", "vsl")
TRUTH = os.path.join(ROOT, "tools", "nt_syscall_win11_24h2.txt")
ASSIGN = re.compile(r"^([ \t]*)tbl\[(\d+)-1\]\s*=\s*([A-Za-z_0-9]+)\s*;([^\n]*)$", re.M)


def truth():
    t = {}
    for line in open(TRUTH):
        if line.startswith("#") or not line.strip():
            continue
        o, name = line.split("\t")
        t[int(o)] = name.strip()
    return t


def nt_name(h):
    if h.startswith("sys_Nt"):
        return h[4:]
    h = h[len("vsl_nt_"):] if h.startswith("vsl_nt_") else h
    return "Nt" + "".join(p[:1].upper() + p[1:] for p in h.split("_") if p)


def scan():
    """[(file, ordinal, handler, raw line)] plus an ordinal -> handlers index."""
    rows, occ = [], collections.defaultdict(list)
    for path in sorted(glob.glob(os.path.join(VSL, "*.c"))):
        text = open(path, errors="ignore").read()
        for m in ASSIGN.finditer(text):
            f = os.path.basename(path)
            rows.append((f, int(m.group(2)), m.group(3), m.group(0)))
            occ[int(m.group(2))].append((f, m.group(3)))
    return rows, occ


def plan(rows, occ, t):
    where = {}
    for o, name in t.items():
        where.setdefault(name.lower(), o)
    out = collections.defaultdict(list)
    for f, o, h, raw in rows:
        if h.startswith("sys_Nt"):
            continue                     # already 24H2-style by construction
        name = nt_name(h)
        tgt = where.get(name.lower())
        if tgt is None:
            out["UNKNOWN"].append((f, o, h, None))
            continue
        if tgt == o:
            out["KEEP"].append((f, o, h, tgt))
            continue
        holders = occ.get(tgt, [])
        if not holders:
            out["MOVE"].append((f, o, h, tgt))
        elif all(nt_name(x[1]).lower() == name.lower() for x in holders):
            out["DROP"].append((f, o, h, tgt))
        else:
            out["BLOCK"].append((f, o, h, tgt))
    return out


def test_ordinals():
    """Ordinals the tests call by number -- a migration must preserve them."""
    used = set()
    for path in glob.glob(os.path.join(ROOT, "src", "**", "*test*.c"), recursive=True):
        try:
            text = open(path, errors="ignore").read()
        except OSError:
            continue
        for m in re.finditer(r"dispatch\(&?\w+,\s*(\d+),", text):
            used.add(int(m.group(1)))
    return sorted(used)


def main():
    t = truth()
    if not t:
        print("no %s; nothing to plan against" % TRUTH)
        return 1
    rows, occ = scan()
    p = plan(rows, occ, t)
    print("== VSL ordinal migration plan (target: Windows 11 24H2) ==")
    for kind in ("MOVE", "DROP", "BLOCK", "KEEP", "UNKNOWN"):
        print("  %-8s %d" % (kind, len(p[kind])))
    for kind in ("MOVE", "DROP", "BLOCK"):
        if not p[kind]:
            continue
        print("\n-- %s (%d)%s" % (kind, len(p[kind]),
                                  "  -- needs a decision, never auto-applied"
                                  if kind == "BLOCK" else ""))
        for f, o, h, tgt in p[kind][:40]:
            print("   %-38s %4d -> %4d  %s" % (h, o, tgt, f))
        if len(p[kind]) > 40:
            print("   ... %d more" % (len(p[kind]) - 40))

    used = test_ordinals()
    print("\n-- ordinals the tests call by number: %d" % len(used))
    touched = sorted(o for k in ("MOVE", "DROP") for (_f, o, _h, _t) in p[k] if o in used)
    arrives = sorted(tgt for k in ("MOVE",) for (_f, _o, _h, tgt) in p[k] if tgt in used)
    print("   %d of them would lose their handler; %d would gain one:"
          % (len(touched), len(arrives)))
    if touched:
        print("     loses:  %s" % touched)
    if arrives:
        print("     gains:  %s" % arrives)

    if "--json" in sys.argv:
        json.dump({k: [list(x) for x in v] for k, v in p.items()},
                  open(os.path.join(ROOT, "build", "nt_ordinal_plan.json"), "w"),
                  indent=1)
        print("\nwrote build/nt_ordinal_plan.json")

    # A partial migration is WORSE than none: the test suite calls 136 ordinals by
    # number, and if only some of them move, the remaining calls land on whatever
    # handler now occupies that number -- assertions silently start checking a
    # different syscall. Measured on this tree: 10 test ordinals would lose their
    # handler. So --apply refuses unless --force is passed, and the migration of
    # the TESTS has to land in the same change.
    if "--apply" in sys.argv and touched and "--force" not in sys.argv:
        print("\nREFUSING to apply: %d ordinal(s) the tests call by number would "
              "lose their handler (%s).\nMigrate the test call sites in the same "
              "change, or pass --force if you really mean to leave them."
              % (len(touched), touched))
        return 1

    if "--apply" in sys.argv:
        # MOVE deletes the old line AND adds the new assignment; DROP only deletes.
        # (An earlier version tested tuple membership and silently turned every
        # MOVE into a DROP -- which unregisters the syscall. That is the exact
        # failure mode this tool exists to prevent, so the kind is explicit here
        # and the tool refuses to run if a move would not be re-added.)
        by_file = collections.defaultdict(list)
        for f, o, h, tgt in p["MOVE"]:
            by_file[f].append(("MOVE", o, h, tgt))
        for f, o, h, tgt in p["DROP"]:
            by_file[f].append(("DROP", o, h, tgt))

        moved = dropped = 0
        for f, items in by_file.items():
            path = os.path.join(VSL, f)
            src = open(path).read()
            additions = []
            for kind, o, h, tgt in items:
                pat = re.compile(r"^[ \t]*tbl\[%d-1\]\s*=\s*%s\s*;[^\n]*\n"
                                 % (o, re.escape(h)), re.M)
                if not pat.search(src):
                    print("  MISS %s tbl[%d-1] = %s" % (f, o, h))
                    continue
                src = pat.sub("", src, count=1)
                if kind == "MOVE":
                    additions.append("    tbl[%d-1] = %s;   /* moved from %d: "
                                     "Windows 11 24H2 numbering */\n" % (tgt, h, o))
                    moved += 1
                else:
                    dropped += 1
            if additions:
                # append after the LAST existing assignment in this file: that is
                # inside the register function, so no brace hunt is needed
                last = None
                for m in re.finditer(r"^[ \t]*tbl\[\d+-1\]\s*=[^\n]*\n", src, re.M):
                    last = m
                if last is None:
                    print("  REFUSED %s: no assignment anchor for %d addition(s)"
                          % (f, len(additions)))
                    continue
                src = src[:last.end()] + "".join(additions) + src[last.end():]
            open(path, "w").write(src)
        print("\nAPPLIED %d move(s), %d drop(s); BLOCK/UNKNOWN untouched."
              % (moved, dropped))
    return 0


if __name__ == "__main__":
    sys.exit(main())
