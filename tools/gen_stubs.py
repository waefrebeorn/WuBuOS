#!/usr/bin/env python3
"""
Generate wubu_test_stubs.c using the compiler to discover which symbols
are already defined. For each .c file, compile with -D to list all
global symbols via `nm`. Then only stub symbols that are called by
selftests but NOT defined anywhere.
"""
import os, re, subprocess

KERNEL = '/home/wubu/wubunos/src/kernel'
CC = 'gcc'
CFLAGS = ['-O0', '-g', '-std=c11', '-D_GNU_SOURCE', '-I' + KERNEL]

# Step 1: Get all defined global symbols from all kernel .c files
all_defined = set()
for f in sorted(os.listdir(KERNEL)):
    if not f.endswith('.c'): continue
    if '_selftest' in f or f == 'wubu_test_stubs.c' or f.startswith('test_') or f.endswith('_test.c'):
        continue
    obj = f'/tmp/stub_check_{f.replace(".c", ".o")}'
    r = subprocess.run([CC] + CFLAGS + ['-c', '-o', obj, os.path.join(KERNEL, f)],
                       capture_output=True, text=True, timeout=15)
    if r.returncode != 0: continue
    # Get symbols
    r2 = subprocess.run(['nm', obj], capture_output=True, text=True, timeout=5)
    for line in r2.stdout.split('\n'):
        parts = line.split()
        if len(parts) >= 3 and parts[1] in ('T', 'D', 'R', 'B', 'W'):
            all_defined.add(parts[2])
    os.unlink(obj)

print(f"Defined global symbols: {len(all_defined)}")

# Step 2: Find all symbols called from selftests that start with wubu_
called_wubu = set()
for f in sorted(os.listdir(KERNEL)):
    m = re.match(r'wubu_(\w+)_selftest\.c$', f)
    if not m: continue
    mod = m.group(1)
    if mod.endswith('_test'): continue
    st = open(os.path.join(KERNEL, f), errors='replace').read()
    for sym in re.findall(r'wubu_\w+', st):
        if sym != f'wubu_{mod}_selftest':
            called_wubu.add(sym)

print(f"wubu_ symbols called from selftests: {len(called_wubu)}")

# Step 3: Find missing symbols (called but not defined)
missing = called_wubu - all_defined
print(f"Missing (need stubs): {len(missing)}")
for s in sorted(missing):
    print(f"  {s}")

# Step 4: Generate stubs
stub_lines = ['#include <stdint.h>', '#include <unistd.h>', '#include <stddef.h>', '']
stub_lines.append('/* Linker-script symbols (kernel.ld) */')
stub_lines.append('uint64_t _kernel_start = 0x00100000;')
stub_lines.append('uint64_t _stack_top = 0x00200000;')
stub_lines.append('uint64_t _image_start = 0x00100000;')
stub_lines.append('uint64_t _bss_start = 0x001F0000;')
stub_lines.append('uint64_t _bss_end = 0x001FC000;')
stub_lines.append('uint64_t _text_end = 0x001F0000;')
stub_lines.append('')
stub_lines.append('/* Self-test runner */')
stub_lines.append('int wubu_self_test_run(const char *name) { (void)name; return 0; }')
stub_lines.append('')
stub_lines.append('/* Globals from excluded arch modules */')
stub_lines.append('uint64_t task_tick_count = 0;')
stub_lines.append('')
stub_lines.append('')

stub_lines.append('/* Auto-generated stubs for functions called by')
stub_lines.append(' * selftests but not defined in any kernel .c file. */')
for sym in sorted(missing):
    if sym == 'wubu_self_test_run':
        continue
    # Determine return type from selftest usage
    # Check if used as string, pointer, int, void
    return_type = 'void'
    for f in os.listdir(KERNEL):
        m = re.match(r'wubu_(\w+)_selftest\.c$', f)
        if not m: continue
        st = open(os.path.join(KERNEL, f), errors='replace').read()
        # Look for the function call context
        for line in st.split('\n'):
            if sym in line and '(' in line and ')' in line:
                if 'strcmp' in line or 'printf' in line or 'CHECK' in line:
                    if sym + '(' in line and 'strcmp' in line:
                        return_type = 'const char *'
                        break
                if 'CHECK' in line and sym + '(' in line and ';' not in line.split(sym)[0][-5:]:
                    # Used as condition or value
                    pass
    stub_lines.append(f'void {sym}(void) {{}}  /* TODO: implement */')

content = '\n'.join(stub_lines) + '\n'
open(os.path.join(KERNEL, 'wubu_test_stubs.c'), 'w').write(content)
print(f"\nWrote {len(stub_lines)} lines to wubu_test_stubs.c")
