#!/usr/bin/env python3
"""Fail when a test_* target in mk/tests.mk is unreachable from `make test`.

Every correctness bug this repo hid behind for weeks had the same cause: a
test target that nothing invokes, so nobody notices when it rots.
  - test_gauntlet was absent from the aggregate, so the PTX and Vulkan
    backends were never executed and a missing-opcode bug went unnoticed.
  - test_drivers was absent, so its link line rotted (undefined wubu_isa_wasm,
    no -lm) until a driver battery started failing for an unrelated reason.
  - The 6502 backend silently lost MIR_SUB for the whole time.

The aggregate only names a handful of tier targets, so reachability has to
be computed transitively. This script does that and compares against a
recorded baseline, so adding a test target without wiring it in is caught
immediately rather than never.

Usage:
    check_test_wiring.py                     # report
    check_test_wiring.py --write-baseline F  # accept the current state
    check_test_wiring.py --explain NAME      # why is NAME unreachable?
"""

import argparse
import os
import re
import sys

MAKEFILE = os.environ.get("TESTS_MK", "mk/tests.mk")


def parse_rules(path):
    """Map target -> prerequisite string. Handles backslash continuations and
    recipe lines (which start with a tab and are not prerequisites)."""
    rules, cur, prev_cont = {}, None, False
    with open(path, encoding="utf-8", errors="replace") as fh:
        for line in fh:
            line = line.rstrip("\n")
            m = re.match(r"^([A-Za-z_0-9.\-]+):(.*)$", line)
            if m:
                cur = m.group(1)
                rules.setdefault(cur, "")
                rules[cur] += " " + m.group(2)
                prev_cont = m.group(2).rstrip().endswith("\\")
                continue
            if cur is None:
                continue
            if line.startswith("\t"):
                # recipe line: not a prerequisite
                prev_cont = False
                continue
            stripped = line.strip()
            if prev_cont and stripped:
                rules[cur] += " " + stripped.rstrip("\\")
                prev_cont = stripped.endswith("\\")
            elif stripped and not stripped.startswith("#"):
                cur = None
    return rules


def reachable(rules, root="test"):
    seen, stack = set(), [root]
    while stack:
        t = stack.pop()
        if t in seen:
            continue
        seen.add(t)
        for p in re.split(r"\s+", rules.get(t, "")):
            p = p.replace("\\", "")
            if p and p not in seen and p in rules:
                stack.append(p)
    return seen


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--makefile", default=MAKEFILE)
    ap.add_argument("--baseline")
    ap.add_argument("--write-baseline")
    ap.add_argument("--explain")
    ap.add_argument("--quiet", action="store_true")
    args = ap.parse_args()

    if not os.path.exists(args.makefile):
        print(f"check_test_wiring: {args.makefile} not found", file=sys.stderr)
        return 0

    rules = parse_rules(args.makefile)
    reach = reachable(rules)
    runnable = {t for t in rules if t.startswith("test_")}
    orphans = sorted(runnable - reach)

    if args.explain:
        target = args.explain
        if target in reach:
            print(f"{target}: REACHABLE from `make test`")
        else:
            print(f"{target}: ORPHAN -- no prerequisite chain from `test` reaches it")
        print("\nDirect prerequisites:")
        for p in re.split(r"\s+", rules.get(target, "")):
            p = p.replace("\\", "")
            if p:
                mark = "  " if p in reach else "* "
                print(f"  {mark}{p}{'  (reachable)' if p in reach else ''}")
        return 0

    if args.write_baseline:
        with open(args.write_baseline, "w", encoding="utf-8") as fh:
            fh.write("# test_* targets unreachable from `make test`.\n")
            fh.write("# Shrink this list by wiring targets into the aggregate.\n")
            for o in orphans:
                fh.write(o + "\n")
        print(f"check_test_wiring: wrote {len(orphans)} orphans to {args.write_baseline}")
        return 0

    if args.baseline and os.path.exists(args.baseline):
        with open(args.baseline, encoding="utf-8") as fh:
            known = {ln.strip() for ln in fh
                     if ln.strip() and not ln.startswith("#")}
        new = sorted(set(orphans) - known)
        if new:
            print("check_test_wiring: FAIL -- new orphan test targets "
                  "(defined but nothing runs them):", file=sys.stderr)
            for o in new:
                print(f"  {o}", file=sys.stderr)
            print("\nWire them into the `test` aggregate or an intermediate "
                  "tier, or accept with --write-baseline.", file=sys.stderr)
            return 1
        if not args.quiet:
            print(f"check_test_wiring: OK -- {len(orphans)} known orphans, "
                  f"no new ones")
        return 0

    print(f"runnable test_* targets : {len(runnable)}")
    print(f"reachable from `test`  : {len(runnable & reach)}")
    print(f"ORPHANS                 : {len(orphans)}")
    for o in orphans[:25]:
        print(f"  {o}")
    if len(orphans) > 25:
        print(f"  ... and {len(orphans) - 25} more")
    return 0


if __name__ == "__main__":
    sys.exit(main())
