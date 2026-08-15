#!/usr/bin/env python3
# Check which ordinals from sysfuncs.lst are registered in the dispatch table

# Parse sysfuncs.lst
with open("reactos-study/reactos/ntoskrnl/sysfuncs.lst") as f:
    reactos = []
    for line in f:
        line = line.strip()
        if line and not line.startswith('#'):
            parts = line.split()
            if len(parts) >= 2:
                name = parts[0]
                if name.startswith('Nt'):
                    ordinal = int(parts[-1])
                    reactos.append((name[2:].lower(), ordinal))

# Parse vsl_nt_misc.c register function for its ordinals
with open("src/runtime/vsl/vsl_nt_misc.c") as f:
    misc_content = f.read()

# Extract all tbl[...] = ... assignments
import re
misc_ordinals = re.findall(r'tbl\[(\d+)-1\]', misc_content)
misc_ordinals = {int(x) for x in misc_ordinals}

# Parse other modules
modules = ['vsl_nt_atoms', 'vsl_nt_job', 'vsl_nt_io', 'vsl_nt_vmem', 
           'vsl_nt_process', 'vsl_nt_thread', 'vsl_nt_section', 
           'vsl_nt_timer', 'vsl_nt_sync', 'vsl_nt_registry', 'vsl_nt_token']

all_ordinals = set(misc_ordinals)

for mod in modules:
    try:
        with open(f"src/runtime/vsl/{mod}.c") as f:
            content = f.read()
        ords = re.findall(r'tbl\[(\d+)-1\]', content)
        all_ordinals.update({int(x) for x in ords})
    except:
        pass

print(f"Total ordinals registered: {len(all_ordinals)}")
print(f"ReactOS ordinals: {len(reactos)}")

# Find missing
reactos_ords = {ord for _, ord in reactos}
missing = reactos_ords - all_ordinals
print(f"\nMissing ordinals ({len(missing)}):")
for m in sorted(missing):
    for name, ord in reactos:
        if ord == m:
            print(f"  {m}: {name}")
            break

# Also show which reactos ones we DO have
have = reactos_ords & all_ordinals
print(f"\nImplemented ordinals ({len(have)}):")
for h in sorted(have):
    for name, ord in reactos:
        if ord == h:
            print(f"  {h}: {name}")
            break