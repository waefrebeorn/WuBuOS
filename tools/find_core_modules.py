#!/usr/bin/env python3
"""
Regenerate ALL test_hw_<mod> targets in mk/tests.mk.

Uses iterative probing to find the maximal set of kernel .c files
that link cleanly with wubu_test_stubs.c. Then generates test_hw_*
targets for each module with a selftest.

Type A: selftest doesn't call wubu_hw_detect() — uses only core runtime
Type B: selftest calls wubu_hw_detect() — uses full module chain
"""
import os, re, subprocess

KERNEL = '/home/wubu/wubunos/src/kernel'
TESTS_MK = '/home/wubu/wubunos/mk/tests.mk'
CC = 'gcc'
CFLAGS = ['-O0', '-g', '-std=c11', '-D_GNU_SOURCE', '-no-pie', '-I' + KERNEL]

# Write stubs file - linker symbols + unresolvable symbols
with open(os.path.join(KERNEL, 'wubu_test_stubs.c'), 'w') as f:
    f.write("""#include <stdint.h>
#include <unistd.h>
#include <stddef.h>

/* Linker-script symbols (kernel.ld) */
uint64_t _kernel_start = 0x00100000;
uint64_t _stack_top = 0x00200000;
uint64_t _image_start = 0x00100000;
uint64_t _bss_start = 0x001F0000;
uint64_t _bss_end = 0x001FC000;
uint64_t _text_end = 0x001F0000;

/* Self-test runner */
int wubu_self_test_run(const char *name) { (void)name; return 0; }
""")

# Step 1: Find compilable wubu_*.c files (non-selftest, non-test)
compilable = []
for f in sorted(os.listdir(KERNEL)):
    if not f.endswith('.c'): continue
    if not f.startswith('wubu_'): continue
    name = re.match(r'wubu_(\w+)\.c$', f)
    if not name: continue
    modname = name.group(1)
    if modname.endswith('_selftest') or modname.endswith('_test'): continue
    if f == 'wubu_test_stubs.c': continue
    
    r = subprocess.run([CC] + CFLAGS + ['-c', '-o', '/dev/null', os.path.join(KERNEL, f)],
                       capture_output=True, text=True, timeout=15)
    if r.returncode == 0:
        compilable.append(f)
    else:
        errs = [l for l in r.stderr.split('\n') if 'error:' in l]
        print(f"  COMPILE FAIL: {f}: {errs[0][:80] if errs else '?'}")

# Also add non-wubu .c files that are part of the kernel (non-test)
for f in sorted(os.listdir(KERNEL)):
    if not f.endswith('.c'): continue
    if f.startswith('wubu_') or f.startswith('test_') or f.endswith('_test.c'): continue
    if f in ('cpio_extract.c', 'wubu_test_stubs.c', 'wubu_bonzi_study.c', 'wubu_crash.c',
             'zip_extract.c', 'cab_extract.c', 'zlib_selftest.c', 'lzx_selftest.c',
             'zip_selftest.c', 'cab_selftest.c'):
        continue
    r = subprocess.run([CC] + CFLAGS + ['-c', '-o', '/dev/null', os.path.join(KERNEL, f)],
                       capture_output=True, text=True, timeout=15)
    if r.returncode == 0:
        compilable.append(f)
        print(f"  + {f}")

print(f"\nCompilable: {len(compilable)}")

# Step 2: Iteratively link and collect ALL undefined symbols
probe_selftest = 'wubu_accel_selftest.c'
probe_module = 'wubu_accel.c'
current = list(compilable)  # DON'T exclude probe_module - it's needed by hw_detect

