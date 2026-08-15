#!/usr/bin/env python3
"""Fix mk/tests.mk: ensure every test_hw_* and exec_games link line that
contains wubu_hw_detect.c ALSO contains wubu_input.c and wubu_display.c."""
import re, sys

P = "mk/tests.mk"
lines = open(P).readlines()

CHANGED = False
for i, line in enumerate(lines):
    # Only touch compile/link continuation lines (tab-indented, contain $(KERNEL)/)
    if not (line.startswith('\t\t') or line.startswith('\t$(CC)')):
        continue
    if 'wubu_hw_detect.c' not in line:
        continue
    # Must have wubu_display.c and wubu_input.c on same logical compile line
    # We'll inject both if missing, after the last $(KERNEL)/wubu_*.c token
    for mod in ['wubu_input.c', 'wubu_display.c']:
        token = f'$(KERNEL)/{mod}'
        if token in line:
            continue
        # find last kernel source file position
        m = list(re.finditer(r'\$\(KERNEL\)/wubu_[a-z_]+\.c', line))
        if m:
            pos = m[-1].end()
            line = line[:pos] + ' ' + token + line[pos:]
            CHANGED = True

open(P, 'w').writelines(lines)
print("CHANGED" if CHANGED else "no-change")
