#!/bin/sh
# WuBuOS Launch Script
# Usage: wubu-run [options]
#   (no args)  — X11 window with Win98 desktop
#   -h         — Headless mode (Styx server only)
#   -t         — Temple REPL full-screen

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
WUBU_BIN="${SCRIPT_DIR}/bin/wubu"

if [ ! -x "$WUBU_BIN" ]; then
    echo "Error: wubu binary not found at $WUBU_BIN"
    exit 1
fi

# Set namespace if available
if [ -f "${SCRIPT_DIR}/namespace/root.ns" ]; then
    export WUBU_NAMESPACE="${SCRIPT_DIR}/namespace/root.ns"
fi

exec "$WUBU_BIN" "$@"
