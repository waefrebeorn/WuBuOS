#!/bin/bash
# gauntlet_sample -- the compiler gauntlet's deterministic stratified sample, as a gate.
#
# 4,431 tests x 14 targets is a full-day run, so the full gauntlet cannot be a tier. Every 20th
# test (222 tests, all thirteen corpora) is deterministic to the digit across isolated runs and
# takes 3 s, which is cheap enough to run on every release.
#
# The gate fails when the sample gets WORSE than the committed baseline: fewer passes, more
# failures, or a lower valid pass rate. It never accepts a better number silently -- a real
# improvement rewrites tools/gauntlet_baseline.json deliberately, by a human, in a commit.
set -o pipefail

COMP=${COMP:-$(cd "$(dirname "$0")/../src/compiler" && pwd -P)}
BASE=${BASE:-$(cd "$(dirname "$0")/.." && pwd -P)/tools/gauntlet_baseline.json}
STRIDE=${WUBU_GAUNTLET_STRIDE:-20}

cd "$COMP" || { echo "gauntlet_sample: FAIL -- no compiler tree at $COMP"; exit 1; }

if [ ! -x ./gauntlet_v3 ]; then
    echo "gauntlet_sample: building gauntlet_v3"
    make gauntlet_v3 > /tmp/gauntlet_sample_build.log 2>&1 || {
        echo "gauntlet_sample: FAIL -- build failed, see /tmp/gauntlet_sample_build.log"
        tail -5 /tmp/gauntlet_sample_build.log
        exit 1
    }
fi

LINE=$(WUBU_GAUNTLET_STRIDE="$STRIDE" ./gauntlet_v3 2>/dev/null | grep -a valid_pass_rate | tail -1)
if [ -z "$LINE" ]; then
    echo "gauntlet_sample: FAIL -- no result line; the runner produced nothing to judge"
    exit 1
fi

python3 - "$LINE" "$BASE" <<'PY'
import json, sys
line, base_path = sys.argv[1], sys.argv[2]
now = json.loads(line)
try:
    base = json.load(open(base_path))
except Exception as e:
    print('gauntlet_sample: FAIL -- no readable baseline at %s (%s)' % (base_path, e))
    sys.exit(1)

print('gauntlet_sample: stride %s -> %s' % (base.get('stride'), line.strip()))
bad = []
if now['pass'] < base['pass']:
    bad.append('pass %d < baseline %d' % (now['pass'], base['pass']))
if now['fail'] > base['fail']:
    bad.append('fail %d > baseline %d' % (now['fail'], base['fail']))
if now.get('valid_pass_rate', 0) + 1e-9 < base['valid_pass_rate']:
    bad.append('valid_pass_rate %.1f < baseline %.1f' % (now['valid_pass_rate'], base['valid_pass_rate']))
if now.get('total', 0) != base.get('total', now.get('total')):
    bad.append('corpus size changed: total %s vs baseline %s (re-baseline deliberately)'
               % (now.get('total'), base.get('total')))

if bad:
    print('gauntlet_sample: FAIL')
    for b in bad:
        print('  ' + b)
    print('  the sample is deterministic, so this is a real regression, not noise.')
    sys.exit(1)
print('gauntlet_sample: PASS -- no regression against baseline')
PY
