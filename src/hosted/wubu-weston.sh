#!/bin/sh
# wubu-weston.sh -- the WuBuOS desktop as a scaled window on the host display.
#
# The hosted body is a Wayland client, so it needs a compositor. This starts
# weston (X11 backend, so it is a normal window on the host desktop), waits
# for its socket, then runs WuBuOS inside it at the requested size.
#
#   wubu-weston.sh [WIDTHxHEIGHT]      default 1280x720
#
# The pixman renderer is deliberate: it is a CPU renderer, so the window's
# contents are readable by ordinary X11 screenshot tools. With the GL
# renderer, X11 readback of the window returns black even though the
# compositor is showing the desktop correctly.
#
# Input follows the window: click it and the keyboard and mouse go to the
# hosted desktop through the compositor.
#
# C11 project, shell glue. Self-contained.

set -u

SIZE=${1:-1280x720}
W=${SIZE%x*}
H=${SIZE#*x}

if [ "$W" -lt 320 ] || [ "$H" -lt 200 ]; then
    echo "wubu-weston: size too small (minimum 320x200)" >&2
    exit 1
fi

XDG_RUNTIME_DIR=${XDG_RUNTIME_DIR:-/run/user/$(id -u)}
export XDG_RUNTIME_DIR
export DISPLAY=${DISPLAY:-:0}

SOCKET=wubu-wayland
BIN=$(dirname "$0")/wubu
[ -x "$BIN" ] || BIN=$(command -v wubuos || echo "")

if [ -z "$BIN" ]; then
    echo "wubu-weston: no hosted binary. Build it with 'make hosted' or install it with 'make install'." >&2
    exit 1
fi

command -v weston >/dev/null 2>&1 || {
    echo "wubu-weston: weston is not installed (apt install weston)" >&2
    exit 1
}

# Clean up the compositor however we leave.
cleanup() {
    [ -n "${WPID:-}" ] && kill "$WPID" 2>/dev/null
    rm -f "$XDG_RUNTIME_DIR/$SOCKET" "$XDG_RUNTIME_DIR/$SOCKET.lock"
}
trap cleanup EXIT INT TERM

# A stale socket from a previous run would make weston pick another name.
rm -f "$XDG_RUNTIME_DIR/$SOCKET" "$XDG_RUNTIME_DIR/$SOCKET.lock"

weston --backend=x11-backend.so \
       --renderer=pixman \
       --width="$W" --height="$H" \
       --socket="$SOCKET" \
       --xwayland &
WPID=$!

# Wait for the compositor to publish its socket (about five seconds).
i=0
while [ "$i" -lt 50 ]; do
    [ -S "$XDG_RUNTIME_DIR/$SOCKET" ] && break
    sleep 0.1
    i=$((i + 1))
done
if [ ! -S "$XDG_RUNTIME_DIR/$SOCKET" ]; then
    echo "wubu-weston: weston did not create its socket" >&2
    exit 1
fi

echo "wubu-weston: ${W}x${H} window, WuBuOS inside (click the window to use it)"
WAYLAND_DISPLAY=$SOCKET XDG_RUNTIME_DIR="$XDG_RUNTIME_DIR" \
    "$BIN" -w "$W" "$H"
