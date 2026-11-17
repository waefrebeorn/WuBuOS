#!/usr/bin/env python3
"""Minimal XWD reader -> PNG.  ffmpeg's xwd decoder mis-parses these captures."""
import struct
import sys
from PIL import Image


def read_xwd(path):
    d = open(path, "rb").read()
    (hsize, ver, fmt, depth, w, h, xoff, order, unit, bitorder, pad,
     bpp, bpl) = struct.unpack(">13I", d[:52])
    if ver != 7:
        raise SystemExit(f"unexpected xwd version {ver}")
    endian = "<" if order == 1 else ">"
    rm, gm, bm = struct.unpack(endian + "3I", d[52:64])
    bpr, vclass, bits_rgb, cmap_n, ncolors = struct.unpack(endian + "5I",
                                                          d[64:84])
    off = hsize + ncolors * 12
    img = Image.new("RGB", (w, h))
    px = img.load()
    for y in range(h):
        row = d[off + y * bpl: off + (y + 1) * bpl]
        for x in range(w):
            i = x * (bpp // 8)
            v = int.from_bytes(row[i:i + bpp // 8],
                               "little" if order == 1 else "big")
            r = (v & rm) >> max(0, (rm.bit_length() - 8)) if rm else 0
            g = (v & gm) >> max(0, (gm.bit_length() - 8)) if gm else 0
            b = (v & bm) >> max(0, (bm.bit_length() - 8)) if bm else 0
            px[x, y] = (r & 255, g & 255, b & 255)
    return img


if __name__ == "__main__":
    im = read_xwd(sys.argv[1])
    im.save(sys.argv[2])
    print(f"{sys.argv[1]} -> {sys.argv[2]} {im.size}")
