#!/bin/sh
# wubu-clamshell-guard.sh -- clamshell mode for the WuBuOS box.
#
# WHAT IT DOES
#   lid closed  -> internal panel off     (xrandr --output eDP-1 --off)
#   lid open    -> internal panel back on  (xrandr --output eDP-1 --auto)
#   always      -> holds a systemd sleep inhibitor, so closing the lid cannot
#                  suspend the machine and the agent keeps working.
#
# WHY USER SPACE, NOT /etc/systemd/logind.conf
#   Changing HandleLidSwitch needs root, and this box has no passwordless sudo.
#   A --mode=block inhibitor on "sleep" + "handle-lid-switch" makes logind honour
#   the block for idle AND lid actions without any privilege -- that is exactly
#   what the inhibitor interface is for.
#
# WHY xrandr AND NOT DPMS
#   The host no-lock guard deliberately disables DPMS.  xrandr --off/--auto is
#   deterministic, independent of the DPMS setting, and reversible, so it does not
#   fight the existing guard.
#
# TESTING HOOKS (used by wubu-clamshell-test.sh, no root needed):
#   WUBU_LID_STATE   path to a file shaped like /proc/acpi/button/lid/LID0/state
#   WUBU_LID_OUTPUT  xrandr output name to act on
#   WUBU_LID_POLL    poll interval seconds (default 1)
#   WUBU_LID_DRY_RUN log the action instead of calling xrandr (no display needed)

set -u

LID_STATE=/proc/acpi/button/lid/LID0/state
OUTPUT=eDP-1
POLL=1
DRY=0
[ -n "${WUBU_LID_STATE:-}" ]   && LID_STATE=$WUBU_LID_STATE
[ -n "${WUBU_LID_OUTPUT:-}" ]  && OUTPUT=$WUBU_LID_OUTPUT
[ -n "${WUBU_LID_POLL:-}" ]    && POLL=$WUBU_LID_POLL
[ -n "${WUBU_LID_DRY_RUN:-}" ] && DRY=$WUBU_LID_DRY_RUN

export DISPLAY=${DISPLAY:-:0}
export XAUTHORITY=${XAUTHORITY:-$HOME/.Xauthority}

# Log to syslog normally; also append to $WUBU_LID_LOG when set, so behaviour can
# be verified on a box where the agent has no access to /var/log/syslog.
log() {
    logger -t wubu-clamshell "$1" 2>/dev/null
    [ -n "${WUBU_LID_LOG:-}" ] && echo "$(date '+%H:%M:%S') $1" >> "$WUBU_LID_LOG"
}

# The output's own preferred mode, read from xrandr's mode list (first entry).
# WHY: `xrandr --output eDP-1 --auto` after an `--off` does NOT reliably restore the
# previous mode -- this panel came back at 320x200, twice, and the whole desktop was a
# postage stamp. Turning the panel back on must re-assert a real mode explicitly.
preferred_mode() {
    xrandr -q 2>/dev/null | awk -v out="$OUTPUT" '
        $1 == out { inblk = 1; next }
        inblk && /^[^[:space:]]/ { exit }
        inblk && $1 ~ /^[0-9]+x[0-9]+$/ { print $1; exit }
    '
}

act() {   # $1 = off|on
    if [ "$DRY" = "1" ]; then
        log "DRY-RUN: xrandr --output $OUTPUT --$([ "$1" = off ] && echo off || echo auto)"
        return 0
    fi
    if [ "$1" = off ]; then
        # NEVER blank the only active display: with no external output there is nothing left
        # to look at, and the panel comes back at 320x200 when it is re-enabled. The lid can
        # report "closed" on a machine whose panel is the display in use.
        CONNECTED=$(xrandr -q 2>/dev/null | grep -c " connected")
        ACTIVE=$(xrandr -q 2>/dev/null | grep -c " connected.*[0-9]x[0-9]*+")
        if [ "${CONNECTED:-0}" -lt 2 ] && [ "${ACTIVE:-0}" -le 1 ]; then
            log "refusing to blank $OUTPUT: it is the only display"
            return 0
        fi
        if xrandr --output "$OUTPUT" --off >/dev/null 2>&1; then
            log "lid $LID_STATE_READ (off): $OUTPUT turned off"
        else
            log "FAILED to set $OUTPUT off"
        fi
        return 0
    fi

    MODE=$(preferred_mode)
    if [ -n "$MODE" ]; then
        xrandr --output "$OUTPUT" --mode "$MODE" >/dev/null 2>&1
        log "lid $LID_STATE_READ (on): $OUTPUT on at $MODE"
    else
        xrandr --output "$OUTPUT" --auto >/dev/null 2>&1
        log "lid $LID_STATE_READ (on): $OUTPUT --auto (no mode parsed)"
    fi
}

# Always defined, even when the state file is missing or unreadable -- act()
# reads it, and under `set -u` an unset variable aborts the whole guard instead of
# degrading to "unknown".  Found under the bare environment systemd gives a user
# unit, where the first poll can happen before any read succeeds.
LID_STATE_READ=unknown

read_lid() {
    # File looks like: "state:\tclosed"  (or open)
    if [ ! -r "$LID_STATE" ]; then
        LID_STATE_READ=unknown
        echo unknown
        return
    fi
    LID_STATE_READ=$(sed -n 's/^state:[[:space:]]*//p' "$LID_STATE" | head -1 | tr -d '[:space:]')
    LID_STATE_READ=${LID_STATE_READ:-unknown}
    echo "$LID_STATE_READ"
}

# NOTE ON THE SLEEP INHIBITOR
#   It is NOT spawned here.  A background `systemd-inhibit ... &` is a child in the
#   unit's cgroup and systemd KILLS the whole cgroup on stop/restart, taking the
#   inhibitor with it -- the lock vanished even though the guard kept running.
#   Instead the unit wraps the guard:
#     ExecStart=/usr/bin/systemd-inhibit --what=sleep:handle-lid-switch \
#         --mode=block --who=wubu-clamshell --why=... this-script
#   so the inhibitor lives exactly as long as the service and no trap is needed.

# --- watch the lid --------------------------------------------------------
prev=$(read_lid)
log "guard started; lid is $prev, output $OUTPUT"

# Act immediately on the starting state so a machine booted with the lid shut
# does not sit with a live panel.
case "$prev" in
    closed) act off ;;
    open)   act on ;;
esac

while :; do
    cur=$(read_lid)
    if [ "$cur" != "$prev" ]; then
        case "$cur" in
            closed) act off ;;
            open)   act on ;;
            # UNKNOWN is not "closed". Treating it as closed is what kept blanking the
            # panel and coming back at 320x200; an unreadable lid must leave the screen alone.
            *)      log "lid state unknown: leaving $OUTPUT untouched" ;;
        esac
        prev=$cur
    fi
    sleep "$POLL"
done
