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
        # `test` depends on `test-tiers`, NOT on a test_* name. Matching only
        # [A-Za-z0-9_]+ silently dropped that edge, so the walk from `test` stopped at its own
        # root and every one of 727 targets was reported as an orphan. A reachability gate that
        # cannot see its own root passes by declaring everything unreachable -- and then rewrote
        # its baseline to agree with itself.
        deps[name] = {d for d in re.findall(r"\b((?:test|check)[A-Za-z0-9_-]*)\b", body) if d != name}
    return deps


def main():
    with open(MK, "r", encoding="utf-8", errors="replace") as f:
        text = f.read()
    deps = targets_and_deps(text)
    tests = {t for t in deps if t.startswith("test_")}
    if "test" not in deps:
        print("gate_reachability: no `test` target in %s" % MK)
        return 2

    # TWO roots: the fast suite and the release suite. A gate that is too slow or too invasive
    # for `make test` still has to be reachable from something, or it is dead code with a
    # passing-looking recipe. test_default_crypto_negative_control injects a regression into
    # tracked source, so it can never run beside 24 parallel compiles.
    reachable, stack = set(), [r for r in ("test", "test_release") if r in deps]
    while stack:
        t = stack.pop()
        if t in reachable:
            continue
        reachable.add(t)
        stack.extend(deps.get(t, ()))

    orphans = sorted(tests - reachable)
    # Read the baseline ONCE, in either shape. The name set used to be consulted only when
    # line 1 was a digit, which made the migration branch unreachable: after the first rewrite
    # the file held names, `int()` failed, base was None, and the gate rewrote the file and
    # reported success forever. A gate that can only ever rewrite itself is not a gate.
    base_set = None
    if os.path.exists(BASELINE):
        with open(BASELINE) as f:
            lines = [ln.strip() for ln in f if ln.strip()]
        if lines and not lines[0].isdigit():
            base_set = set(lines)

    if base_set is None:
        with open(BASELINE, "w") as f:
            f.write("\n".join(sorted(orphans)) + "\n")
        print("gate_reachability: baseline written as a NAME SET (%d orphans)" % len(orphans))
        # Writing the bar and clearing it in the same run proves nothing: whatever orphans
        # exist RIGHT NOW become the new normal by definition. Fail once, so a human reads the
        # list and commits it deliberately. The next run compares against the committed file.
        print(">>> RE-RUN after reviewing and committing %s <<<" % BASELINE)
        return 1

    orphan_set = set(orphans)
    appeared = sorted(orphan_set - base_set)
    fixed = sorted(base_set - orphan_set)
    if appeared:
        print("gate_reachability: FAIL -- %d NEW orphan(s); a target lost its path from "
              "`make test`:" % len(appeared))
        for o in appeared[:20]:
            print("  NEW ORPHAN: %s" % o)
        return 1
    if fixed:
        print("gate_reachability: %d target(s) became reachable -- remove them from %s: %s"
              % (len(fixed), BASELINE, ", ".join(fixed[:10])))
    if not fixed:
        print("gate_reachability: PASS -- %d reachable, %d known orphans, no new ones"
              % (len(reachable), len(orphans)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
