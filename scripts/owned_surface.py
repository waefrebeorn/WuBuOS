#!/usr/bin/env python3
"""owned_surface.py -- derive the OWNED-vs-VENDORED module manifest straight
from the Makefile (docs/GAP_ROADMAP GAP-C1), not from prose.

A product source module is OWNED if its basename appears in the recipe of any
`test_<name>:` target in mk/tests.mk (i.e. it is compiled+linked into a test
binary and so exercised); otherwise it is VENDORED/unproven (only in the
kernel/driver build, or nowhere in a test recipe).

Usage: python3 scripts/owned_surface.py [--write docs/owned-surface.md]
Reads wubuos repo from CWD.
"""
import os, re, sys, subprocess, collections

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
TESTMK = os.path.join(ROOT, "mk", "tests.mk")

def test_recipes():
    """yield (target_name, recipe_text) for every test_<name>: in mk/tests.mk."""
    lines = open(TESTMK, encoding="utf-8", errors="replace").read().splitlines()
    cur = None
    buf = []
    def flush():
        nonlocal buf
        if cur: yield cur, "\n".join(buf)
        buf = []
    for ln in lines:
        m = re.match(r"^(test_[A-Za-z0-9_]+):", ln)
        if m:
            yield from flush()
            cur = m.group(1); buf = []
            continue
        if re.match(r"^[A-Za-z0-9_.%${}/-]+[*:]?:", ln):   # any other target header
            yield from flush(); cur = None; continue
        # accumulate the recipe + continuation lines (tab-indented or backslash)
        stripped = ln.strip()
        if (ln[:1] == "\t" or ln.rstrip()[-1:] == "\\") and stripped:
            buf.append(ln)
    yield from flush()

def owned_from_recipes():
    """basename -> list of test targets that compile it."""
    owned = collections.defaultdict(list)
    for name, body in test_recipes():
        for c in re.findall(r"\b([A-Za-z0-9_]+\.c)\b", body):
            if c not in owned[name]: owned[c].append(name)
    return owned

def product_modules():
    """all src/*.c product basenames (exclude *_test.c and test/ dirs)."""
    out = subprocess.run(["find", os.path.join(ROOT, "src"), "-name", "*.c"],
                         capture_output=True, text=True).stdout.splitlines()
    mods = {}
    for p in out:
        if "/test/" in "/"+p.replace("\\", "/") or "_test.c" in p: continue
        if p.endswith(".o") or p.endswith(".d"): continue
        base = os.path.basename(p)
        mods.setdefault(base, []).append(p)
    return mods

def main():
    owned = owned_from_recipes()
    mods = product_modules()
    # classify
    rows = []
    owned_set = set(owned)
    n_owned = n_vendor = 0
    for base in sorted(mods):
        if base in owned_set:
            n_owned += 1
            rows.append((base, "OWNED", ";".join(owned[base][:4])))
        else:
            n_vendor += 1
            rows.append((base, "VENDORED", ""))

    total = n_owned + n_vendor
    doc = [
        "# WuBuOS owned-surface manifest (GAP-C1)",
        "",
        "Generated automatically by `scripts/owned_surface.py` from `mk/tests.mk`",
        "test-target recipes -- NOT prose. A product `.c` is **OWNED** if its",
        "basename is compiled by at least one `test_*:` target's recipe (so it is",
        "linked into a test binary and exercised); otherwise **VENDORED** (only in",
        "the kernel/driver build or nowhere in the test recipes).",
        "",
        f"- product source modules: {total}",
        f"- OWNED (appear in a test target recipe): {n_owned}",
        f"- VENDORED / unproven (no test-target recipe reference): {n_vendor}",
        "",
        "## OWNED modules (tested)",
        "",
    ]
    for base, st, tests in rows:
        if st == "OWNED":
            doc.append(f"- `{base}`  -- tests: {tests}")
    doc += ["", "## VENDORED / unproven modules (no test-target recipe)", ""]
    for base, st, tests in rows:
        if st == "VENDORED":
            doc.append(f"- `{base}`")
    txt = "\n".join(doc) + "\n"
    print(f"modules: {total}  OWNED: {n_owned}  VENDORED: {n_vendor} "
          f"({n_owned*100//total if total else 0}% owned)")
    # self-check: every OWNED basename must actually appear in a test recipe
    missing = sorted(b for b in rows if b[1] == "OWNED" and b[0] not in owned_set)
    print(f"self-check: OWNED basenames NOT in a test recipe = {len(missing)} "
          f"(must be 0)")
    if "--check" in sys.argv:
        out = os.path.join(ROOT, "docs", "owned-surface.md")
        current = open(out, encoding="utf-8").read() if os.path.exists(out) else ""
        stale = (current.rstrip() + "\n") != txt
        print("C2 enforcement:")
        print(f"  stale-manifest    = {stale}  (must be False; re-run "
              f"scripts/owned_surface.py --write)")
        print(f"  owned-no-recipe   = {len(missing)}  (must be 0)")
        if missing: print("  violations: " + ", ".join(missing))
        if missing or stale:
            print(">>> OWNED-SURFACE GATE FAILED (a module is claimed owned but has "
                  "no test recipe, and/or the manifest is stale) <<<")
            return 1
        print(">>> OWNED-SURFACE GATE PASS: every OWNED module has a test recipe; "
              "manifest current <<<")
        return 0
    if "--write" in sys.argv:
        out = os.path.join(ROOT, "docs", "owned-surface.md")
        os.makedirs(os.path.dirname(out), exist_ok=True)
        open(out, "w", encoding="utf-8").write(txt)
        print(f"wrote {out} ({len(txt)} bytes)")
    else:
        open("/tmp/owned-surface.md", "w", encoding="utf-8").write(txt)
        print("wrote /tmp/owned-surface.md (pass --write to land in docs/)")

if __name__ == "__main__":
    main()
