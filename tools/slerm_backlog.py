#!/usr/bin/env python3
"""slerm_backlog.py -- the policy "everything vendored is a goal" as a gate.

RULE
    Every mechanism in third_party/tiny/upstreams.json must have exactly one slerm
    goal in docs/GOAL-SLERM.json, and a goal may only be DONE when its gate target
    exists in mk/tests.mk. A vendored file is a SCAFFOLD and a test oracle; it never
    ships in the product.

WHY
    Without this, "all will be slermed" is a promise. With it, taking a library
    creates a tracked obligation in the same commit, and closing one is a checkable
    event rather than a sentence.

CHECKS
    1. one goal per upstream entry (id in `replaces`), no orphans either way
    2. every goal has a module path, a criterion >= 25 chars, and a gate
    3. status in {OPEN, SCAFFOLD, DONE}; DONE requires `test_<stem>` in mk/tests.mk
    4. no duplicate n, and `next_n` is greater than every n

Usage: tools/slerm_backlog.py [--list]
Exit: 0 clean, 1 violation.
"""
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
UP = ROOT / "third_party/tiny/upstreams.json"
GO = ROOT / "docs/GOAL-SLERM.json"
TESTS = ROOT / "mk/tests.mk"


def main():
    ups = json.loads(UP.read_text())["entries"] if UP.exists() else []
    doc = json.loads(GO.read_text()) if GO.exists() else {"goals": [], "next_n": 1}
    goals = doc.get("goals", [])
    problems = []

    replaced = {}
    for g in goals:
        r = g.get("replaces")
        if not r:
            problems.append(f"{g.get('id')}: no `replaces` field")
            continue
        if r in replaced:
            problems.append(f"{r}: two slerm goals claim one gap ({replaced[r]}, {g.get('id')})")
        replaced[r] = g.get("id")

    for e in ups:
        if e["id"] not in replaced:
            problems.append(f"{e['id']}: vendored with no slerm goal (policy violation)")

    tests_text = TESTS.read_text() if TESTS.exists() else ""
    seen_n = set()
    for g in goals:
        n = g.get("n")
        if n in seen_n:
            problems.append(f"duplicate n={n}")
        seen_n.add(n)
        if not g.get("module") or "/" not in g.get("module", ""):
            problems.append(f"{g.get('id')}: module must be a path")
        if len(g.get("criterion", "")) < 25:
            problems.append(f"{g.get('id')}: criterion is not decidable (<25 chars)")
        if not g.get("gate"):
            problems.append(f"{g.get('id')}: no gate")
        st = g.get("status")
        if st not in ("OPEN", "SCAFFOLD", "DONE"):
            problems.append(f"{g.get('id')}: status '{st}' is not OPEN/SCAFFOLD/DONE")
        if st == "DONE":
            stem = Path(g["module"]).stem
            if f"test_{stem}:" not in tests_text:
                problems.append(f"{g.get('id')}: marked DONE but mk/tests.mk has no test_{stem} target")
            elif not (ROOT / g["module"]).exists():
                problems.append(f"{g.get('id')}: marked DONE but {g['module']} does not exist")

    if doc.get("next_n", 0) <= max((g.get("n", 0) for g in goals), default=0):
        problems.append("next_n must be greater than every n")

    if "--list" in sys.argv:
        for g in goals:
            print(f"{g['n']:5d} {g['status']:9s} {g['module']:34s} {g['title'][:70]}")
        print()

    counts = {}
    for g in goals:
        counts[g.get("status")] = counts.get(g.get("status"), 0) + 1
    summary = ", ".join(f"{k}={v}" for k, v in sorted(counts.items()))
    if problems:
        print("VIOLATIONS:")
        for p in problems:
            print("  -", p)
        print(f"slerm_backlog: {len(problems)} violation(s)")
        return 1
    print(f"slerm_backlog: {len(goals)} goals, {len(ups)} vendored mechanisms, all owned ({summary})")
    return 0


if __name__ == "__main__":
    sys.exit(main())
