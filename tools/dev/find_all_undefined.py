#!/usr/bin/env python3
"""
Find ALL undefined symbols across ALL Type B test targets, then
add them to wubu_test_stubs.c as stubs.
"""
import os, re, subprocess, glob

KERNEL = '/home/wubu/wubunos/src/kernel'
CC = 'gcc'
CFLAGS = ['-O0', '-g', '-std=c11', '-D_GNU_SOURCE', '-no-pie', '-I' + KERNEL]

# Get core module list
with open('/tmp/core_modules.txt') as f:
    core_modules = [line.strip() for line in f if line.strip()]

# Get test modules
selftests = set()
for f in os.listdir(KERNEL):
    m = re.match(r'wubu_(\w+)_selftest\.c$', f)
    if m: selftests.add(m.group(1))
all_modules = set()
for f in core_modules:
    m = re.match(r'wubu_(\w+)\.c$', f)
    if m: all_modules.add(m.group(1))
for f in os.listdir(KERNEL):
    m = re.match(r'wubu_(\w+)\.c$', f)
    if m:
        name = m.group(1)
        if not name.endswith('_selftest') and not name.endswith('_test'):
            all_modules.add(name)
test_modules = sorted(all_modules & selftests)
exclude = {'flush', 'flush2', 'self_test', 'agi_kernel', 'hive', 'bonzi'}
test_modules = [m for m in test_modules if m not in exclude]

# Find all undefined symbols across Type B targets
all_undefined = set()
for mod in test_modules:
    selftest_c = f'wubu_{mod}_selftest.c'
    module_c = f'wubu_{mod}.c'
    
    # Check if Type B
    st = open(os.path.join(KERNEL, selftest_c), errors='replace').read()
    if 'wubu_hw_detect()' not in st and 'wubu_probe_all' not in st:
        continue  # Skip Type A
    
    cc_files = [selftest_c, module_c] + [f for f in core_modules if f != module_c]
    cmd = [CC] + CFLAGS + [os.path.join(KERNEL, 'wubu_test_stubs.c')] + \
          [os.path.join(KERNEL, f) for f in cc_files] + ['-lm', '-lm', '-o', '/dev/null']
    r = subprocess.run(cmd, capture_output=True, text=True, timeout=120)
    
    if r.returncode == 0:
        continue
    
    for line in r.stderr.split('\n'):
        m = re.search(r'undefined reference to `([^`]+)', line)
        if m:
            all_undefined.add(m.group(1))

print(f"Total unique undefined symbols: {len(all_undefined)}")
for sym in sorted(all_undefined):
    print(f"  {sym}")
