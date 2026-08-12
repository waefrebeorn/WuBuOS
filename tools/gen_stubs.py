#!/usr/bin/env python3
"""
Generate wubu_test_stubs.c with stubs for functions that are CALLED
(with parentheses) from selftests but NOT defined in any kernel .c/.h file.
Uses nm to find actual symbol definitions (not regex on source code).
"""
import os, re, subprocess

KERNEL = '/home/wubu/wubunos/src/kernel'

# Get all defined global symbols from ALL compilable .c files
all_defined = set()
for f in sorted(os.listdir(KERNEL)):
    if not f.endswith('.c') or '_selftest' in f or f == 'wubu_test_stubs.c' or f.startswith('test_') or f.endswith('_test.c'):
        continue
    obj = f'/tmp/stub_sym_{f.replace(".c", ".o")}'
    r = subprocess.run(['gcc', '-O0', '-g', '-std=c11', '-D_GNU_SOURCE', '-I' + KERNEL, '-c', '-o', obj, os.path.join(KERNEL, f)],
                       capture_output=True, text=True, timeout=15)
    if r.returncode != 0: continue
    r2 = subprocess.run(['nm', obj], capture_output=True, text=True, timeout=5)
    for line in r2.stdout.split('\n'):
        parts = line.split()
        if len(parts) >= 3 and parts[1] in ('T', 'D', 'R', 'B', 'W'):
            all_defined.add(parts[2])
    os.unlink(obj)

print(f"Defined global symbols: {len(all_defined)}")

# Find all wubu_* function CALLS in selftest files
called_funcs = set()
for f in sorted(os.listdir(KERNEL)):
    if not re.match(r'wubu_\w+_selftest\.c$', f): continue
    content = open(os.path.join(KERNEL, f), errors='replace').read()
    # Match function calls: wubu_word() or wubu_word(args)
    for m in re.finditer(r'\bwubu_\w+\s*\(', content):
        func = m.group(0).rstrip()[:-1]  # remove the (
        func = func.strip()
        called_funcs.add(func)

# Also get function calls from wubu_hw_detect.c (which calls all probes)
for f in ['wubu_hw_detect.c', 'wubu_probe.c']:
    path = os.path.join(KERNEL, f)
    if os.path.exists(path):
        content = open(path, errors='replace').read()
        for m in re.finditer(r'\bwubu_\w+\s*\(', content):
            func = m.group(0).rstrip()[:-1].strip()
            called_funcs.add(func)

print(f"Called wubu_ functions: {len(called_funcs)}")

# Missing = called but not defined
missing = called_funcs - all_defined
missing = {m for m in missing if not m.startswith('wubu_') or len(m) > 12}  # filter module names

# Actually: filter out bare module names (like wubu_accel) - only keep _probe, _summary, etc.
missing = {m for m in missing if re.match(r'wubu_\w+_\w+', m) or m in ('wubu_kvfs_create', 'wubu_kvfs_open')}

print(f"Missing (need stubs): {len(missing)}")

# Generate stubs with smarter return types
stub_lines = ['#include <stdint.h>', '#include <stddef.h>', '#include <unistd.h>', '']
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
stub_lines.append('/* Globals from excluded modules */')
stub_lines.append('uint64_t task_tick_count = 0;')
stub_lines.append('')

stub_lines.append('/* Auto-generated stubs for functions called by selftests')
stub_lines.append(' * but not defined in any kernel .c file. */')

for func in sorted(missing):
    if func in ('wubu_self_test_run', 'wubu_hw_detect'):
        continue
    # Determine return type from naming convention
    if func.endswith('_summary'):
        stub_lines.append(f'void {func}(char *out, size_t cap) {{ (void)out; (void)cap; }}')
    elif func.endswith('_driver_for') or func.endswith('_codec_for') or \
         func.endswith('_layout_for') or func.endswith('_for') or \
         func.endswith('_str') or func.endswith('_path') or \
         func.endswith('_name') or func.endswith('_driver') or \
         func.endswith('_chain') or func.endswith('_config') or \
         func.endswith('_mode') or func.endswith('_format') or \
         func.endswith('_type') or func.endswith('_state') or \
         func.endswith('_route') or func.endswith('_route'):
        stub_lines.append(f'const char *{func}(void) {{ return NULL; }}')
    elif func.endswith('_driver') or func.endswith('_state') or func.endswith('_params'):
        stub_lines.append(f'const char *{func}(void) {{ return NULL; }}')
    elif func.endswith('_present') or func.endswith('_available') or func.endswith('_count') or func.endswith('_has'):
        stub_lines.append(f'int {func}(void) {{ return 0; }}')
    elif func.endswith('_probe'):
        stub_lines.append(f'void {func}(void) {{}}')
    elif func.endswith('_create'):
        stub_lines.append(f'void *{func}(void) {{ return NULL; }}')
    elif func.endswith('_open'):
        stub_lines.append(f'void *{func}(void) {{ return NULL; }}')
    elif func.endswith('_free'):
        stub_lines.append(f'void {func}(void *p) {{ (void)p; }}')
    else:
        # Check how it's called
        stub_lines.append(f'void {func}(void) {{}}')

content = '\n'.join(stub_lines) + '\n'
open(os.path.join(KERNEL, 'wubu_test_stubs.c'), 'w').write(content)
print(f"Wrote {len(stub_lines)} lines")
print(f"Sample stubs: {sorted(missing)[:10]}")
