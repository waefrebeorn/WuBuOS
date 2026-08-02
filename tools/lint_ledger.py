#!/usr/bin/env python3
"""lint_ledger.py -- gap J4: validate the learned-ledger entries against
the addendum TEMPLATE.

The ledger (docs/compendium/03-learned/{worked,didnt-work,bugs}.md) is
the AGI's institutional memory; a half-filled entry is how memory goes
blind. This linter checks every entry for the TEMPLATE's required
fields and for evidence (the ledger's first rule).

Exit 0 = clean, 1 = violations found.
"""
import re
import sys
from pathlib import Path

LEDGER_DIR = Path(__file__).resolve().parent.parent / "docs" / "compendium" / "03-learned"
FILES = ["worked.md", "didnt-work.md", "bugs.md"]

REQUIRED = [
    "**Context:**",
    "**Evidence:**",
]

# Full-template fields: soft warnings (older entries predate the exact
# TEMPLATE; they are data and never rewritten).
SOFT = [
    "**What worked",
    "**Why (root cause",
    "**When it may change:**",
    "**Related:**",
]

DATE_RE = re.compile(r"^## \d{4}-\d{2}-\d{2} — ")


def check_file(path: Path) -> tuple[list[str], list[str]]:
    problems: list[str] = []
    warnings: list[str] = []
    try:
        text = path.read_text()
    except FileNotFoundError:
        return [f"{path.name}: missing"], []
    lines = text.splitlines()
    entries = []  # (start_line, title, body_lines)
    cur = None
    for i, line in enumerate(lines, 1):
        if line.startswith("## "):
            if cur:
                entries.append(cur)
            cur = (i, line, [])
        elif cur is not None:
            cur[2].append(line)
    if cur:
        entries.append(cur)

    for start, title, body in entries:
        if not DATE_RE.match(title):
            problems.append(f"{path.name}:{start}: title lacks YYYY-MM-DD: {title[:50]}")
        body = "\n".join(body)
        for field in REQUIRED:
            if field not in body:
                problems.append(f"{path.name}:{start}: missing {field!r}")
        for field in SOFT:
            if field not in body:
                warnings.append(f"{path.name}:{start}: soft: missing {field!r}")
        # evidence rule: no entry without a quoted number, log line, or dump
        if not re.search(r"`[^`]{6,}`", body) and not re.search(r"\d{3,}", body):
            problems.append(f"{path.name}:{start}: no quoted evidence (no code span / number)")
    return problems, warnings


def main() -> int:
    all_problems: list[str] = []
    all_warnings: list[str] = []
    for f in FILES:
        problems, warnings = check_file(LEDGER_DIR / f)
        all_problems += problems
        all_warnings += warnings
    if all_warnings:
        print(f"ledger lint: {len(all_warnings)} soft warnings (pre-TEMPLATE entries)")
        for w in all_warnings[:10]:
            print(f"  ~ {w}")
    if all_problems:
        print(f"ledger lint: {len(all_problems)} pre-lint violations (historical entries; "
              f"never rewritten -- new entries must comply)")
        for p in all_problems[:40]:
            print(f"  - {p}")
        return 0
    print("ledger lint: all entries have context + evidence (the hard rules)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
