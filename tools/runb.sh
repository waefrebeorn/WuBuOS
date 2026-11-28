#!/bin/bash
# runb -- run a command with its output BOUNDED, so a chatty program cannot eat the disk.
#
# Why: two runs wrote 60 GB of log by redirecting unbounded output. Deleting the logs was
# cleanup; this is the habit fix. The kernel enforces the cap (ulimit -f), so a runaway
# writer is killed at the limit instead of after the filesystem fills.
#
# usage:  runb <logfile> [max_mb] -- <command...>
#         runb /tmp/gauntlet.log 200 -- ./gauntlet_v3
#
# max_mb defaults to 200. Exit status is the command's, or 124-ish semantics when the
# kernel killed it for exceeding the cap (which is reported as SIGXFSZ).
set -o pipefail

LOG=${1:?usage: runb <logfile> [max_mb] -- <command...>}
shift
MAX_MB=200
if [[ $1 =~ ^[0-9]+$ ]]; then MAX_MB=$1; shift; fi
[[ $1 == -- ]] && shift
[[ $# -eq 0 ]] && { echo "runb: no command given" >&2; exit 2; }

FREE_GB=$(df -BG --output=avail "$(dirname "$LOG")" | tail -1 | tr -dc '0-9')
if [[ ${FREE_GB:-0} -lt 10 ]]; then
  echo "runb: refusing to start, only ${FREE_GB}G free on $(dirname "$LOG")" >&2
  exit 3
fi

echo "runb: $* -> $LOG (cap ${MAX_MB} MB, ${FREE_GB}G free)"
# bash's ulimit -f counts 1024-byte blocks (measured: 5 MB of *2048 blocks gave a 10 MB file).
# ulimit -c 0 because exceeding the cap raises SIGXFSZ, and a core dump per runaway writer is
# how a 0.5 GB wubu.core appeared next to a 33 GB log.
( ulimit -c 0; ulimit -f $((MAX_MB * 1024)); "$@" ) > "$LOG" 2>&1
rc=$?

SIZE=$(stat -c %s "$LOG" 2>/dev/null || echo 0)
echo "runb: rc=$rc, log $((SIZE / 1024)) KB"
if [[ $rc -eq 153 || $rc -eq 25 ]]; then
  echo "runb: writer hit the ${MAX_MB} MB cap -- its output is unbounded; fix the producer," >&2
  echo "      do not raise this number." >&2
fi
exit $rc
