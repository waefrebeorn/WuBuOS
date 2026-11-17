#!/usr/bin/env bash
# reap_stale_tests.sh -- kill test binaries that outlived their run, and the
# orphan GLX windows they leave behind.
#
# Why this exists: the contract tests are short-lived, but a hung/stale build
# (old binary name, or a run wedged on GPU init) sits for DAYS holding an X
# connection, which pins an unmapped 340x250 window open in the WM and looks
# like a swarm of zombie processes. Nothing else in the tree reaps them.
#
# Usage:
#   tools/dev/reap_stale_tests.sh            # report + kill stale runs/windows
#   tools/dev/reap_stale_tests.sh --dry-run  # only report
#
# A test process is "stale" when it matches the test binary names AND has been
# alive longer than AGE_MINUTES (default 10).

set -uo pipefail

AGE_MINUTES="${WUBU_REAP_AGE_MIN:-10}"
DRY=0
[ "${1:-}" = "--dry-run" ] && DRY=1

PATTERN='(vsl_nt_contract_test|wubu_vsl_nt_contract_test|wubu_vsl_nt_contract_test\.san|.*_test(\.san)?$|gauntlet|wubu-buddy|wubucompositor)'

killed=0
while read -r pid args; do
    [ "$pid" = "$$" ] && continue
    # age in minutes from etimes (seconds)
    et=$(ps -o etimes= -p "$pid" 2>/dev/null | tr -d ' ')
    [ -z "$et" ] && continue
    age=$(( et / 60 ))
    [ "$age" -lt "$AGE_MINUTES" ] && continue
    if [ "$DRY" = "1" ]; then
        echo "would kill pid=$pid age=${age}m args=${args:0:80}"
    else
        kill -9 "$pid" 2>/dev/null && { echo "killed pid=$pid age=${age}m args=${args:0:80}"; killed=$((killed+1)); }
    fi
done < <(ps -eo pid=,args= | awk -v p="$PATTERN" '$0 ~ p {print $1, substr($0, index($0,$2))}')

# Orphan GLX windows left by dead clients (our orb/compositor demo windows).
closed=0
if [ -n "${DISPLAY:-}" ] && command -v xwininfo >/dev/null 2>&1; then
    while read -r wid; do
        [ "$DRY" = "1" ] && { echo "would close window $wid"; continue; }
        DISPLAY="$DISPLAY" xkill -id "$wid" >/dev/null 2>&1 && closed=$((closed+1))
    done < <(DISPLAY="$DISPLAY" xwininfo -root -tree 2>/dev/null \
             | grep -E "340x250|320x200" | grep -oE "0x[0-9a-f]+" | sort -u)
fi

echo "reaped ${killed} stale process(es), ${closed} orphan window(s)"
