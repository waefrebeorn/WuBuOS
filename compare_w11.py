#!/usr/bin/env python3
# Compare Windows 11 syscall tables: 22000 (21H2) vs 29599 (24H2)

with open("/tmp/w11_22000.txt") as f:
    old = {line.split('\t')[0]: int(line.split('\t')[1]) for line in f if line.strip() and '\t' in line}

with open("/tmp/w11_29599.txt") as f:
    new = {line.split('\t')[0]: int(line.split('\t')[1]) for line in f if line.strip() and '\t' in line}

print(f"Windows 11 21H2 (22000): {len(old)} syscalls")
print(f"Windows 11 24H2 (29599): {len(new)} syscalls")

# New syscalls in 24H2
new_syscalls = set(new.keys()) - set(old.keys())
print(f"\nNew syscalls in 24H2: {len(new_syscalls)}")
for name in sorted(new_syscalls):
    print(f"  {name}\t{new[name]}")

# Removed syscalls (unlikely but possible)
removed = set(old.keys()) - set(new.keys())
if removed:
    print(f"\nRemoved syscalls: {len(removed)}")
    for name in sorted(removed):
        print(f"  {name}\t{old[name]}")

# Changed ordinals
changed = []
for name in set(old.keys()) & set(new.keys()):
    if old[name] != new[name]:
        changed.append((name, old[name], new[name]))
if changed:
    print(f"\nChanged ordinals: {len(changed)}")
    for name, o, n in sorted(changed):
        print(f"  {name}: {o} -> {n}")