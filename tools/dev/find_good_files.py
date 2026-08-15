#!/usr/bin/env python3
"""
Regenerate ALL test_hw_<mod> targets in mk/tests.mk.

Approach: For each target, compile ONLY the selftest + module .c +
a minimal core set. If wubu_hw_detect.c is included and needs
probe symbols, include ALL non-problematic wubu_*.c modules.

Strategy: discover which .c files compile cleanly in a standalone
-D_GNU_SOURCE -I$(KERNEL) context, and use that list as the kernel
dependency chain.
"""
import os, re, subprocess, tempfile

KERNEL = '/home/wubu/wubunos/src/kernel'
TESTS_MK = '/home/wubu/wubunos/mk/tests.mk'

# First, discover all .c files and which compile cleanly
all_c_files = []
for f in sorted(os.listdir(KERNEL)):
    if f.endswith('.c') and '_selftest' not in f and not f.endswith('_test.c'):
        all_c_files.append(f)

# Try compiling each file to find problematic ones
good_files = []
bad_files = []
for f in all_c_files:
    r = subprocess.run(
        ['gcc', '-c', '-O0', '-g', '-std=c11', '-D_GNU_SOURCE',
         '-I' + KERNEL, '-o', '/dev/null', os.path.join(KERNEL, f)],
        capture_output=True, text=True, timeout=10
    )
    if r.returncode == 0:
        good_files.append(f)
    else:
        bad_files.append(f)

print(f"Good: {len(good_files)}, Bad: {len(bad_files)}")
for b in bad_files:
    err = subprocess.run(
        ['gcc', '-c', '-O0', '-g', '-std=c11', '-D_GNU_SOURCE', '-I' + KERNEL, '-o', '/dev/null', os.path.join(KERNEL, b)],
        capture_output=True, text=True, timeout=10
    )
    errs = err.stderr.split('\n')
    first_err = [e for e in errs if 'error:' in e][:1]
    print(f"  {b}: {first_err[0][:100] if first_err else 'unknown'}")
