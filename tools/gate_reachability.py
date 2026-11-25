#!/usr/bin/env python3
"""gate_reachability.py -- count the gates that `make test` never runs.

A test target that nothing depends on is not a test: it is a file that will rot
silently, and the tree has hundreds. This gate does not try to wire them all --
many are obsolete or unbuildable -- it makes the number VISIBLE and stops it
growing, which is what a ratchet is for.

Exit 0 when the orphan count is at or below the baseline, 1 when it grew.
Run from the repo root.
"""
import os
import re
import sys

MK = os.environ.get("WUBU_TESTS_MK", "mk/tests.mk")
BASELINE = "tools/gate_reachability.baseline"


def targets_and_deps(text):
    """target -> set of test_* targets it mentions as a prerequisite."""
    deps = {}
    for m in re.finditer(r"^([A-Za-z0-9_./-]+):(.*?)(?=^\S|\Z)", text, re.M | re.S):
        name, body = m.group(1), m.group(2)
        deps[name] = {d for d in re.findall(r"\b(test_[A-Za-z0-9_]+)\b", body) if d != name}
    return deps


def main():
    with open(MK, "r", encoding="utf-8", errors="replace") as f:
        text = f.read()
    deps = targets_and_deps(text)
    tests = {t for t in deps if t.startswith("test_")}
    if "test" not in deps:
        print("gate_reachability: no `test` target in %s" % MK)
        return 2

    reachable, stack = set(), ["test"]
    while stack:
        t = stack.pop()
        if t in reachable:
            continue
        reachable.add(t)
        stack.extend(deps.get(t, ()))

    orphans = sorted(tests - reachable)
    base = None
    if os.path.exists(BASELINE):
        try:
            with open(BASELINE) as f:
                base = int(f.read().split()[0])
        except (ValueError, IndexError):
            base = None

    print("gate_reachability: %d test targets, %d reachable from `test`, %d orphans"
          % (len(tests), len(tests & reachable), len(orphans)))
    for o in orphans[:15]:
        print("  orphan: %s" % o)
    if len(orphans) > 15:
        if os.environ.get("WUBU_REACH_ALL"):
            for o in orphans[15:]:
                print("  orphan: %s" % o)
        else:
            print("  ... and %d more (tools/gate_reachability.baseline)" % (len(orphans) - 15))

    # THE RATCHET STORES NAMES, NOT A COUNT. A count can only say "one more orphan
    # exists somewhere among 412"; it cannot say WHICH target stopped being reachable,
    # so the gate could not be acted on -- exactly the state it was left in when this
    # was still a bare number. A set diff names the target in one line.
    base_set = None
    if base is not None:
        with open(BASELINE) as f:
            lines = [ln.strip() for ln in f if ln.strip()]
        if lines and not lines[0].isdigit():
            base_set = set(lines)

    if base_set is None:
        with open(BASELINE, "w") as f:
            f.write("\n".join(sorted(orphans)) + "\n")
        print("gate_reachability: baseline written as a NAME SET (%d orphans)" % len(orphans))
        return 0

    appeared = sorted(orphans - base_set)
    fixed = sorted(base_set - orphans)
    if appeared:
        print("gate_reachability: FAIL -- %d NEW orphan(s); a target lost its path from "
              "`make test`:" % len(appeared))
        for o in appeared[:20]:
            print("  NEW ORPHAN: %s" % o)
        return 1
    if fixed:
        print("gate_reachability: %d target(s) became reachable -- remove them from %s: %s"
              % (len(fixed), BASELINE, ", ".join(fixed[:10])))
    return 0


if __name__ == "__main__":
    sys.exit(main())
