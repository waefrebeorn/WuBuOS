#!/usr/bin/env bash
# Run the full suite under a lock file, so "is a suite running?" stops being a guess.
#
# Why this exists: `pgrep -f 'make -k -j24 test'` matches the assistant's OWN shell command line, so
# checking for a running suite by pattern reported BUSY while nothing ran (and the same self-match has
# killed shell sessions three times via pkill -f). A lock file cannot match itself.
#
# Usage:
#   tools/suite.sh            # full suite, log to $LOG (default: scratch/gate.log)
#   tools/suite.sh --check    # exit 0 if free, 3 if a suite holds the lock
set -u
cd "$(dirname "$0")/.." || exit 2

LOCK=.suite.lock
LOG=${SUITE_LOG:-$HOME/.hermes/cache/scratch/gate.log}

if [ "${1:-}" = "--check" ]; then
    if [ -f "$LOCK" ] && kill -0 "$(cat "$LOCK" 2>/dev/null)" 2>/dev/null; then
        echo "BUSY: suite pid $(cat "$LOCK") is running (log: $LOG)"; exit 3
    fi
    echo "FREE"; exit 0
fi

if [ -f "$LOCK" ] && kill -0 "$(cat "$LOCK" 2>/dev/null)" 2>/dev/null; then
    echo "refusing to start: suite pid $(cat "$LOCK") already running"; exit 3
fi

echo $$ > "$LOCK"
trap 'rm -f "$LOCK"' EXIT INT TERM
echo "suite started (pid $$), log -> $LOG"
time make -k -j24 test > "$LOG" 2>&1
rc=$?
echo "MAKE_RC=$rc"
tail -2 "$LOG"
echo "ERRORS=$(grep -acE 'Error [0-9]' "$LOG")"
exit $rc
