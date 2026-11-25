#!/usr/bin/env bash
# x11_stub_build.sh -- the ONE place the X11 stub's link set is written down.
#
# The stub used to build from four sources. It now uses six, and when only two of the
# three check scripts were updated the third failed to link with
# "undefined reference to wubu_x11_window_put_image" -- a build break that looks like
# a missing feature. One owner, one line, called by every check.
set -uo pipefail
cd "$(dirname "$0")/.." || exit 1
gcc -std=c11 -O1 -Isrc/runtime tools/x11_handshake_stub.c \
    src/runtime/wubu_x11_bridge.c src/runtime/wubu_x11_setup_reply.c \
    src/runtime/wubu_x11_request.c src/runtime/wubu_x11_surface.c \
    src/runtime/wubu_x11_windows.c -o build/x11_handshake_stub
