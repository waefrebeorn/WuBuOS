#!/usr/bin/env python3
"""
Build all Type B test targets, collect all unique undefined reference
symbols from linker errors, then add stubs for those symbols.
Uses nm to check which symbols are already defined.
"""
import os, re, subprocess

KERNEL = '/home/wubu/wubunos/src/kernel'
TESTS_MK = '/home/wubu/wubunos/mk/tests.mk'
CC = 'gcc'
CFLAGS = ['-O0', '-g', '-std=c11', '-D_GNU_SOURCE', '-no-pie', '-I' + KERNEL]

# Load core modules
with open('/tmp/core_modules.txt') as f:
    core_modules = [line.strip() for line in f if line.strip()]

# Get test modules
selftests = set()
for f in os.listdir(KERNEL):
    m = re.match(r'wubu_(\w+)_selftest\.c$', f)
    if m: selftests.add(m.group(1))
all_mods = set()
for f in os.listdir(KERNEL):
    m = re.match(r'wubu_(\w+)\.c$', f)
    if m:
        name = m.group(1)
        if not name.endswith('_selftest') and not name.endswith('_test'):
            all_mods.add(name)
test_modules = sorted(all_mods & selftests)
test_modules = [m for m in test_modules if m not in {'flush', 'flush2', 'self_test', 'agi_kernel', 'hive', 'bonzi'}]

# Build each Type B target and collect undefined symbols
all_undefined = set()
for mod in test_modules[:20]:  # First 20
    selftest_c = f'wubu_{mod}_selftest.c'
    module_c = f'wubu_{mod}.c'
    st = open(os.path.join(KERNEL, selftest_c), errors='replace').read()
    if 'wubu_hw_detect()' not in st and 'wubu_probe_all' not in st:
        continue  # Skip Type A
    
    cc_files = [selftest_c, module_c] + [f for f in core_modules if f != module_c]
    cmd = [CC] + CFLAGS + [os.path.join(KERNEL, 'wubu_test_stubs.c')] + \
          [os.path.join(KERNEL, f) for f in cc_files] + ['-lm', '-lm', '-o', '/dev/null']
    r = subprocess.run(cmd, capture_output=True, text=True, timeout=120)
    
    for line in r.stderr.split('\n'):
        m = re.search(r'undefined reference to `([^`]+)', line)
        if m:
            all_undefined.add(m.group(1))
    undef_count = len([l for l in r.stderr.split('\n') if 'undefined' in l])
    print(f"  {mod}: {'OK' if r.returncode == 0 else f'{undef_count} undefined'}")

print(f"\nTotal unique undefined: {len(all_undefined)}")
for s in sorted(all_undefined):
    print(f"  {s}")
