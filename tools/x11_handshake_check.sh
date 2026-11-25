#!/usr/bin/env bash
# x11_handshake_check.sh -- prove the setup reply against a REAL X client.
#
# The unit test proves the encoder agrees with the protocol layout as I read it.
# This proves a real client ACCEPTS it: xdpyinfo must get past the handshake and
# send its first request. It then reports "connection broken", which is the pass
# condition -- the stub closes on purpose and does not pretend to be a server.
#
# :77 is used because it is far from any real display; the abstract socket is
# where clients look first, so a tiny proxy bridges abstract -> filesystem.
set -uo pipefail
cd "$(dirname "$0")/.." || exit 1
SOCK=/tmp/.X11-unix/X77
gcc -std=c11 -O1 -Isrc/runtime tools/x11_handshake_stub.c \
    src/runtime/wubu_x11_bridge.c src/runtime/wubu_x11_setup_reply.c \
    src/runtime/wubu_x11_request.c -o build/x11_handshake_stub || exit 1
./build/x11_handshake_stub "$SOCK" >/tmp/x11_stub.log 2>&1 &
STUB=$!
python3 - "$SOCK" <<'PY' &
import socket, select, sys
path = sys.argv[1]
s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
s.bind('\0/tmp/.X11-unix/X77'); s.listen(1)
c, _ = s.accept()
f = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM); f.connect(path)
try:
    while True:
        r, _, _ = select.select([c, f], [], [])
        if c in r:
            d = c.recv(65536)
            if not d: break
            f.sendall(d)
        if f in r:
            d = f.recv(65536)
            if not d: break
            c.sendall(d)
finally:
    c.close(); f.close()
PY
PROXY=$!
sleep 1
DISPLAY=:77 timeout 10 xdpyinfo >/tmp/x11_client.out 2>&1
wait $STUB 2>/dev/null
kill $PROXY 2>/dev/null
cat /tmp/x11_stub.log
# PASS now means the client ran the whole way: every request was answered, none
# was unimplemented, and xdpyinfo printed a screen. "unknown=0" is the strict
# part -- a client that limps through on errors is not a working server.
if grep -q "PASS client accepted the reply and drove" /tmp/x11_stub.log &&
   grep -q "unknown=0" /tmp/x11_stub.log &&
   grep -q "dimensions:" /tmp/x11_client.out; then
    echo "x11 handshake: PASS (a real X client completed against this server)"
    exit 0
fi
echo "x11 handshake: FAIL"
echo "--- client said:"; cat /tmp/x11_client.out
exit 1
