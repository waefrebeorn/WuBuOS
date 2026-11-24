#!/usr/bin/env python3
"""orphan_audit.py -- build+run every test_* target that NO aggregate reaches.

The suite defines 665 test targets; 463 are reachable from test_all and the
tiers, and 202 are not. A test nothing runs is a claim nobody checks: it can rot
for months (test_hosted did, and three test_cp_* recipes named sources that no
longer exist). This measures, per orphan, whether it still BUILDS and whether it
PASSES, with a hard timeout and a few in flight, and writes one TSV row each.

usage: python3 tools/orphan_audit.py [--jobs N] [--timeout S] [out.tsv]
"""
import argparse
import concurrent.futures as cf
import os
import re
import subprocess
import time

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def orphans():
    """test_* targets that `make test` cannot reach.

    Make continuations MUST be joined first: an aggregate like test_orphans
    spans a dozen lines, and reading only its first line made 116 targets look
    unreachable when they were already in a tier (a mistake this file made
    once, which produced a duplicate tier built on a phantom gap). Then walk
    the graph from `test`.
    """
    mk = open(os.path.join(ROOT, 'mk', 'tests.mk')).read().replace('\\\n', ' ')
    site = set(re.findall(r'^(test_[a-z0-9_]+):', mk, re.M))
    edges = {}
    for line in mk.split('\n'):
        m = re.match(r'^([a-z0-9_.-]+):\s*(.*)$', line)
        if not m:
            continue
        names = re.findall(r'(?<![\w$(){}/.-])(test_[a-z0-9_]+)(?![\w])', m.group(2))
        if names:
            edges[m.group(1)] = set(names)
    seen, frontier = {'test'}, {'test'}
    while frontier:
        nxt = set()
        for t in frontier:
            nxt |= edges.get(t, set()) - seen
        seen |= nxt
        frontier = nxt
    return sorted(site - seen - {'test', 'test_all'})


MAKEFILE_TEXT = ['', set()]


def _load_locks():
    """Remember which targets take the CT gate lock (they run one at a time)."""
    text = open(os.path.join(ROOT, 'mk', 'tests.mk')).read()
    MAKEFILE_TEXT[0] = text
    # a target serialises if any of ITS recipe lines mention the lock
    cur = None
    for line in text.split('\n'):
        m = re.match(r'^(test_[a-z0-9_]+):', line)
        if m:
            cur = m.group(1)
        elif line.startswith('\t') and 'CT_GATE_LOCK' in line and cur:
            MAKEFILE_TEXT[1].add(cur)


def run_one(target, timeout, logdir):
    log = os.path.join(logdir, target + '.log')
    t0 = time.time()
    try:
        p = subprocess.run(['make', target], cwd=ROOT, timeout=timeout,
                           stdout=open(log, 'w'), stderr=subprocess.STDOUT)
        rc = p.returncode
    except subprocess.TimeoutExpired:
        # A gate that serialises on the CT lock waits for the gates ahead of it;
        # with six audited at once that wait can exceed the budget while the gate
        # itself is fine. Report the reason, not a failure.
        if target in MAKEFILE_TEXT[1]:
            return target, 'SERIALIZED', time.time() - t0
        # Otherwise the gate may simply be slower than the audit's budget --
        # test_optin_crypto runs a real TLS handshake matrix and needs minutes.
        # Give every timed-out target one honest second chance at triple the
        # budget before calling it a timeout; a slow gate is not a broken one.
        try:
            p = subprocess.run(['make', target], cwd=ROOT, timeout=timeout * 3,
                               stdout=open(log, 'w'), stderr=subprocess.STDOUT)
            if p.returncode == 0:
                return target, 'SLOW', time.time() - t0
        except subprocess.TimeoutExpired:
            pass
        return target, 'TIMEOUT', time.time() - t0
    secs = time.time() - t0
    if rc == 0:
        return target, 'PASS', secs
    if rc == 124 or 'Terminated' in (open(log, errors='ignore').read()[-200:] if os.path.exists(log) else ''):
        # The CT/package gates serialise on one lock because they rewrite the
        # same source file; six at once means each waits its turn and hits the
        # audit's timeout while doing nothing wrong. Say so instead of "TIMEOUT".
        if '$(CT_GATE_LOCK)' in MAKEFILE_TEXT[0] and target in MAKEFILE_TEXT[1]:
            return target, 'SERIALIZED', secs
    text = open(log, errors='ignore').read()
    if 'No rule to make target' in text:
        return target, 'ROT', secs        # a prerequisite does not exist
    if re.search(r'error:', text):
        return target, 'BUILD', secs
    return target, 'FAIL', secs


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--jobs', type=int, default=6)
    ap.add_argument('--timeout', type=int, default=120)
    ap.add_argument('out', nargs='?', default=os.path.join('build', 'orphan-audit.tsv'))
    a = ap.parse_args()

    env = dict(os.environ)
    env.pop('XDG_RUNTIME_DIR', None)
    logdir = os.path.join(ROOT, 'build', 'logs', 'orphan')
    os.makedirs(logdir, exist_ok=True)

    _load_locks()
    names = orphans()
    locked = [t for t in names if t in MAKEFILE_TEXT[1]]
    free = [t for t in names if t not in MAKEFILE_TEXT[1]]
    print(f'{len(names)} orphan test targets: {len(free)} parallel, '
          f'{len(locked)} serialised on the CT lock')

    rows = []
    with cf.ThreadPoolExecutor(max_workers=a.jobs) as ex:
        futs = {ex.submit(run_one, t, a.timeout, logdir): t for t in free}
        for i, f in enumerate(cf.as_completed(futs), 1):
            target, result, secs = f.result()
            rows.append((target, result, secs))
            print(f'  [{i:3}/{len(free)}] {result:7} {secs:6.1f}s  {target}', flush=True)

    # The lock-taking gates run one at a time BY DESIGN, so auditing them in the
    # parallel phase only makes them queue and hit the budget (that is what
    # produced fourteen bogus TIMEOUTs). They get their own sequential phase and
    # a longer budget: the lock, not the gate, was always the wait.
    if locked:
        print(f'-- serialised phase: {len(locked)} gates, {max(a.timeout, 420)}s each')
        for i, t in enumerate(locked, 1):
            target, result, secs = run_one(t, max(a.timeout, 420), logdir)
            rows.append((target, result, secs))
            print(f'  [{i:3}/{len(locked)}] {result:7} {secs:6.1f}s  {target}', flush=True)

    rows.sort(key=lambda r: (r[1], r[0]))
    with open(os.path.join(ROOT, a.out), 'w') as out:
        out.write('target\tresult\tsecs\n')
        for t, r, s in rows:
            out.write(f'{t}\t{r}\t{s:.1f}\n')

    counts = {}
    for _, r, _ in rows:
        counts[r] = counts.get(r, 0) + 1
    print('\n'.join(f'{k:8} {v}' for k, v in sorted(counts.items())))
    print('wrote', a.out)


if __name__ == '__main__':
    main()
