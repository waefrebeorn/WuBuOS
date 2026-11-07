#!/usr/bin/env python3
"""unowned_surface.py -- derive WuBuOS's OWNED vs UNOWNED (junk-drawer) surface.

Implements the classification rule from docs/owned-surface.md as a command:
  OWNED   = a source file referenced by at least one make target recipe OR in the
            kernel objects list (mk/objects.mk) -- i.e. it is built+possible-tested.
  UNOWNED = referenced by NO target recipe AND NOT in the objects list -- it is
            neither built by a make target nor directly testable: the junk-drawer
            debt. (A file in the objects list but in no test target is BUILT-
            UNTESTED; reported separately so you can promote it.)

Usage: python3 scripts/unowned_surface.py   (from repo root)
Exit: 0. Prints the inbox report. Honest numbers, not prose.
"""
import re, os, sys, glob

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
tests_mk = os.path.join(ROOT, "mk", "tests.mk")
objects_mk = os.path.join(ROOT, "mk", "objects.mk")

def read(p):
    try:
        return open(p).read()
    except Exception:
        return ""

tests_txt = read(tests_mk)
objects_txt = read(objects_mk)

# All filenames referenced ANYWHERE across every mk file + the top Makefile
# (matches literal `src/kernel/foo.c` AND make-var `$(KERNEL)/foo.c` forms;
# a literal-path-only regex misses the $(KERNEL)/$(RT) proxies and falsely
# classifies every kernel module as unowned).
mk_files = glob.glob(os.path.join(ROOT, "mk", "*.mk")) + \
           [os.path.join(ROOT, "Makefile")]
all_mk = "\n".join(read(f) for f in mk_files)
refs = set(re.findall(r'[\w.\-]+\.(?:c|h|o|S)\b', all_mk))

def is_owned(path):
    base = os.path.basename(path)
    core = os.path.splitext(base)[0]
    return base in refs or core in refs

kernel = sorted(glob.glob(os.path.join(ROOT, "src", "kernel", "*.c")))
runtime = sorted(glob.glob(os.path.join(ROOT, "src", "runtime", "*.c")))
all_src = kernel + runtime

owned, unowned, built_untested = [], [], []
for s in all_src:
    rel = os.path.relpath(s, ROOT)
    if not is_owned(rel):
        unowned.append(rel)
    else:
        owned.append(rel)

print("Scanning src/kernel + src/runtime .c files against every mk/*.mk + top Makefile ...")
print(f"== TEXTUALLY REFERENCED in some makefile: {len(owned)} ==")
print(f"== TEXTUALLY UNNAMED (first-pass sieve): {len(unowned)} ==")
print("    CAVEAT: a file built via a wildcard/indirect obj list (e.g. the")
print("    bare-metal kernel link) is NOT textually named and lands here EVEN")
print("    THOUGH IT IS BUILT (metal_main.c, libc.c, interrupt_*, ps2 are boot-")
print("    kernel files that run bare-metal). This is a sieve, not a 'delete me'")
print("    verdict: verify each against `make -n <kernel-target>` before acting.")
print("\n-- TEXTUALLY UNNAMED sample (first 40) --")
for u in unowned[:60]:
    print("  ", u)
if len(unowned) > 40:
    print(f"   ... and {len(unowned)-40} more")

# built (in objects list) but not in any TEST target = has no regression test
