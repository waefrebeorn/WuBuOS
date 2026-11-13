#!/usr/bin/env python3
"""
dead_guard_scan.py -- find protection code that nothing ever runs.

WHY THIS EXISTS
---------------
Writing a guard and never calling it is worse than writing none: the next reader
assumes the thing is checked.  In this tree that exact defect existed for real --
`check_canaries()` stamped red-zone canaries on every allocation and detected
clobbered ones correctly, but had no caller on any allocator path, so an overflow
was silently absorbed by free().  Nothing else called the checker either.

A grep for the function name does not find that shape, because the checker DOES
have a caller (mem_check_redzones) that itself has no caller.  This tool builds
the real call graph and reports functions that nothing reaches.

WHAT IT REPORTS
---------------
Functions defined in .c files whose name suggests a check, verification or probe,
and which have ZERO call sites anywhere in the tree (their own definition
excluded).  Results are sorted by severity guess: access/audit/gate names first.

KNOWN FALSE POSITIVES -- READ BEFORE ACTING
-------------------------------------------
1. DISPATCH TABLES.  A handler assigned as `tbl[5-1] = vsl_nt_access_check_and_audit;`
   has no textual `name(` call, so it looks dead.  It is live.  The vsl_nt token
   checks, task_validate (#define TaskValidate task_validate in zealos_parity.h)
   and most *_probe drivers are in this class.
2. PARITY ALIASES.  `#define X Y` in zealos_parity.h means Y is called as X.
3. PUBLIC API.  A header-declared function may be called from another repo
   (wubuwizard/wubucompositor link against these objects) or by user scripts.
4. FUNCTION POINTERS PASSED AS ARGUMENTS (callback registration).

So treat every hit as a HYPOTHESIS, open the file, and decide.

USAGE
-----
    ./tools/dead_guard_scan.py                 # whole src tree
    ./tools/dead_guard_scan.py --repo ..       # another checkout
    ./tools/dead_guard_scan.py --all-names     # do not filter by name
"""

import argparse
import collections
import os
import re
import sys

DEF_RE = re.compile(
    r'^(?:static\s+)?(?:inline\s+)?[A-Za-z_][\w \t\*]*?\b(\w+)\s*\([^;{}]*\)\s*\{',
    re.M,
)
CALL_RE = re.compile(r'\b(\w+)\s*\(')
PROBE_RE = re.compile(
    r'check|verif|valid|guard|canar|audit|sanit|detect|gate|scrub|assert', re.I
)
# vendored / third-party trees that are not ours to fix
SKIP_PARTS = (
    'third_party', 'thirdparty', 'vendor', 'imgui', 'khronos', 'mujoco',
    '.git', 'node_modules', 'lua/www', '~WuBu~/lua',
)


def is_ours(path: str) -> bool:
    low = path.lower()
    return not any(part in low for part in SKIP_PARTS)


def scan(root: str):
    defs = {}                                  # name -> (relfile, lineno)
    spans = collections.defaultdict(list)      # relfile -> [(start, end)]
    calls = collections.Counter()

    for dirpath, _, files in os.walk(root):
        for fn in files:
            if not fn.endswith('.c'):
                continue
            path = os.path.join(dirpath, fn)
            if not is_ours(path):
                continue
            try:
                text = open(path, encoding='utf-8', errors='ignore').read()
            except OSError:
                continue
            rel = os.path.relpath(path, root)
            for m in DEF_RE.finditer(text):
                defs.setdefault(m.group(1), (rel, text[:m.start()].count('\n') + 1))
                spans[rel].append((m.start(), m.end()))
            for m in CALL_RE.finditer(text):
                # the signature of a definition is not a call site
                if any(a <= m.start() < b for a, b in spans[rel]):
                    continue
                calls[m.group(1)] += 1
    return defs, calls


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--repo', default='.', help='checkout root (scans <root>/src)')
    ap.add_argument('--all-names', action='store_true',
                    help='report every uncalled function, not just probe-named ones')
    args = ap.parse_args()

    root = os.path.join(args.repo, 'src')
    if not os.path.isdir(root):
        print(f'no src/ under {args.repo}', file=sys.stderr)
        return 2

    defs, calls = scan(root)
    dead = [(n, loc) for n, loc in sorted(defs.items()) if calls.get(n, 0) == 0]

    selected = dead if args.all_names else [(n, l) for n, l in dead if PROBE_RE.search(n)]

    print(f'scanned {len(defs)} function definitions in {root}')
    print(f'uncalled everywhere: {len(dead)}'
          f'   (showing {"all" if args.all_names else "probe-named"}: '
          f'{len(selected)})\n')

    print('=== DEAD GUARD CANDIDATES ===')
    if not selected:
        print('  none')
    for name, (rel, line) in selected:
        print(f'  {name:44s} {rel}:{line}')

    print('\nReminder: dispatch tables, parity #defines, cross-repo API and')
    print('callbacks all produce false positives here. Verify before deleting.')
    return 0


if __name__ == '__main__':
    sys.exit(main())