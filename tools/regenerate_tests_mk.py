#!/usr/bin/env python3
"""
Regenerate ALL test_hw_<mod> targets in mk/tests.mk with FAST incremental
builds, using a single CORE_OBJS make variable (defined once) instead of
repeating 283 filenames per target.

Strategy:
  - CORE_OBJS := cached .o for each verified core module (defined once)
  - Each test_hw_<mod> links its selftest.c + module.c + stubs.c against
    $(CORE_OBJS).  Core .o cached via a %.o pattern rule (only rebuilt on
    source change).  First build compiles everything; later builds fast.

Layout (post-reorg):
  - Kernel sources:           src/kernel/*.c  (*.h)
  - Test selftest sources:    src/kernel/test/*_selftest.c, src/kernel/test/test_*.c
  - Test build artifacts:     build/testbin/test_hw_<mod>
  - Cached objects:           build/testobj/*.o
"""
import os, re

KERNEL = '/home/wubu/wubunos/src/kernel'
TESTDIR = os.path.join(KERNEL, 'test')
TESTS_MK = '/home/wubu/wubunos/mk/tests.mk'
CC = 'gcc'
CFLAGS_BASE = '-O0 -g -Wall -Wextra -std=c11 -D_GNU_SOURCE -no-pie -I' + KERNEL

with open('/tmp/core_modules.txt') as f:
    core_modules = [line.strip() for line in f if line.strip()]

core_runtime = ['libc.c', 'libc_string.c', 'memory.c', 'klog.c', 'wubu_pci.c']

# Discover test modules (selftests live in src/kernel/test/ now)
selftests = set()
for f in os.listdir(TESTDIR):
    m = re.match(r'wubu_(\w+)_selftest\.c$', f)
    if m: selftests.add(m.group(1))
all_modules = set()
for f in os.listdir(KERNEL):
    m = re.match(r'wubu_(\w+)\.c$', f)
    if m:
        name = m.group(1)
        if not name.endswith('_selftest') and not name.endswith('_test') and name != 'flush' and name != 'flush2':
            all_modules.add(name)
test_modules = sorted(all_modules & selftests)
test_modules = [m for m in test_modules if not m.startswith('flush')]

type_a, type_b = [], []
for mod in test_modules:
    st = open(os.path.join(TESTDIR, f'wubu_{mod}_selftest.c'), errors='replace').read()
    if 'wubu_hw_detect()' in st or 'wubu_probe_all' in st:
        type_b.append(mod)
    else:
        type_a.append(mod)
print(f"Type A: {len(type_a)}, Type B: {len(type_b)}, total: {len(test_modules)}")

# Read existing tests.mk (working .c-based committed version), strip
# test_hw_* target blocks AND any old cached-object infrastructure.
with open(TESTS_MK, encoding='utf-8') as f:
    lines = f.read().split('\n')

preamble = []
i = 0
while i < len(lines):
    line = lines[i]
    # Skip cached-object infra lines and their recipe blocks
    if line.startswith('CACHE :=') or line.startswith('CORE_OBJS :=') or \
       line.startswith('RUNTIME_OBJS :=') or line.startswith('TEST_BIN :=') or \
       line.startswith('$(CACHE):') or \
       line.startswith('$(CACHE)/%.o:') or line.startswith('# ---- cached kernel-test'):
        i += 1
        continue
    # Stop at first test_hw_ target
    if line.startswith('test_hw_') and ':' in line:
        break
    preamble.append(line)
    i += 1

# Build the new file: preamble + cached infra + CORE_OBJS + all targets
out = []
out.extend(preamble)

out.append('')
out.append('# ---- fast incremental kernel-test build (cached objects) ----')
out.append('CACHE := build/testobj')
out.append('TEST_BIN := build/testbin')
out.append('')
out.append('# Cached-object pattern rule: any kernel .c -> build/testobj/*.o')
out.append('$(CACHE)/%.o: $(KERNEL)/%.c')
out.append('\t@mkdir -p $(CACHE)')
out.append('\t$(CC) -O0 -g -std=c11 -D_GNU_SOURCE -no-pie -I$(KERNEL) -c $< -o $@')
out.append('.PHONY: test_all')
out.append('test_all: ' + ' '.join(f'test_hw_{m}' for m in test_modules))
out.append('')
out.append('# The full verified core module set (pre-compiled once).')
core_objs = ' '.join(f'$(CACHE)/{f.replace(".c", ".o")}' for f in core_modules)
out.append(f'CORE_OBJS := {core_objs}')
out.append('')
runtime_objs = ' '.join(f'$(CACHE)/{f.replace(".c", ".o")}' for f in core_runtime)
out.append(f'RUNTIME_OBJS := {runtime_objs}')
out.append('')

new_targets = []
for mod in test_modules:
    selftest_c = f'wubu_{mod}_selftest.c'
    module_c = f'wubu_{mod}.c'
    is_type_b = mod in type_b

    if is_type_b:
        # CORE_OBJS minus the module's own .o (module .c is compiled fresh)
        objs = [f'$(CACHE)/{f.replace(".c", ".o")}' for f in core_modules if f != module_c]
        objs_var = ' '.join(objs)
    else:
        objs = [f'$(CACHE)/{f.replace(".c", ".o")}' for f in core_runtime if f != module_c]
        objs_var = ' '.join(objs)

    # Compile fresh: selftest + module + stubs (test-specific, provide main)
    # Selftests live in src/kernel/test/, binaries output to build/testbin/
    target = f'''test_hw_{mod}: $(KERNEL)/test/{selftest_c} $(KERNEL)/{module_c} $(KERNEL)/wubu_test_stubs.c {objs_var}
\t@mkdir -p $(TEST_BIN)
\t$(CC) {CFLAGS_BASE} \\
\t\t$(KERNEL)/test/{selftest_c} \\
\t\t$(KERNEL)/{module_c} \\
\t\t$(KERNEL)/wubu_test_stubs.c \\
\t\t{objs_var} \\
\t\t-lm -lm \\
\t\t-o $(TEST_BIN)/test_hw_{mod}
\t$(TEST_BIN)/test_hw_{mod}'''
    new_targets.append(target)

out.extend(new_targets)
out.append('')

with open(TESTS_MK, 'w', encoding='utf-8') as f:
    f.write('\n'.join(out))

# Verify
with open(TESTS_MK) as f:
    content = f.read()
target_names = re.findall(r'^(test_hw_\w+):', content, re.M)
dups = [t for t in set(target_names) if target_names.count(t) > 1]
print(f"Wrote {len(new_targets)} targets, file = {len(out)} lines")
print(f"Duplicate targets: {len(dups)}")
