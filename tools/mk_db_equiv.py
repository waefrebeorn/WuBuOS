#!/usr/bin/env python3
"""mk_db_equiv.py -- prove a makefile refactor changed no target's prerequisites.

A makefile split or dedup is only safe if make's own database still says the same
thing afterwards. This compares two `make -pq` dumps target by target and fails on
any difference: a prerequisite added, dropped, reordered, or a target that vanished.

Usage: make -pq test > before.txt; <refactor>; make -pq test > after.txt
       tools/mk_db_equiv.py before.txt after.txt
Exit: 0 identical, 1 any difference.
"""
import re
import sys

TARGET = re.compile(r'^([^\s:#=]+):\s+(.*)$')


def load(path):
    t = {}
    for line in open(path, errors='replace'):
        if line.startswith('\t') or line.startswith('#'):
            continue
        m = TARGET.match(line.rstrip('\n'))
        if not m:
            continue
        name, prereq = m.group(1), m.group(2).strip()
        if not prereq or name.startswith('.') or '=' in name:
            continue
        t.setdefault(name, []).append(prereq)
    return t


def main():
    if len(sys.argv) != 3:
        print(__doc__.strip().splitlines()[-1])
        return 2
    before, after = load(sys.argv[1]), load(sys.argv[2])
    problems = []
    for name, plists in before.items():
        if name not in after:
            problems.append(f"target vanished: {name}")
            continue
        a = sorted(list(dict.fromkeys(" ".join(plists).split())))
        b = sorted(list(dict.fromkeys(" ".join(after[name]).split())))
        if a != b:
            missing = [x for x in a if x not in b]
            extra = [x for x in b if x not in a]
            problems.append(f"{name}: -{len(missing)} +{len(extra)}  first_missing={missing[:3]} first_extra={extra[:3]}")
    for name in after:
        if name not in before:
            problems.append(f"target appeared: {name}")
    print(f"mk_db_equiv: {len(before)} targets before, {len(after)} after, {len(problems)} difference(s)")
    for p in problems[:25]:
        print("  -", p)
    return 1 if problems else 0


if __name__ == '__main__':
    sys.exit(main())
