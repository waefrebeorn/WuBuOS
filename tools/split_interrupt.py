#!/usr/bin/env python3
"""Split interrupt.c into 3 focused modules:
  - interrupt.c: core dispatch + IDT setup (lines 1-548, 724-963)
  - interrupt_exceptions.c: exception handlers (lines 549-711)
  - interrupt_syscall.c: syscall registration + management (lines 724-end)
"""
import shutil

# Load the original
with open("src/kernel/interrupt.c") as f:
    lines = f.readlines()

# Find the split boundaries:
# 1. Exception handlers start at line 549 (handle_double_fault)
# 2. Syscall section starts at line 724 (typedef syscall_fn_t)
# Let's find exact boundaries...
for i, line in enumerate(lines):
    if 'void handle_double_fault' in line:
        print(f"Exception handlers start at line {i+1}")
    if 'typedef int64_t (*syscall_fn_t)' in line:
        print(f"Syscall section starts at line {i+1}")
    if 'void handle_gpf' in line and 'void handle_gpf(' in line:
        ex_end = i
        print(f"Last exception handler ends near line {i+1}")

# Find where exception handlers end (handle_gpf + its body)
# Find the gap_init section end / next non-exception function
ex_start = 548  # 0-indexed (line 549)
# Find end of handle_gpf
ex_end = None
for i in range(ex_start, len(lines)):
    if lines[i].strip() == '}' and i > ex_start:
        # Check if this is handle_gpf's closing brace
        for j in range(i, i-30, -1):
            if 'void handle_gpf' in lines[j]:
                ex_end = i + 1  # include the closing brace
                print(f"Exception handlers end at line {ex_end}")
                break
        if ex_end:
            break

# Find syscall start
syscall_start = None
for i, line in enumerate(lines):
    if 'typedef int64_t (*syscall_fn_t)' in line:
        syscall_start = i
        print(f"Syscall typedef at line {syscall_start+1}")
        break

# Find end of file / last syscall function
for i in range(len(lines)-1, syscall_start, -1):
    if lines[i].strip() and not lines[i].startswith('//') and not lines[i].startswith('/*'):
        sf = i
        break

print(f"\nFile total: {len(lines)} lines")
print(f"Exception handlers: lines {ex_start+1}-{ex_end}")
print(f"Syscall section: lines {syscall_start+1}-{sf+1}")
PYEOF