#!/usr/bin/env python3
"""hardcode_ratchet.py -- freeze the hardcoding, so the refactor can only shrink it.

WHY
    "Do not hardcode anything" is a direction, not a measurement. The tree has
    three objective hardcoding smells and each one has a count today:

      1. DESK_ICON_*  -- the icon enum doubles as an application ID, so adding an
         app means editing an enum in wm.h, an index in apps.c, and a switch in
         startmenu.c.  References OUTSIDE src/apps are the coupling.
      2. raw 0xRRGGBB colour literals in src/gui -- a colour that is not in the
         theme cannot change with the theme.
      3. bare 2-3 digit literals in the window/shell sizing code -- geometry that
         is not a token cannot be scaled by DPI or by the density setting.

    A direction with no count is a direction nobody can tell is being followed.

WHAT IT DOES
    Counts the three smells per file, compares against tools/hardcode_baseline.json.
    FAILS if any file's count went UP (a new hardcoded value), and PRINTS every
    file that went DOWN so the baseline can be lowered in the same commit that
    earns it. A count that drops is never automatically accepted -- the baseline
    is a ratchet the author tightens, so a delete cannot silently mask an add.

    `--write` rewrites the baseline (only for a commit that lowers it).

Usage: tools/hardcode_ratchet.py [--write] [--table]
Exit: 0 no increase, 1 an increase.
"""
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
BASE = ROOT / "tools/hardcode_baseline.json"

# (name, root dir, include glob, pattern, eligibility)
SMELLS = [
    ("icon_enum", "src", "*.c", r"\bDESK_ICON_[A-Z_]+\b",
     lambda p: not str(p).startswith("src/apps/")),
    ("colour", "src/gui", "*.c", r"\b0x[0-9a-fA-F]{6}\b",
     lambda p: not str(p).endswith("_test.c")),
    ("geometry", "src/gui/shell", "*.c", r"(?<![\w.])(1[0-9]{2}|[2-9][0-9])(?![\w.])",
     lambda p: True),
]


def count(root: Path):
    out = {}
    for name, sub, glob, pat, ok in SMELLS:
        d = ROOT / sub
        if not d.exists():
            continue
        rx = re.compile(pat)
        for f in sorted(d.rglob(glob)):
            rel = str(f.relative_to(ROOT))
            if not ok(rel):
                continue
            text = f.read_text(errors="ignore")
            # strip block/line comments so prose about a value is not counted
            text = re.sub(r"/\*.*?\*/", "", text, flags=re.S)
            text = re.sub(r"//[^\n]*", "", text)
            n = len(rx.findall(text))
            if n:
                out.setdefault(rel, {})[name] = n
    return out


def main():
    cur = count(ROOT)
    prev = json.loads(BASE.read_text()) if BASE.exists() else {}
    if "--write" in sys.argv:
        BASE.write_text(json.dumps(cur, indent=1, sort_keys=True) + "\n")
        total = sum(sum(v.values()) for v in cur.values())
        print(f"baseline written: {len(cur)} files, {total} hardcoded values")
        return 0

    increases, drops, total = [], [], 0
    for f, smells in cur.items():
        for s, n in smells.items():
            total += n
            was = prev.get(f, {}).get(s, 0)
            if n > was:
                increases.append(f"{f}: {s} {was} -> {n}")
            elif n < was:
                drops.append(f"{f}: {s} {was} -> {n}")
    for f, smells in prev.items():
        for s, n in smells.items():
            if f not in cur or s not in cur[f]:
                drops.append(f"{f}: {s} {n} -> 0")

    if "--table" in sys.argv:
        for f, smells in sorted(cur.items()):
            print(f"  {f:44s} " + " ".join(f"{k}={v}" for k, v in sorted(smells.items())))
        print(f"  TOTAL hardcoded values: {total}")

    for d in drops:
        print("LOWERED (tighten the baseline in this commit):", d)
    if increases:
        print("RATCHET VIOLATION -- new hardcoded values:")
        for i in increases:
            print("  -", i)
        return 1
    print(f"hardcode_ratchet: no increase ({total} values in {len(cur)} files; baseline {len(prev)} files)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
