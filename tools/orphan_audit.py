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
    mk = open(os.path.join(ROOT, 'mk', 'tests.mk')).read()
    site = set(re.findall(r'^(test_[a-z0-9_]+):', mk, re.M))
    reached = set()
    for line in mk.split('\n'):
        m = re.match(r'^([a-z0-9_.-]+):\s*(.*)$', line)
        if not m:
            continue
        names = re.findall(r'(?<![\w$()/.-])(test_[a-z0-9_]+)(?![\w])', m.group(2))
        if len(names) >= 5:            # an aggregate (test_all, a tier, ...)
            reached |= set(names)
    return sorted(site - reached - {'test_all'})


def run_one(target, timeout, logdir):
    log = os.path.join(logdir, target + '.log')
    t0 = time.time()
    try:
        p = subprocess.run(['make', target], cwd=ROOT, timeout=timeout,
                           stdout=open(log, 'w'), stderr=subprocess.STDOUT)
        rc = p.returncode
    except subprocess.TimeoutExpired:
        return target, 'TIMEOUT', time.time() - t0
    secs = time.time() - t0
    if rc == 0:
        return target, 'PASS', secs
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

    names = orphans()
    print(f'{len(names)} orphan test targets; {a.jobs} in flight, {a.timeout}s each')

    rows = []
    with cf.ThreadPoolExecutor(max_workers=a.jobs) as ex:
        futs = [ex.submit(run_one, t, a.timeout, logdir) for t in names]
        for i, f in enumerate(cf.as_completed(futs), 1):
            target, result, secs = f.result()
            rows.append((target, result, secs))
            print(f'  [{i:3}/{len(names)}] {result:7} {secs:6.1f}s  {target}', flush=True)

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
