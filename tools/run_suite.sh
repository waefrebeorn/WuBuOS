#!/usr/bin/env bash
# run_suite.sh -- ONE suite at a time, enforced by a lock rather than by memory.
#
# Three times in one session two or three `make -k -j24 test` runs overlapped: the
# walls went to 982 s and 1200 s against a healthy 410-430 s, gates failed under
# contention, and the failures looked like code defects for a while. The wall clock
# was the only tell, and telling yourself "remember to check" does not work.
#
# So the check is mechanical: take an exclusive lock. A second invocation WAITS (or
# refuses with --nowait) instead of starting a second fleet.
set -uo pipefail
cd "$(dirname "$0")/.." || exit 1
# A lock only helps if every launcher takes it, and older invocations do not. So
# also look for a fleet that is ALREADY running before taking the lock: the failure
# mode this guards against is a stale suite from an earlier command, not a
# concurrent call of this script.
if pgrep -f '^make -k -j24 test$' >/dev/null 2>&1; then
    if [ "${1:-}" = "--nowait" ]; then
        echo "run_suite: a suite is already running (pgrep) -- refusing to start a second fleet"
        exit 3
    fi
    echo "run_suite: a suite is already running -- waiting for it to finish..."
    while pgrep -f '^make -k -j24 test$' >/dev/null 2>&1; do sleep 5; done
fi

LOCK=${WUBU_SUITE_LOCK:-/tmp/wubu-suite.lock}
exec 9>"$LOCK"
if [ "${1:-}" = "--nowait" ]; then
    if ! flock -n 9; then
        echo "run_suite: another suite holds $LOCK -- refusing to start a second fleet"
        exit 3
    fi
    shift
else
    echo "run_suite: waiting for $LOCK (another suite is running)..."
    flock -w 600 9 || { echo "run_suite: lock not obtained in 600s"; exit 3; }
fi
unset XDG_RUNTIME_DIR
START=$(date +%s%N)
LOG=${WUBU_SUITE_LOG:-/tmp/full-suite.log}
# 9>&- : the lock fd must NOT be inherited by the suite. When a wrapper was killed
# (or its shell exited), make and its -j24 children kept fd 9 open, so the lock stayed
# held by dead work and every later launch queued behind it -- the 3600-second wait
# that produced "lock not obtained" on a machine with no suite running at all.
flock -w 60 9 || true
make -k -j24 test 9>&- >"$LOG" 2>&1
RC=$?
END=$(date +%s%N)
printf "FULL_GATE_EXIT=%d wall=%.1fs errors=" "$RC" "$(echo "($END-$START)/1000000000" | bc -l)"
grep -acE '^make.*\*\*\*' "$LOG"
grep -aE '^make.*\*\*\*' "$LOG" | head -3
tail -1 "$LOG" | strings
exit $RC
