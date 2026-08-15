#!/usr/bin/env python3
"""
Iteratively find the working set of .c files for test_hw targets.
Strategy: start with all compilable .c files, try linking, find
undefined symbols, exclude the files that reference unresolvable
symbols, retry.
"""
import os, re, subprocess, tempfile

KERNEL = '/home/wubu/wubunos/src/kernel'
TESTS_MK = '/home/wubu/wubunos/mk/tests.mk'
CC = 'gcc'
CFLAGS = ['-O0', '-g', '-Wall', '-Wextra', '-std=c11', '-D_GNU_SOURCE', '-I' + KERNEL]

# Phase 1: Find all compilable .c files
all_c = []
for f in sorted(os.listdir(KERNEL)):
    if not f.endswith('.c'): continue
    if '_selftest' in f: continue
    if f.startswith('test_') or f.endswith('_test.c'): continue
    if f == 'cpio_extract.c': continue
    
    r = subprocess.run([CC] + CFLAGS + ['-c', '-o', '/dev/null', os.path.join(KERNEL, f)],
                       capture_output=True, text=True, timeout=15)
    if r.returncode == 0:
        all_c.append(f)
    else:
        errs = [l for l in r.stderr.split('\n') if 'error:' in l]
        print(f"  SKIP (compile): {f}: {errs[0][:100] if errs else '?'}")

print(f"Compilable: {len(all_c)}")

# Phase 2: Iteratively link - find which files cause unresolved symbols
# Build a test selftest + all files, see what's undefined
# Start with all_c, try linking, exclude failing files

# Find a selftest to use
selftests = [f for f in os.listdir(KERNEL) if re.match(r'wubu_\w+_selftest\.c$', f)]
test_selftest = 'wubu_accel_selftest.c'  # known good

# Try linking with all files + a selftest
def try_link(files, selftest):
    tmp = tempfile.mktemp(suffix='.o')
    cmd = [CC] + CFLAGS + [os.path.join(KERNEL, selftest)] + \
          [os.path.join(KERNEL, f) for f in files] + \
          ['-lm', '-lm', '-o', '/dev/null']
    r = subprocess.run(cmd, capture_output=True, text=True, timeout=30)
    errors = r.stderr
    if r.returncode == 0:
        return True, None
    else:
        return False, errors

# Iteratively remove files that cause undefined references
current = list(all_c)
removed = []
max_iter = 20
for iteration in range(max_iter):
    # Try linking with current set + test selftest
    ok, errors = try_link(current, test_selftest)
    if ok:
        print(f"Iteration {iteration}: SUCCESS with {len(current)} files")
        break
    
    # Find undefined symbol references
    undefined = set()
    for line in errors.split('\n'):
        m = re.search(r"undefined reference to `([^`]+)'", line)
        if m:
            undefined.add(m.group(1))
    
    # Find which .c file (not yet removed) references those symbols
    # and remove them
    to_remove = set()
    for fn in current:
        fpath = os.path.join(KERNEL, fn)
        src = open(fpath, errors='replace').read()
        for sym in list(undefined):
            # Check if this file defines the symbol (not just references it)
            if re.search(rf'\b{sym}\b\s*=', src) and 'define' not in src:
                # This file defines the symbol, keep it
                break
        else:
            # Check if this file references undefined symbols
            for sym in undefined:
                if re.search(rf'\b{sym}\b', src):
                    # This file references the undefined symbol
                    # Skip files that DEFINCE the symbol (like interrupt_apic.c defines g_lapic_base)
                    # We should keep definers!
                    pass
    
    # Simpler approach: find the .c file that has the undefined symbols, exclude it
    new_removed = set()
    for line in errors.split('\n'):
        m = re.match(r'.*?/(\w+\.c):.*undefined reference', line)
        if m:
            fn = m.group(1)
            if fn in current:
                new_removed.add(fn)
    
    if not new_removed:
        # Try .S files in errors
        for line in errors.split('\n'):
            m = re.match(r'.*?/(\w+\.S):.*undefined reference', line)
            if m:
                fn = m.group(1)
                if fn in current:
                    new_removed.add(fn)
    
    if not new_removed:
        print(f"Iteration {iteration}: Cannot find offending files. Errors:")
        for sym in sorted(undefined)[:5]:
            print(f"  undefined: {sym}")
        break
    
    for fn in new_removed:
        current.remove(fn)
        removed.append(fn)
    print(f"Iteration {iteration}: removed {len(new_removed)} files: {new_removed}")

print(f"\nFinal set: {len(current)} files ({len(removed)} removed)")

# Now check if test_hw_gt2xx can build with this set
# First, find the mod for gt2xx
mod_files = [f for f in os.listdir(KERNEL) if f == 'wubu_gt2xx.c']
print(f"gt2xx module: {mod_files}")

# Check if gt2xx is a test module
gt2xx_selftest = 'wubu_gt2xx_selftest.c'
if gt2xx_selftest in selftests:
    # Try linking gt2xx selftest + gt2xx.c + kernel files
    cc_files = [gt2xx_selftest, 'wubu_gt2xx.c'] + [f for f in current if f != 'wubu_gt2xx.c']
    cmd = [CC] + CFLAGS + [os.path.join(KERNEL, f) for f in cc_files] + ['-lm', '-lm', '-o', '/dev/null']
    r = subprocess.run(cmd, capture_output=True, text=True, timeout=60)
    if r.returncode == 0:
        print("gt2xx test build: SUCCESS")
    else:
        errs = [l for l in r.stderr.split('\n') if 'error:' in l or 'undefined' in l]
        print(f"gt2xx test build: FAILED ({len(errs)} errors)")
        for e in errs[:5]:
            print(f"  {e[:120]}")
