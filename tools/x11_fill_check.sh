#!/usr/bin/env bash
# x11_fill_check.sh -- a controlled rectangle, checked to the pixel.
#
# This is deliberately NOT a real application: xlogo's own traffic mixes background
# clears with shape requests we do not implement, so "xlogo drew something" cannot
# tell you whether the parse is right. One rectangle with known coordinates can.
#
# Every request length is computed from the bytes actually sent. Three separate
# hand-counted lengths in this session were wrong by four bytes, and each one made
# the server answer the first request and then appear to hang.
set -uo pipefail
cd "$(dirname "$0")/.." || exit 1
SOCK=/tmp/.X11-unix/X77
rm -f "$SOCK" /tmp/x11_stub_frame.ppm

gcc -std=c11 -O1 -Isrc/runtime tools/x11_handshake_stub.c \
    src/runtime/wubu_x11_bridge.c src/runtime/wubu_x11_setup_reply.c \
    src/runtime/wubu_x11_request.c src/runtime/wubu_x11_surface.c \
    src/runtime/wubu_x11_windows.c -o build/x11_handshake_stub || {
    echo "x11 fill: FAIL (stub did not build)"; exit 1; }

./build/x11_handshake_stub "$SOCK" >/tmp/x11_fill_stub.log 2>&1 &
STUB=$!
sleep 0.5

python3 - "$SOCK" <<'PY'
import socket, struct, sys, time
sock = sys.argv[1]
def req(op, body):
    total = 4 + len(body)
    assert total % 4 == 0, (op, total)
    return struct.pack('<BBH', op, 0, total // 4) + body

c = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
c.connect(sock)
c.sendall(struct.pack('<BxHHHHxx', ord('l'), 11, 0, 0, 0))
h = c.recv(8); ln = struct.unpack('<H', h[6:8])[0]; b = b''
while len(b) < ln * 4: b += c.recv(ln * 4 - len(b))
c.sendall(req(1, struct.pack('<IIhhhhHHII', 0x500001, 0x400001, 0, 0, 100, 100, 0, 1, 0x400021, 0)))
c.sendall(req(8, struct.pack('<I', 0x500001)))
c.sendall(req(55, struct.pack('<III', 0x600001, 0x500001, 0x4) + struct.pack('<I', 0x00ffffff)))
c.sendall(req(69, struct.pack('<IIhhhh', 0x500001, 0x600001, 10, 10, 20, 30)))
time.sleep(0.6)
c.close()
PY

wait $STUB 2>/dev/null
grep -aE "CREATEGC|FILLRECT" /tmp/x11_fill_stub.log || true

python3 - <<'PY'
import sys
try:
    d = open('/tmp/x11_stub_frame.ppm', 'rb').read()
except OSError:
    print("x11 fill: FAIL (no frame)"); sys.exit(1)
head, px = d.split(b'\n255\n', 1)
w, h = (int(x) for x in head.split(b'\n')[1].split())
def at(x, y):
    o = (y * w + x) * 3
    return tuple(px[o:o+3])
bad = 0
for (x, y, want, label) in [(15, 20, (255,255,255), "inside"),
                            (10, 10, (255,255,255), "top-left corner"),
                            (29, 39, (255,255,255), "last pixel"),
                            (30, 39, (0,0,0), "one past in x"),
                            (0, 0, (0,0,0), "outside")]:
    got = at(x, y)
    ok = got == want
    print(f"  [{'PASS' if ok else 'FAIL'}] pixel({x},{y}) {label}: {got}")
    bad += 0 if ok else 1
if bad: print("x11 fill: FAIL"); sys.exit(1)
print("x11 fill: PASS (PolyFillRectangle executed to the pixel)")
PY
