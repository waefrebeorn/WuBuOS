#!/usr/bin/env python3
"""Convert the raw RGBA frame dumped by dos_shim_proof into a PNG."""
import struct, zlib, sys

def main():
    path = sys.argv[1] if len(sys.argv) > 1 else "/tmp/dos_shim_proof.rgba"
    out  = sys.argv[2] if len(sys.argv) > 2 else "/tmp/dos_shim_proof.png"
    with open(path, "rb") as f:
        w = struct.unpack("<i", f.read(4))[0]
        h = struct.unpack("<i", f.read(4))[0]
        rgba = f.read()
    assert len(rgba) == w * h * 4, (len(rgba), w*h*4)
    # RGBA -> RGB, add a 1px black border so the 640x400 window is visible on any bg
    B = 8
    W, H = w + 2*B, h + 2*B
    raw = bytearray()
    def px(x, y):
        if B <= x < B+w and B <= y < B+h:
            i = ((y-B)*w + (x-B)) * 4
            return rgba[i], rgba[i+1], rgba[i+2]
        return (0, 0, 0)
    for y in range(H):
        raw.append(0)  # filter type 0
        for x in range(W):
            r, g, b = px(x, y)
            raw.append(r); raw.append(g); raw.append(b)
    def chunk(typ, data):
        c = typ + data
        return struct.pack(">I", len(data)) + c + struct.pack(">I", zlib.crc32(c) & 0xffffffff)
    sig = b"\x89PNG\r\n\x1a\n"
    ihdr = struct.pack(">IIBBBBB", W, H, 8, 2, 0, 0, 0)
    idat = zlib.compress(bytes(raw), 9)
    with open(out, "wb") as f:
        f.write(sig + chunk(b"IHDR", ihdr) + chunk(b"IDAT", idat) + chunk(b"IEND", b""))
    print(f"wrote {out} ({W}x{H})")

if __name__ == "__main__":
    main()
