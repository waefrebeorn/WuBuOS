#!/usr/bin/env bash
# x11_draw_check.sh -- a guest draws into a window, and the pixels are checked.
#
# The handshake gate proves a client can CONNECT. This proves it can DRAW: a raw
# protocol client (no Xlib) creates a window, maps it, sends PutImage, and the
# frame is dumped to a PPM and read back at exact coordinates.
#
# The coordinate checks are the whole point. "a file was produced" says nothing;
# pixel(5,4) red, pixel(12,9) red and pixel(13,9) black says the rectangle landed
# where the client put it and stopped where it stopped.
set -uo pipefail
cd "$(dirname "$0")/.." || exit 1
SOCK=/tmp/.X11-unix/X77
PPM=/tmp/x11_stub_frame.ppm
rm -f "$SOCK" "$PPM"

gcc -std=c11 -O1 -Isrc/runtime tools/x11_handshake_stub.c \
    src/runtime/wubu_x11_bridge.c src/runtime/wubu_x11_setup_reply.c \
    src/runtime/wubu_x11_request.c src/runtime/wubu_x11_surface.c \
    src/runtime/wubu_x11_windows.c -o build/x11_handshake_stub || {
    echo "x11 draw: FAIL (stub did not build)"; exit 1; }

./build/x11_handshake_stub "$SOCK" >/tmp/x11_draw_stub.log 2>&1 &
STUB=$!
sleep 0.5

python3 - "$SOCK" <<'PY'
import socket, struct, sys, time
sock = sys.argv[1]
c = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
c.connect(sock)
c.sendall(struct.pack('<BxHHHHxx', ord('l'), 11, 0, 0, 0))
hdr = c.recv(8)
ln = struct.unpack('<H', hdr[6:8])[0]
body = b''
while len(body) < ln * 4:
    body += c.recv(ln * 4 - len(body))
# CreateWindow: 4 + 28 bytes = 8 words
c.sendall(struct.pack('<BBH', 1, 0, 8) +
          struct.pack('<IIhhhhHHII', 0x400010, 0x400001, 10, 10, 40, 30, 0, 1, 0x400021, 0))
# MapWindow: 2 words
c.sendall(struct.pack('<BBHI', 8, 0, 2, 0x400010))
# PutImage: 8x6 red at (5,4), depth 24, ZPixmap
px = b''.join(struct.pack('<I', 0x00ff0000) for _ in range(8 * 6))
c.sendall(struct.pack('<BBHIIHHHHBBH', 72, 0, (24 + len(px)) // 4,
                      0x400010, 0x400000, 8, 6, 5, 4, 0, 24, 0) + px)
time.sleep(0.6)
c.close()
PY

wait $STUB 2>/dev/null
cat /tmp/x11_draw_stub.log

python3 - <<'PY'
import sys
try:
    d = open('/tmp/x11_stub_frame.ppm', 'rb').read()
except OSError:
    print("x11 draw: FAIL (no frame dumped)"); sys.exit(1)
head, px = d.split(b'\n255\n', 1)
w, h = (int(x) for x in head.split(b'\n')[1].split())
problems = []
def at(x, y):
    o = (y * w + x) * 3
    return tuple(px[o:o+3])
for (x, y, want, label) in [(5, 4, (255,0,0), "inside the rect"),
                            (12, 9, (255,0,0), "last pixel of the rect"),
                            (13, 9, (0,0,0), "one past the rect"),
                            (0, 0, (0,0,0), "outside the rect")]:
    got = at(x, y)
    ok = got == want
    print(f"  [{'PASS' if ok else 'FAIL'}] pixel({x},{y}) {label}: {got}")
    if not ok: problems.append((x, y, got, want))
if problems:
    print("x11 draw: FAIL"); sys.exit(1)
print(f"x11 draw: PASS (guest drew into a {w}x{h} window; pixels verified)")
PY
