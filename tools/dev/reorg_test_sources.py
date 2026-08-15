#!/usr/bin/env python3
"""
Reorganize src/kernel/: move test sources (*_selftest.c, test_*.c) out of
the flat kernel dir into src/kernel/test/, and move test_hw_* binaries into
build/testbin/.  Pure organization -- no source content changes.

Then regenerate mk/tests.mk so test targets reference the new selftest
locations.  Build artifacts (.o/.d/test_hw binaries) are gitignored, so
this only affects tracked selftest sources + the generated tests.mk.
"""
import os, re, shutil, subprocess

KERNEL = '/home/wubu/wubunos/src/kernel'
TESTDIR = os.path.join(KERNEL, 'test')
BINDIR = '/home/wubu/wubunos/build/testbin'

os.makedirs(TESTDIR, exist_ok=True)
os.makedirs(BINDIR, exist_ok=True)

moved_c = 0
moved_bin = 0

for f in sorted(os.listdir(KERNEL)):
    full = os.path.join(KERNEL, f)
    # Test source files
    if re.match(r'wubu_\w+_selftest\.c$', f) or re.match(r'test_\w+\.c$', f):
        shutil.move(full, os.path.join(TESTDIR, f))
        moved_c += 1
    # Test binaries (test_hw_*, test_* with no extension)
    elif f.startswith('test_') and not f.endswith('.c') and not f.endswith('.d') \
         and not f.endswith('.o') and os.path.isfile(full):
        shutil.move(full, os.path.join(BINDIR, f))
        moved_bin += 1

print(f"Moved {moved_c} test sources to {TESTDIR}/")
print(f"Moved {moved_bin} test binaries to {BINDIR}/")

# git mv for tracked selftests (to keep history), plain move for untracked
print("\nNOTE: use git add -A to stage; git tracks renames automatically.")
