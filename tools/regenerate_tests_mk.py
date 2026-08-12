#!/usr/bin/env python3
"""
Regenerate ALL test_hw_<mod> targets in mk/tests.mk.

Uses the verified core module set from /tmp/core_modules.txt
(found by find_core_modules.py) for Type B targets. Type A targets
use only the minimal core runtime.

Each target:
  - type_hw_<mod>_selftest.c (defines main)
  - wubu_<mod>.c (module under test)
  - Type B: all 282 core kernel modules (verified to link cleanly)
  - Type A: libc.c, libc_string.c, memory.c, klog.c, wubu_pci.c
  - wubu_test_stubs.c (in BOTH types for linker symbols)
  - -lm -lm -no-pie
"""
import os, re

KERNEL = '/home/wubu/wubunos/src/kernel'
TESTS_MK = '/home/wubu/wubunos/mk/tests.mk'

src = open(TESTS_MK, encoding='utf-8').read()

# Remove existing test_hw_* blocks
lines = src.split('\n')
out = []
skip = False
for line in lines:
    if skip:
        if line.strip() == '' or (line and not line.startswith('\t') and not line.startswith(' ') and ':' not in line):
            skip = False
            if line.strip() == '':
                continue
            out.append(line)
        continue
    if re.match(r'^test_hw_\w+:', line):
        skip = True
        if out and out[-1] == '':
            out.pop()
        continue
    out.append(line)
src_base = '\n'.join(out)

# Load verified core modules
core_file = '/tmp/core_modules.txt'
if os.path.exists(core_file):
    with open(core_file) as f:
        core_modules = [line.strip().replace('$(KERNEL)/', '').rstrip('\\').strip()
                       for line in f if line.strip()]
    print(f"Loaded {len(core_modules)} core modules from {core_file}")
else:
    print("ERROR: /tmp/core_modules.txt not found. Run find_core_modules.py first.")
    exit(1)

core_cc = ['libc.c', 'libc_string.c', 'memory.c', 'klog.c', 'wubu_pci.c']

# Discover test modules
selftests = set()
for f in os.listdir(KERNEL):
    m = re.match(r'wubu_(\w+)_selftest\.c$', f)
    if m: selftests.add(m.group(1))
all_modules = set()
for f in core_modules:
    m = re.match(r'wubu_(\w+)\.c$', f)
    if m: all_modules.add(m.group(1))
# Also check all .c files in KERNEL for modules not in core_modules
for f in os.listdir(KERNEL):
    m = re.match(r'wubu_(\w+)\.c$', f)
    if m:
        name = m.group(1)
        if not name.endswith('_selftest') and not name.endswith('_test'):
            all_modules.add(name)

test_modules = sorted(all_modules & selftests)
exclude = {'flush', 'flush2', 'self_test', 'agi_kernel', 'hive', 'bonzi'}
test_modules = [m for m in test_modules if m not in exclude]
print(f"Test modules: {len(test_modules)}")

# Classify: Type A (no wubu_hw_detect call) vs Type B
type_a, type_b = [], []
for mod in test_modules:
    selftest_f = f'wubu_{mod}_selftest.c'
    content = open(os.path.join(KERNEL, selftest_f), errors='replace').read()
    if 'wubu_hw_detect()' in content or 'wubu_probe_all' in content:
        type_b.append(mod)
    else:
        type_a.append(mod)
print(f"Type A: {len(type_a)}, Type B: {len(type_b)}")

new_targets = []
for mod in test_modules:
    selftest_c = f'wubu_{mod}_selftest.c'
    module_c = f'wubu_{mod}.c'
    is_type_b = mod in type_b
    
    if is_type_b:
        other_modules = [f for f in core_modules if f != module_c]
        cc_files = [f'$(KERNEL)/{selftest_c}', f'$(KERNEL)/{module_c}'] + \
                   [f'$(KERNEL)/{f}' for f in other_modules] + \
                   ['$(KERNEL)/wubu_test_stubs.c']
        dep_files = [f'$(KERNEL)/{f}' for f in core_modules] + \
                    [f'$(KERNEL)/wubu_test_stubs.c', f'$(KERNEL)/{selftest_c}']
    else:
        cc_files = [f'$(KERNEL)/{selftest_c}', f'$(KERNEL)/{module_c}'] + \
                   [f'$(KERNEL)/{f}' for f in core_cc] + \
                   ['$(KERNEL)/wubu_test_stubs.c']
        dep_files = [f'$(KERNEL)/{f}' for f in core_cc] + \
                    [f'$(KERNEL)/wubu_test_stubs.c', f'$(KERNEL)/{selftest_c}']
    
    cc_chain = ' \\\n\t\t'.join(cc_files + ['-lm', '-lm'])
    deps = ' '.join(dep_files)
    
    target = f'''test_hw_{mod}: {deps}
\t$(CC) -O0 -g -Wall -Wextra -std=c11 -D_GNU_SOURCE -no-pie -I$(KERNEL) \\
\t\t{cc_chain} \\
\t\t-o $(KERNEL)/test_hw_{mod}
\t$(KERNEL)/test_hw_{mod}'''
    new_targets.append(target)

result = src_base.rstrip() + '\n\n' + '\n\n'.join(new_targets) + '\n'
open(TESTS_MK, 'w', encoding='utf-8').write(result)

print(f"\nWrote {len(new_targets)} targets")
dupes = [t for t in set(re.findall(r'(test_hw_\w+):', result)) if result.count(f'{t}:') > 1]
print(f"Duplicate targets: {len(dupes)}")
