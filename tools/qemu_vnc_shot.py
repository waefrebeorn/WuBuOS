#!/usr/bin/env python3
"""qemu_vnc_shot.py -- grab QEMU's *real* guest framebuffer over VNC (RFB 3.8).

QEMU's monitor `screendump` under `-display none` returns a 640x480 dummy
surface even when the guest has programmed a 1024x768 VBE mode, so it cannot
verify a graphics path. This speaks just enough RFB to read the actual
framebuffer the display produces.

Usage:
  qemu-system-x86_64 -display vnc=127.0.0.1:9 ...
  tools/qemu_vnc_shot.py 127.0.0.1 5909 out.ppm
"""
import socket
import struct
import sys


def recv_exact(sock, n):
    buf = b""
    while len(buf) < n:
        chunk = sock.recv(n - len(buf))
        if not chunk:
            raise EOFError("vnc closed")
        buf += chunk
    return buf


def grab(host, port, path, timeout=20):
    s = socket.create_connection((host, port), timeout=timeout)
    s.settimeout(timeout)

    ver = recv_exact(s, 12)
    if not ver.startswith(b"RFB "):
        raise RuntimeError(f"not RFB: {ver!r}")
    s.sendall(b"RFB 003.008\n")

    n_types = recv_exact(s, 1)[0]
    types = recv_exact(s, n_types)
    if 1 not in types:                     # 1 = None
        raise RuntimeError(f"server requires auth; offered {list(types)}")
    s.sendall(bytes([1]))

    # 3.8 sends SecurityResult even for None.
    result = struct.unpack(">I", recv_exact(s, 4))[0]
    if result != 0:
        raise RuntimeError(f"security failed: {result}")

    s.sendall(bytes([1]))                  # ClientInit, shared

    w, h = struct.unpack(">HH", recv_exact(s, 4))
    recv_exact(s, 16)                      # server pixel format
    name_len = struct.unpack(">I", recv_exact(s, 4))[0]
    name = recv_exact(s, name_len)

    # Ask for 32bpp truecolour, red-shift 16 so bytes come back B,G,R,X.
    s.sendall(struct.pack(">Bxxx", 0) + struct.pack(
        ">BBBBHHHBBBxxx", 32, 24, 0, 1, 255, 255, 255, 16, 8, 0))
    s.sendall(struct.pack(">BxH", 2, 1) + struct.pack(">i", 0))   # raw encoding
    s.sendall(struct.pack(">BBHHHH", 3, 0, 0, 0, w, h))

    out = bytearray(w * h * 4)
    filled = 0
    while filled < h * w:
        msg = recv_exact(s, 1)[0]
        if msg == 0:                       # FramebufferUpdate
            recv_exact(s, 1)
            nrect = struct.unpack(">H", recv_exact(s, 2))[0]
            for _ in range(nrect):
                x, y, rw, rh = struct.unpack(">HHHH", recv_exact(s, 8))
                enc = struct.unpack(">i", recv_exact(s, 4))[0]
                if enc != 0:
                    raise RuntimeError(f"unexpected encoding {enc}")
                data = recv_exact(s, rw * rh * 4)
                for row in range(rh):
                    src = row * rw * 4
                    dst = ((y + row) * w + x) * 4
                    out[dst:dst + rw * 4] = data[src:src + rw * 4]
                filled += rw * rh
        elif msg == 1:                     # SetColourMapEntries
            recv_exact(s, 3)
            n = struct.unpack(">H", recv_exact(s, 2))[0]
            recv_exact(s, n * 6)
        elif msg == 2:                     # Bell
            pass
        elif msg == 3:                     # ServerCutText
            recv_exact(s, 3)
            ln = struct.unpack(">I", recv_exact(s, 4))[0]
            recv_exact(s, ln)
        else:
            raise RuntimeError(f"unexpected server message {msg}")

    s.close()

    rgb = bytearray(w * h * 3)
    for i in range(w * h):
        rgb[3 * i + 0] = out[4 * i + 2]
        rgb[3 * i + 1] = out[4 * i + 1]
        rgb[3 * i + 2] = out[4 * i + 0]
    with open(path, "wb") as f:
        f.write(b"P6\n%d %d\n255\n" % (w, h))
        f.write(bytes(rgb))
    return w, h, name.decode("latin-1", "replace")


if __name__ == "__main__":
    host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
    port = int(sys.argv[2]) if len(sys.argv) > 2 else 5909
    path = sys.argv[3] if len(sys.argv) > 3 else "/tmp/vnc.ppm"
    w, h, name = grab(host, port, path)
    print(f"{w}x{h} from '{name}' -> {path}")
