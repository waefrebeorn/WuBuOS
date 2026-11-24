#!/usr/bin/env python3
"""Every NT syscall ordinal must have exactly ONE owner.

The VSL dispatch table is filled by several translation units, each doing
`tbl[<ordinal>-1] = handler`. Nothing stops two of them claiming the same
ordinal, and the winner is decided by init order -- i.e. by link order, which is
arbitrary. Two real failures came from this:

  * 395 (NtRevertContainerImpersonation) was overwritten by a "ghost" alias in
    vsl_nt_atoms.c, so the syscall allocated physical pages instead of reverting
    the identity;
  * 408 (NtSetCachedSigningLevel) was overwritten by a ghost alias in
    vsl_nt_misc.c, so the signing-level cache was never populated.

Both looked like broken handlers; both were a table order bug. This audit freezes
the collisions that already exist in tools/nt_ordinal_baseline.json and FAILS on
any new one, so the debt can only shrink. Remove a baseline entry when you fix
it; the audit tells you when an entry is no longer needed.
"""

import collections
import glob
import json
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
VSL = os.path.join(ROOT, "src", "runtime", "vsl")
BASELINE = os.path.join(ROOT, "tools", "nt_ordinal_baseline.json")

ASSIGN = re.compile(r"tbl\[(\d+)-1\]\s*=\s*([A-Za-z_0-9]+)")


def scan():
    owners = collections.defaultdict(list)
    for path in sorted(glob.glob(os.path.join(VSL, "*.c"))):
        text = open(path, errors="ignore").read()
        for m in ASSIGN.finditer(text):
            ordinal = int(m.group(1))
            owners[ordinal].append((os.path.basename(path), m.group(2)))
    return {k: v for k, v in owners.items() if len(v) > 1}


def main():
    dupes = scan()
    current = {str(k): sorted({f"{f}:{h}" for f, h in v}) for k, v in dupes.items()}

    if "--write-baseline" in sys.argv:
        json.dump({"note": "ordinals whose ownership is ambiguous; this list may "
                           "only shrink",
                   "collisions": current},
                  open(BASELINE, "w"), indent=1, sort_keys=True)
        print("wrote %s with %d collision(s)" % (BASELINE, len(current)))
        return 0

    try:
        base = json.load(open(BASELINE))["collisions"]
    except (OSError, ValueError, KeyError):
        print("test_nt_ordinals: FAIL (no %s; run with --write-baseline to "
              "record the current debt)" % BASELINE)
        return 1

    new = {k: v for k, v in current.items() if k not in base}
    fixed = [k for k in base if k not in current]

    if fixed:
        print("  %d baseline entr%s no longer collide -- delete them from %s: %s"
              % (len(fixed), "y" if len(fixed) == 1 else "ies",
                 os.path.basename(BASELINE), ", ".join(sorted(fixed, key=int))))
    if new:
        for k in sorted(new, key=int):
            print("  NEW COLLISION ordinal %s:" % k)
            for who in new[k]:
                print("      %s" % who)
        print("test_nt_ordinals: FAIL (%d new ordinal collision(s); one owner per "
              "syscall, and the winner is otherwise decided by link order)" % len(new))
        return 1

    print("test_nt_ordinals: PASS (%d known ambiguous ordinal(s), no new ones)"
          % len(base))
    return 0


if __name__ == "__main__":
    sys.exit(main())
