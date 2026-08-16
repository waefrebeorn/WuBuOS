#!/usr/bin/env python3
"""
extract_tests.py — Extract and organize all compiler test suites into WuBu gauntlet format.

Scans ~/vault/test-suites/ for downloaded test suites, categorizes each test,
and generates:
  - tests/gauntlet_integer.c    (integer arithmetic tests)
  - tests/gauntlet_control.c    (control flow tests)
  - tests/gauntlet_bitwise.c    (bitwise operation tests)
  - tests/gauntlet_comparison.c (comparison tests)
  - tests/gauntlet_memory.c     (memory/struct/array tests)
  - tests/gauntlet_float.c      (floating point tests)
  - tests/gauntlet_string.c     (string/libc tests)
  - tests/gauntlet_stress.c     (edge cases, torture tests)
  - tests/gauntlet_manifest.h   (registry of all tests)
"""

import os
import re
import json
from pathlib import Path

VAULT = Path.home() / "vault" / "test-suites"
OUTPUT = Path.home() / "wubuos" / "src" / "compiler" / "test_gauntlet" / "suites"

# Test categories (match wubu_test_gauntlet.h)
CATEGORIES = {
    "integer":   {"patterns": [r"\bint\b", r"\bchar\b", r"\breturn\s+\d"], "exclude": [r"float", r"double"]},
    "control":   {"patterns": [r"\bif\b", r"\bfor\b", r"\bwhile\b", r"\bswitch\b", r"\bdo\s*\{"]},
    "bitwise":   {"patterns": [r"&", r"\|", r"\^", r"~", r"<<", r">>"]},
    "comparison":{"patterns": [r"==", r"!=", r"<=", r">=", r"<", r">"]},
    "memory":    {"patterns": [r"\bstruct\b", r"\barray\b", r"\bmalloc\b", r"\bfree\b", r"\b&[a-z]"]},
    "float":     {"patterns": [r"\bfloat\b", r"\bdouble\b", r"\bdouble\b", r"\bmath\.h\b"]},
    "string":    {"patterns": [r"\bstring\.h\b", r"\bprintf\b", r"\bsprintf\b", r"\bstrlen\b", r"\bstrcpy\b"]},
    "stress":    {"patterns": [r"\btorture\b", r"\boverflow\b", r"\bboundary\b", r"\bextreme\b"]},
}

def categorize(source: str) -> list:
    """Return list of categories this test belongs to."""
    cats = []
    for cat, info in CATEGORIES.items():
        matches = sum(1 for p in info["patterns"] if re.search(p, source))
        if matches >= 2:
            excludes = info.get("exclude", [])
            if not any(re.search(e, source) for e in excludes):
                cats.append(cat)
    return cats if cats else ["integer"]  # default

def extract_expected(source: str) -> int:
    """Try to extract expected return value from test source."""
    # Look for "return <number>;" at end of main
    m = re.search(r'return\s+(-?\d+)\s*;', source)
    if m:
        return int(m.group(1))
    return 0

def scan_suite(suite_path: Path, suite_name: str) -> dict:
    """Scan a test suite directory and categorize all .c files."""
    tests = {cat: [] for cat in CATEGORIES}
    tests["uncategorized"] = []

    for c_file in suite_path.rglob("*.c"):
        try:
            source = c_file.read_text(errors='ignore')
        except:
            continue

        # Skip files that are too large or clearly not tests
        if len(source) > 50000:
            continue
        if '#include "config.h"' in source and 'raytracer' in str(c_file):
            continue  # Skip complex multi-file tests

        cats = categorize(source)
        expected = extract_expected(source)

        test_entry = {
            "file": str(c_file),
            "suite": suite_name,
            "name": c_file.stem,
            "expected": expected,
            "size": len(source),
            "categories": cats,
        }

        for cat in cats:
            tests[cat].append(test_entry)

    return tests

def main():
    OUTPUT.mkdir(parents=True, exist_ok=True)

    all_tests = {}

    # Scan each suite
    suites = {
        "gcc-torture": VAULT / "gcc" / "gcc" / "testsuite" / "gcc.c-torture" / "execute",
        "compcert": VAULT / "CompCert-small-tests",
        "stdtests": VAULT / "stdtests",
    }

    for name, path in suites.items():
        if path.exists():
            print(f"Scanning {name}...")
            result = scan_suite(path, name)
            for cat, tests in result.items():
                all_tests.setdefault(cat, []).extend(tests)
                print(f"  {cat}: {len(tests)} tests")
        else:
            print(f"  {name}: NOT FOUND at {path}")

    # Write manifest
    manifest = OUTPUT / "test_manifest.json"
    with open(manifest, "w") as f:
        # Convert to serializable format
        serializable = {}
        for cat, tests in all_tests.items():
            serializable[cat] = len(tests)
        json.dump(serializable, f, indent=2)

    print(f"\n=== Summary ===")
    total = 0
    for cat, cat_tests in sorted(all_tests.items()):
        count = len(cat_tests)
        print(f"  {cat:15s}: {count:5d} tests")
        total += count
    print(f"  {'TOTAL':15s}: {total:5d} tests")

    # Write per-category C arrays
    for cat, tests in all_tests.items():
        if not tests:
            continue
        out_file = OUTPUT / f"gauntlet_{cat}.c"
        with open(out_file, "w") as f:
            f.write(f"/* Auto-generated: {cat} tests from {len(tests)} sources */\n")
            f.write(f"/* Suites: gcc-torture, compcert, stdtests */\n\n")
            f.write('#include "wubu_test_gauntlet.h"\n\n')

            for i, t in enumerate(tests[:500]):  # Cap at 500 per category
                f.write(f"/* Test {i}: {t['name']} (from {t['suite']}) */\n")
                f.write(f"/* Expected: {t['expected']} */\n")
                f.write(f"/* Source: {t['file']} */\n\n")

        print(f"  Wrote {out_file.name}")

if __name__ == "__main__":
    main()
