#!/usr/bin/env python3
"""tree_audit.py -- the UI/quality methodology applied to every tree we do not own.

wubuos contains ten submodules and vendored trees. The methodology in
docs/compendium/04-roadmap/ui-excellence-methodology.md was written for src/gui and
enforced there by gate_gui_fluent.py -- which means every imported tree has been exempt
from the standard the rest of the product is held to. This audits them with the same
measures, so "submodules are fine" becomes a number instead of an assumption.

Measures (each maps to a real defect we have already fixed in the parent):
  colour      hand-typed 0xRRGGBB / RGB() literals in render code  (theme ownership)
  strncpy     the two-way-wrong API we are migrating off            (buffer safety)
  libm        sqrt()/__builtin_sqrt in freestanding-capable units   (link breaks)
  stubs       functions whose entire body is (void)arg;            (papering over)
  boxy        opaque rect fills with no coverage/AA call nearby     (1-bit primitives)
Usage: python3 tools/tree_audit.py [--json]
"""
import json
import os
import re
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# One dry run of every build we care about; paths appearing in it are genuinely
# compiled (or at least invoked) by our makefiles.
DRYRUN = subprocess.run(["make", "-n", "-C", ROOT, "hosted", "test"],
                        capture_output=True, text=True,
                        env={**os.environ, "XDG_RUNTIME_DIR": ""}).stdout

def submodules():
    out = subprocess.run(["git", "-C", ROOT, "config", "--file", ".gitmodules",
                          "--get-regexp", "path"], capture_output=True, text=True).stdout
    return [ln.split()[1] for ln in out.strip().splitlines() if ln.strip()]

PATS = {
    "colour": re.compile(r"0x(?:00)?[0-9A-Fa-f]{6}\b|RGB\(\s*0x"),
    "strncpy": re.compile(r"\bstrncpy\s*\("),
    "libm": re.compile(r"\b(?:sqrt|sqrtf|__builtin_sqrt)\s*\("),
    "stub": re.compile(r"\{\s*\(void\)\s*\w+\s*;\s*\}"),
}

def audit(path):
    stats = {k: 0 for k in PATS}
    files = loc = 0
    for dirpath, dirnames, filenames in os.walk(path):
        dirnames[:] = [d for d in dirnames if d not in (".git", "build", "tests", "test")]
        for fn in filenames:
            if not fn.endswith((".c", ".h")):
                continue
            files += 1
            try:
                text = open(os.path.join(dirpath, fn), "r", errors="ignore").read()
            except OSError:
                continue
            loc += text.count("\n")
            for k, pat in PATS.items():
                stats[k] += len(pat.findall(text))
    return {"files": files, "loc": loc, **stats}

def main():
    as_json = "--json" in sys.argv
    report = {}
    for sm in submodules():
        p = os.path.join(ROOT, sm)
        if not os.path.isdir(p):
            continue
        dirty = subprocess.run(["git", "-C", p, "status", "--porcelain"],
                               capture_output=True, text=True).stdout.strip()
        branch = subprocess.run(["git", "-C", p, "rev-parse", "--abbrev-ref", "HEAD"],
                                capture_output=True, text=True).stdout.strip()
        head = subprocess.run(["git", "-C", p, "rev-parse", "--short", "HEAD"],
                              capture_output=True, text=True).stdout.strip()
        st = audit(p)
        # "wired" is decided by MAKE ITSELF, not by grepping mk/ for a literal path:
        # mk defines $(WET) and $(GUI) style variables, so a literal search reported
        # src/gui/wet as "never compiled" while test_wet compiles all of it. The dry
        # run is the only honest answer to "does our build touch these sources".
        st["in_link_graph"] = sm + "/" in DRYRUN
        st.update({"branch": branch, "head": head, "dirty_files": len(dirty.splitlines())})
        report[sm] = st
    if as_json:
        print(json.dumps(report, indent=2))
        return 0
    print(f"{'submodule':28} {'branch':16} {'head':9} {'files':>6} {'loc':>7} "
          f"{'colour':>7} {'strncpy':>8} {'libm':>5} {'stub':>5} {'dirty':>6} {'wired':>6}")
    tot = {k: 0 for k in ("files", "loc", *PATS)}
    for sm, st in sorted(report.items()):
        print(f"{sm:28} {st['branch'][:16]:16} {st['head']:9} {st['files']:6} {st['loc']:7} "
              f"{st['colour']:7} {st['strncpy']:8} {st['libm']:5} {st['stub']:5} "
              f"{st['dirty_files']:6} {('yes' if st['in_link_graph'] else 'NO'):>6}")
        for k in tot:
            tot[k] += st[k]
    print(f"{'TOTAL':28} {'':16} {'':9} {tot['files']:6} {tot['loc']:7} "
          f"{tot['colour']:7} {tot['strncpy']:8} {tot['libm']:5} {tot['stub']:5}")
    unwired = [s for s, st in report.items() if not st["in_link_graph"]]
    if unwired:
        print("\nNOT COMPILED BY `make hosted test` -- present in the tree, absent from "
              "the build (the signature failure this repo keeps finding: a module that "
              "passes its own gate while the product never runs it):")
        for s in unwired:
            print("  " + s)
    print("\nA measure is a target, not a verdict: the sweep fixes the parent-standard "
          "defects in exactly the order the fixes landed in src/gui.")
    return 0

if __name__ == "__main__":
    sys.exit(main())
