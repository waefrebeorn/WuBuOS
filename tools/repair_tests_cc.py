#!/usr/bin/env python3
"""
Repair mk/tests.mk: every test target's CC recipe is missing its own
wubu_<mod>_selftest.c file (the one that defines main()).

For each target `test_hw_<mod>:`, ensure the CC recipe compiles
wubu_<mod>_selftest.c as the FIRST file after the $(CC) line, and
also wubu_<mod>.c (the module itself, needed for probe symbols).
"""
import re, sys

path = 'mk/tests.mk'
src = open(path, encoding='utf-8').read()

fixed = 0
already = 0
problems = []

lines = src.split('\n')
i = 0
out = []
while i < len(lines):
    line = lines[i]
    if line.startswith('test_hw_'):
        m = re.match(r'(test_hw_\w+):', line)
        if not m:
            out.append(line); i += 1; continue
        tname = m.group(1)
        mod = tname[len('test_hw_'):]
        block_lines = [line]
        i += 1
        while i < len(lines) and lines[i].strip() != '':
            block_lines.append(lines[i])
            i += 1
        cc_idx = None
        for bi, bl in enumerate(block_lines):
            if '$(CC)' in bl:
                cc_idx = bi
                break
        if cc_idx is None:
            problems.append(tname + ': no CC line')
            out.extend(block_lines)
            continue
        recipe = '\n'.join(block_lines[cc_idx:])
        selftest_needle = f'wubu_{mod}_selftest.c'
        module_needle = f'wubu_{mod}.c'
        has_selftest = selftest_needle in recipe
        has_module = module_needle in recipe
        if has_selftest and has_module:
            already += 1
            out.extend(block_lines)
            continue
        insert = []
        if not has_selftest:
            insert.append('\t\t$(KERNEL)/wubu_' + mod + '_selftest.c \\')
        if not has_module:
            insert.append('\t\t$(KERNEL)/wubu_' + mod + '.c \\')
        if not insert:
            out.extend(block_lines); continue
        new_block = block_lines[:cc_idx+1] + insert + block_lines[cc_idx+1:]
        out.extend(new_block)
        fixed += 1
    else:
        out.append(line)
        i += 1

result = '\n'.join(out)
open(path, 'w', encoding='utf-8').write(result)

print(f"Fixed (inserted missing selftest/module): {fixed}")
print(f"Already correct: {already}")
if problems:
    print("Problems:")
    for p in problems:
        print("  ", p)
