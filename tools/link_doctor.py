#!/usr/bin/env python3
"""link_doctor.py -- resolve undefined references to the file that defines them.

Hand-adding one missing source per build is how a link list goes stale in the
first place. This asks the linker what it needs, finds the defining translation
unit in the tree, appends it to that target's recipe, and repeats until the
target links or a symbol has no definition anywhere.

usage: tools/link_doctor.py test_era_apps [--max-rounds 12] [--dry-run]
"""
import argparse
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
TESTS_MK = ROOT / "mk" / "tests.mk"


def run_make(target):
    p = subprocess.run(["make", target], cwd=ROOT, capture_output=True, text=True, timeout=300)
    return p.returncode, p.stdout + p.stderr


def undefined_symbols(log):
    syms = []
    for m in re.finditer(r"undefined reference to [`']([^']+)'", log):
        s = m.group(1)
        if s not in syms:
            syms.append(s)
    return syms


def find_definition(sym):
    """Return the .c file that defines sym at column 0, or None."""
    esc = re.escape(sym)
    pats = [
        "^[A-Za-z_][^;()]*" + esc + "[[:space:]]*\\(",          # function definition
        "^[[:space:]]*[A-Za-z_][A-Za-z0-9_ *]*" + esc + "[[:space:]]*=",  # variable
    ]
    for p in pats:
        r = subprocess.run(["grep", "-rEl", "-E", p, "--include=*.c", "src"],
                           cwd=ROOT, capture_output=True, text=True)
        files = [f for f in r.stdout.split() if f]
        if files:
            return files[0]
    return None


def add_to_recipe(target, source):
    """Append `source` to the first continuation line of target's link command."""
    lines = TESTS_MK.read_text().split("\n")
    start = None
    for i, l in enumerate(lines):
        if l.startswith(target + ":"):
            start = i
            break
    if start is None:
        return False, "target block not found"
    for j in range(start + 1, min(start + 12, len(lines))):
        if lines[j].startswith("\t") and lines[j].rstrip().endswith("\\"):
            if source in lines[j]:
                return False, "already present"
            lines[j] = lines[j].rstrip()[:-1] + f" {source} \\"
            TESTS_MK.write_text("\n".join(lines))
            return True, f"line {j+1}"
    return False, "no continuation line in recipe"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("target")
    ap.add_argument("--max-rounds", type=int, default=12)
    ap.add_argument("--dry-run", action="store_true")
    a = ap.parse_args()

    for rnd in range(1, a.max_rounds + 1):
        rc, log = run_make(a.target)
        if rc == 0:
            print(f"{a.target}: LINKS AND PASSES (round {rnd - 1})")
            return 0
        syms = undefined_symbols(log)
        if not syms:
            print(f"{a.target}: fails for a non-link reason:")
            for line in log.strip().splitlines()[-5:]:
                print("   ", line[:140])
            return 1
        print(f"{a.target}: round {rnd}, {len(syms)} undefined: {', '.join(syms[:6])}")
        added = 0
        for s in syms[:8]:
            f = find_definition(s)
            if not f:
                print(f"   no definition for `{s}` anywhere in src/ -- needs code, not a link fix")
                continue
            if a.dry_run:
                print(f"   would add {f} for `{s}`")
                added += 1
                continue
            ok, why = add_to_recipe(a.target, f"$(ROOT)/{f}" if False else f)
            if ok:
                print(f"   + {f} for `{s}` ({why})")
                added += 1
            else:
                print(f"   {f} for `{s}`: {why}")
        if added == 0:
            print(f"{a.target}: nothing further can be resolved by linking")
            return 1
    print(f"{a.target}: still failing after {a.max_rounds} rounds")
    return 1


if __name__ == "__main__":
    sys.exit(main())
