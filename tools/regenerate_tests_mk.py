#!/usr/bin/env python3
"""
Regenerate ALL test_hw_<mod> targets in mk/tests.mk.

Each target: selftest + module .c + ALL compilable kernel modules +
wubu_test_stubs.c + -lm -lm -no-pie.

wubu_test_stubs.c provides linker symbols normally supplied by the
kernel's linker script and arch assembly (SMP trampolines, I/O ports,
etc.) that aren't available in the standalone test harness.
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

# Discover all non-selftest, non-test .c/.S files
kernel_files = []
selftests = set()
for f in sorted(os.listdir(KERNEL)):
    if f.endswith('.c'):
        m = re.match(r'wubu_(\w+)_selftest\.c$', f)
        if m:
            selftests.add(m.group(1))
            continue
        m = re.match(r'wubu_(\w+)\.c$', f)
        if m:
            name = m.group(1)
            if name.endswith('_selftest') or name.endswith('_test'):
                continue
            kernel_files.append(f)
        elif not f.startswith('test_') and not f.endswith('_test.c') and f != 'cpio_extract.c' and f != 'wubu_test_stubs.c':
            kernel_files.append(f)
    elif f.endswith('.S'):
        if not f.startswith('test_'):
            kernel_files.append(f)

# Exclude files that define main() or _start
exclude_def = {'crt0.S', 'boot.S', 'zip_extract.c', 'cab_extract.c', 'lzx_selftest.c',
                'zip_selftest.c', 'zlib_selftest.c', 'cab_selftest.c', 'wubu_math.c',
                'cpio_extract.c', 'wubu_test_stubs.c'}
kernel_files = [f for f in kernel_files if f not in exclude_def]

print(f"Kernel files: {len(kernel_files)}")

# Discover test modules
all_modules = set()
for f in kernel_files:
    m = re.match(r'wubu_(\w+)\.c$', f)
    if m: all_modules.add(m.group(1))

test_modules = sorted(all_modules & selftests)
exclude_mods = {'flush', 'flush2', 'self_test', 'agi_kernel', 'hive', 'bonzi'}
test_modules = [m for m in test_modules if m not in exclude_mods]
print(f"Test modules: {len(test_modules)}")

# Classify: Type B if calls wubu_hw_detect() or wubu_probe_all()
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
    
    # ALL targets get the same kernel deps (all kernel files + stubs)
    # For Type A, we only need core runtime (smaller, faster compile)
    if is_type_b:
        other_modules = [f for f in kernel_files if f != module_c]
        cc_files = [f'$(KERNEL)/{selftest_c}', f'$(KERNEL)/{module_c}'] + \
                   [f'$(KERNEL)/{f}' for f in other_modules] + \
                   ['$(KERNEL)/wubu_test_stubs.c']
        dep_files = [f'$(KERNEL)/{f}' for f in kernel_files] + \
                    [f'$(KERNEL)/wubu_test_stubs.c', f'$(KERNEL)/{selftest_c}']
    else:
        core_cc = ['libc.c', 'libc_string.c', 'memory.c', 'klog.c', 'wubu_pci.c']
        cc_files = [f'$(KERNEL)/{selftest_c}', f'$(KERNEL)/{module_c}'] + \
                   [f'$(KERNEL)/{f}' for f in core_cc]
        dep_files = [f'$(KERNEL)/{f}' for f in core_cc] + [f'$(KERNEL)/{selftest_c}']
    
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
