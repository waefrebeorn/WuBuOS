#!/usr/bin/env python3
# Compare ReactOS sysfuncs.lst with Windows 11 24H2

with open("reactos-study/reactos/ntoskrnl/sysfuncs.lst") as f:
    reactos = {}
    for line in f:
        line = line.strip()
        if line and not line.startswith('#'):
            parts = line.split()
            if len(parts) >= 2:
                name = parts[0]
                if name.startswith('Nt'):
                    try:
                        ord_num = int(parts[-1])
                        reactos[name] = ord_num
                    except:
                        pass

with open("/tmp/w11_29599.txt") as f:
    w11 = {}
    for line in f:
        line = line.strip()
        if line and '\t' in line:
            name, ord_num = line.split('\t')
            if name.startswith('Nt'):
                w11[name] = int(ord_num)

print(f"ReactOS sysfuncs.lst: {len(reactos)} syscalls")
print(f"Windows 11 24H2: {len(w11)} syscalls")

# What Windows 11 has that ReactOS doesn't
w11_only = set(w11.keys()) - set(reactos.keys())
print(f"\nWindows 11 only (not in ReactOS): {len(w11_only)}")
for name in sorted(w11_only):
    print(f"  {name}\t{w11[name]}")

# What ReactOS has that Windows 11 doesn't
reactos_only = set(reactos.keys()) - set(w11.keys())
print(f"\nReactOS only (not in Windows 11): {len(reactos_only)}")
for name in sorted(reactos_only):
    print(f"  {name}\t{reactos[name]}")

# Ordinal differences
ord_diff = []
for name in set(reactos.keys()) & set(w11.keys()):
    if reactos[name] != w11[name]:
        ord_diff.append((name, reactos[name], w11[name]))
if ord_diff:
    print(f"\nOrdinal differences: {len(ord_diff)}")
    for name, r, w in sorted(ord_diff)[:50]:
        print(f"  {name}: ReactOS={r}, Windows11={w}")
    if len(ord_diff) > 50:
        print(f"  ... and {len(ord_diff) - 50} more")