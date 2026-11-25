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
        print("  ... and %d more (tools/gate_reachability.baseline)" % (len(orphans) - 15))

    if base is None:
        with open(BASELINE, "w") as f:
            f.write("%d\n" % len(orphans))
        print("gate_reachability: baseline written (%d)" % len(orphans))
        return 0
    if len(orphans) > base:
        print("gate_reachability: FAIL -- %d orphans, baseline was %d. A new gate was "
              "added without wiring it into `make test`, or a group lost a member."
              % (len(orphans), base))
        return 1
    if len(orphans) < base:
        print("gate_reachability: %d fewer orphans than the baseline -- lower the ratchet in "
              "%s to %d" % (base - len(orphans), BASELINE, len(orphans)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
