#!/usr/bin/env bash
# run-wubucompositor.sh -- boot the WuBuOS desktop on OUR compositor.
#
# Slerm'd compositor: /home/wubu/wubucompositor (waefrebeorn/wubucompositor),
# a hand-written Wayland compositor on libwayland-server + Xlib (no wlroots).
# It opens a 1024x768 window in the current display and auto-launches the
# wubu hosted client fullscreen, rendering with present-on-change (no flash).
#
# Usage:
#   tools/run-wubucompositor.sh            # build + run
#   tools/run-wubucompositor.sh -w 960 540 # window size override
#
set -euo pipefail

WUBUOS_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
COMP_DIR="${WUBUCOMPOSITOR_DIR:-/home/wubu/wubucompositor}"
BIN="$COMP_DIR/wubucompositor"

W=${1:-1024}
H=${2:-768}
if [ "$1" = "-w" ]; then W="${2:-1024}"; H="${3:-768}"; fi

# settle a clean desktop first: no stale weston / client fighting over X
pkill -f 'wubucompositor$' 2>/dev/null || true
pkill -f 'hosted/wubu' 2>/dev/null || true
# a SIGKILL'd compositor leaves its wayland socket; clear stale ones so
# add_socket_auto names fresh
rm -f "${XDG_RUNTIME_DIR:-/run/user/$(id -u)}"/wayland-* 2>/dev/null || true

if [ ! -x "$BIN" ]; then
    make -C "$COMP_DIR" >/dev/null
fi
if [ ! -x "$BIN" ]; then
    echo "WuBuCompositor not built ($BIN); run make -C $COMP_DIR" >&2
    exit 1
fi

echo "WuBuCompositor: booting WuBuOS desktop ${W}x${H} ..."
cd "$COMP_DIR"
# the compositor forks the client itself (WUBU_START_CMD override supported)
exec env DISPLAY="${DISPLAY:-:0}" "$BIN"
