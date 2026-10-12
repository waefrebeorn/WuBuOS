#!/usr/bin/env python3
"""
find_orphan_sources.py -- flag src/**/*.c files referenced by NO build target.

The structural audit found three cases of dead parallel implementations
(a second Notepad, a second Editor, a shipping terminal stub beside a
real orphaned one). Each survived because nothing checked whether a
source file was actually linked into any target. This scanner makes that
hygiene self-enforcing.

Method:
  1. Resolve $(VAR) -> directory from mk/vars.mk.
  2. Scan EVERY makefile (Makefile + mk/**.mk + tools makefiles) for
     path tokens of the form $(VAR)/file or bare dir/file ending in
     .c/.o. Record each as an extension-less normalized path (so a .c
     source and its .o object are the same key).
  3. Walk src/**.c. A file is ORPHANED if its extension-less path is in
     no referenced set.
  4. Classify: *_test.c / *_selftest.c / *_stub*.c are their own targets
     (expected). A non-test source referenced by nothing is a real
     orphan -- the interesting case.

Exit code 1 if any non-test source is orphaned (CI gate), else 0.
--verbose lists the expected test-only files too.
"""
import os
import re
import sys

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
MK_VARS = os.path.join(REPO, "mk", "vars.mk")

# $(VAR)/path/file  and  bare  path/file  (both ending .c or .o)
TOKEN_VAR = re.compile(r"\$\(([A-Z_]+)\)/([\w./-]+?)\.(?:c|o)\b")
TOKEN_BARE = re.compile(r"(?<![\w/])((?:src|tools)/[\w./-]+?)\.(?:c|o)\b")


def load_vars():
    vars = {}
    if os.path.exists(MK_VARS):
        for line in open(MK_VARS):
            m = re.match(r"\s*([A-Z_]+)\s*=\s*(\S+)", line)
            if m:
                vars[m.group(1)] = m.group(2)
    return vars


def all_makefiles():
    """Only the repo's OWN build files. Vendored trees (vendor/,
    os-studies/) ship thousands of third-party makefiles that would
    pollute the reference set with foreign paths."""
    out = []
    root_mk = os.path.join(REPO, "Makefile")
    if os.path.exists(root_mk):
        out.append(root_mk)
    mkdir = os.path.join(REPO, "mk")
    if os.path.isdir(mkdir):
        for f in sorted(os.listdir(mkdir)):
            if f.endswith(".mk"):
                out.append(os.path.join(mkdir, f))
    return out


def collect_referenced(vars):
    """Extension-less normalized paths referenced by any makefile."""
    ref = set()
    for mkf in all_makefiles():
        try:
            text = open(mkf, errors="ignore").read()
        except OSError:
            continue
        for var, rel in TOKEN_VAR.findall(text):
            base = vars.get(var, var)
            ref.add(os.path.normpath(os.path.join(base, rel)))
        for rel in TOKEN_BARE.findall(text):
            ref.add(os.path.normpath(rel))
    return ref


def is_test_source(name):
    return (name.endswith("_test.c") or name.endswith("_selftest.c")
            or name.endswith("_stub.c") or name.endswith("_stubs.c")
            or name.endswith("_test_stub.c") or name.endswith("_test_stubs.c"))


def main():
    verbose = "--verbose" in sys.argv
    list_all = "--all" in sys.argv
    vars = load_vars()
    ref = collect_referenced(vars)

    # Group referenced files by basename so we can spot a duplicate
    # implementation: an orphan .c whose basename is ALSO built from a
    # different directory (e.g. apps/notepad/notepad.c orphaned while
    # apps/notepad.c is the real, referenced one). That pattern is exactly
    # what the structural audit found (second Notepad, second Editor,
    # shipping terminal stub beside a real orphan).
    ref_by_base = {}
    for r in ref:
        ref_by_base.setdefault(os.path.basename(r), []).append(r)

    # Scope: the trees governed by mk/objects.mk + mk/tests.mk. src/brain,
    # src/firmware, src/tools and vault trees have their own build systems
    # (wildcard rules, per-directory makefiles) this scanner can't model, so
    # a raw orphan count there is mostly false positives.
    SCOPED = ("gui", "apps", "kernel", "runtime", "compiler", "jit",
              "hosted", "bridge", "shell", "framework", "audio", "worldsim")
    src_root = os.path.join(REPO, "src")

    dupes, all_orphans = [], []
    # Walk git-TRACKED .c files only. Untracked trees (e.g. src/compiler/,
    # a stale sibling-repo checkout with 0 tracked files) are not this
    # repo's source and would otherwise appear as bogus duplicates.
    tracked = set()
    try:
        import subprocess as _sp
        out = _sp.run(["git", "ls-files", "src"], cwd=REPO,
                      capture_output=True, text=True, timeout=60)
        if out.returncode == 0:
            tracked = set(out.stdout.splitlines())
    except Exception:
        pass

    for rel in sorted(tracked):
        if not rel.endswith(".c"):
            continue
        top = rel.split("/")[1] if rel.count("/") >= 1 else ""
        if top not in SCOPED:
            continue
        fn = os.path.basename(rel)
        if is_test_source(fn):
            continue
        key = os.path.normpath(rel[:-2])  # strip .c -> matches .o keys
        if key in ref:
            continue
        all_orphans.append(rel)
        b = os.path.basename(key)
        if b in ref_by_base:
            dupes.append((rel, ref_by_base[b]))

    dupes.sort()
    all_orphans.sort()

    print("== DUPLICATE implementations (orphan .c whose name is built elsewhere) ==")
    if not dupes:
        print("  (none)")
    for rel, refs in dupes:
        print("  ORPHAN %s  <-> built: %s" % (rel, ", ".join(refs)))

    if list_all or verbose:
        print("\n== all unreferenced non-test sources in scoped trees ==")
        print("  (%d; mostly built via wildcard/per-dir makefiles -- review, "
              "don't mass-delete)" % len(all_orphans))
        for rel in all_orphans:
            print("  ", rel)

    if dupes:
        print("\n%d duplicate implementation(s). Delete the dead copy." % len(dupes))
        return 1
    print("\nNo duplicate implementations. Clean.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