# First pass: link with all files, collect undefined symbols that are NOT
# defined by any file in the set
all_undefined = set()
for iteration in range(40):
    cc_files = [probe_selftest] + current
    cmd = [CC] + CFLAGS + [os.path.join(KERNEL, 'wubu_test_stubs.c')] + \
          [os.path.join(KERNEL, f) for f in cc_files] + ['-lm', '-lm', '-o', '/dev/null']
    r = subprocess.run(cmd, capture_output=True, text=True, timeout=120)
    
    if r.returncode == 0:
        print(f"SUCCESS iteration {iteration}: {len(current)} files")
        break
    
    # Collect ALL undefined symbols
    undefined = set()
    for line in r.stderr.split('\n'):
        m = re.search(r'undefined reference to `([^`]+)', line)
        if m: undefined.add(m.group(1))
    
    # Find offending .c/.S files from error context
    offending = set()
    error_lines = r.stderr.split('\n')
    for i, line in enumerate(error_lines):
        if 'undefined reference' in line or 'multiple definition' in line or 'relocation' in line:
            # Check this line and the line above for file references
            for check_line in [line] + ([error_lines[i-1]] if i > 0 else []):
                m = re.match(r'.*?/(\w+\.(?:c|S)):', check_line)
                if m:
                    fn = m.group(1)
                    if fn in current:
                        offending.add(fn)
    
    if not offending:
        # Collect undefined symbols into stubs
        for sym in sorted(undefined):
            if sym.startswith('_') or sym == 'wubu_self_test_run':
                continue
            # Check if any file defines it
            found = False
            for fn in current:
                content = open(os.path.join(KERNEL, fn), errors='replace').read()
                if re.search(rf'(?:int|void|uint\d+_t|size_t|char|bool|double|float)\s+{sym}\s*\(', content):
                    found = True
                    break
            if not found:
                all_undefined.add(sym)
        break
    
    for fn in offending:
        if fn in current:
            current.remove(fn)
    print(f"Iteration {iteration}: removed {len(offending)}: {offending}")

# Add remaining undefined symbols to stubs
if all_undefined:
    print(f"\nAdding {len(all_undefined)} stubs for: {sorted(all_undefined)[:10]}")
    stub = open(os.path.join(KERNEL, 'wubu_test_stubs.c')).read()
    # Remove trailing """)
    stub = stub.rstrip().rstrip('""")')
    for sym in sorted(all_undefined):
        stub += f'\nint {sym}(void) {{ return 0; }}  /* auto-stub */'
    stub += '\n"""\n'
    # Actually, simpler: rewrite the stubs file
    with open(os.path.join(KERNEL, 'wubu_test_stubs.c'), 'w') as f:
        f.write("""#include <stdint.h>
#include <unistd.h>
#include <stddef.h>

uint64_t _kernel_start = 0x00100000;
uint64_t _stack_top = 0x00200000;
uint64_t _image_start = 0x00100000;
uint64_t _bss_start = 0x001F0000;
uint64_t _bss_end = 0x001FC000;
uint64_t _text_end = 0x001F0000;
int wubu_self_test_run(const char *name) { (void)name; return 0; }
""")
        for sym in sorted(all_undefined):
            f.write(f'int {sym}(void) {{ return 0; }}  /* auto-stub */\n')
    
    # Re-run iteration with new stubs
    current = list(compilable)
    for iteration in range(40):
        cc_files = [probe_selftest] + current
        cmd = [CC] + CFLAGS + [os.path.join(KERNEL, 'wubu_test_stubs.c')] + \
              [os.path.join(KERNEL, f) for f in cc_files] + ['-lm', '-lm', '-o', '/dev/null']
        r = subprocess.run(cmd, capture_output=True, text=True, timeout=120)
        if r.returncode == 0:
            print(f"SUCCESS with stubs: {len(current)} files")
            break
        offending = set()
        error_lines = r.stderr.split('\n')
        for i, line in enumerate(error_lines):
            if 'undefined reference' in line or 'multiple definition' in line or 'relocation' in line:
                for check_line in [line] + ([error_lines[i-1]] if i > 0 else []):
                    m = re.match(r'.*?/(\w+\.(?:c|S)):', check_line)
                    if m and m.group(1) in current:
                        offending.add(m.group(1))
        if not offending:
            print(f"Cannot resolve after stubs. Errors:")
            for l in error_lines[-8:]:
                if l.strip(): print(f"  {l[:150]}")
            break
        for fn in offending:
            if fn in current: current.remove(fn)
        print(f"Iteration {iteration}: removed {offending}")

# Save the working core module set
with open('/tmp/core_modules.txt', 'w') as f:
    for fn in sorted(current):
        f.write(fn + '\n')
print(f"\nFinal core modules: {len(current)} files")
