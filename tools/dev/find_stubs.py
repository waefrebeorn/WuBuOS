#!/usr/bin/env python3
"""
Add stubs for all undefined symbols found in test builds.
For each test_hw_<mod> target that fails to link, collect the
undefined symbols and add them to wubu_test_stubs.c.
"""
import os, re, subprocess, sys

KERNEL = '/home/wubu/wubunos/src/kernel'
CC = 'gcc'
CFLAGS = ['-O0', '-g', '-Wall', '-std=c11', '-D_GNU_SOURCE', '-no-pie', '-I' + KERNEL]

# Read current stubs
stub_file = os.path.join(KERNEL, 'wubu_test_stubs.c')
stub_content = open(stub_file).read()

# Get all test modules
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
test_mods = sorted(all_mods & selftests)
test_mods = [m for m in test_mods if m not in {'flush', 'flush2', 'self_test', 'agi_kernel', 'hive', 'bonzi'}]

# Get kernel_files (compilable non-selftest, non-test, excluding known bad)
kernel_files = []
for f in sorted(os.listdir(KERNEL)):
    if not f.endswith('.c') or f.endswith('.S'): continue
    if '_selftest' in f: continue
    if f.startswith('test_') or f.endswith('_test.c'): continue
    if f == 'cpio_extract.c' or f == 'wubu_test_stubs.c': continue
    r = subprocess.run([CC] + CFLAGS + ['-c', '-o', '/dev/null', os.path.join(KERNEL, f)],
                       capture_output=True, text=True, timeout=15)
    if r.returncode == 0:
        kernel_files.append(f)

# Known arch files that fail to link
arch_bad = {'interrupt.c', 'interrupt_exceptions.c', 'metal_main.c', 'ps2.c',
            'tasking.c', 'wubu_smp.c', 'wubu_vmm.c', 'wubu_apic.c',
            'wubu_memmgr.c', 'wubu_console.c', 'wubu_console_colonel.c',
            'wubu_console_recovery.c', 'wubu_swap.c', 'wubu_as.c',
            'wubu_vdso.c', 'wubu_agi_kernel.c', 'wubu_bonzi.c',
            'wubu_serial.c', 'wubu_sync.c', 'wubu_hid.c',
            'interrupt_pic.c', 'interrupt_pit.c', 'interrupt_syscall.c',
            'interrupt_timer.c', 'wubu_verifier.c', 'wubu_mem.c',
            'wubu_input.c', 'wubu_mmu.c', 'wubu_sha256.c',
            'interrupt_apic.c', 'isr_stubs.S', 'tasking_switch.S',
            'ps2.c', 'vbe.c', 'ahci.c', 'fat32.c', 'fat32_cluster.c',
            'fat32_dir.c', 'fat32_fat.c', 'fat32_file.c', 'fat32_format.c',
            'fat32_name.c', 'txfs.c', 'tzfs.c', 'input.c',
            'wubu_hpet.c', 'wubu_rtc.c', 'wubu_tpm.c', 'wubu_smbios.c',
            'wubu_wdt.c', 'wubu_uart.c', 'wubu_ioport.c',
            'wubu_tss.c', 'wubu_msr.c', 'wubu_gfxboot.c',
            'wubu_theme.c', 'wubu_smc.c', 'wubu_thermal.c'}

# Actually, let me just try ALL compilable files and collect ALL undefined symbols
# for one Type B target, then stub them all
test_mod = 'accel'
test_selftest = f'wubu_{test_mod}_selftest.c'
module_c = f'wubu_{test_mod}.c'

cc_files = [test_selftest, module_c] + [f for f in kernel_files if f != module_c]
cmd = [CC] + CFLAGS + [os.path.join(KERNEL, f) for f in cc_files] + ['-lm', '-lm', '-o', '/dev/null']
r = subprocess.run(cmd, capture_output=True, text=True, timeout=120)

undefined_syms = set()
for line in r.stderr.split('\n'):
    m = re.search(r'undefined reference to `([^`]+)', line)
    if m:
        undefined_syms.add(m.group(1))

# Also get the .c file that defines each
print(f"Undefined symbols ({len(undefined_syms)}):")
for sym in sorted(undefined_syms):
    # Find which .c file references this symbol
    print(f"  {sym}")

# Now generate stubs for each
print(f"\nGenerating stubs for {len(undefined_syms)} symbols...")

existing = set(re.findall(r'\b(\w+)\s*\(', stub_content))
stubs = []
for sym in sorted(undefined_syms):
    # Skip linker symbols (already handled)
    if sym.startswith('_') or sym in existing:
        continue
    # Skip math functions (libm)
    if sym in ('sin', 'cos', 'sqrt', 'pow', 'log', 'exp', 'floor', 'ceil'):
        continue
    # Determine return type (default to int/void)
    stubs.append(f'void {sym}(void) {{}}  /* auto-stub */')
    stubs.append(f'int {sym}(void);  /* placeholder */')

print(f"New stubs needed: {len(stubs)}")
# Write stubs to a separate file for review
with open('/tmp/auto_stubs.c', 'w') as f:
    f.write('// Auto-generated stubs for undefined symbols\n')
    f.write('#include <stddef.h>\n\n')
    for sym in sorted(undefined_syms):
        if sym.startswith('_') or sym in existing:
            continue
        f.write(f'void {sym}(void) {{}}\n')
print(f"Wrote /tmp/auto_stubs.c")
