#!/usr/bin/env python3
"""Generate JPEG fixtures with Pillow, plus the pixels Pillow says they contain.

The oracle is deliberately foreign: Pillow encodes the file and Pillow decodes it back to
raw RGB. Our decoder never sees the encoder. A baseline decoder cannot be bit-exact with
libjpeg's IDCT (the spec allows several), so the gate compares within a stated tolerance
and every fixture writes its own tolerance with it.
"""
import os, sys
from PIL import Image

out = sys.argv[1] if len(sys.argv) > 1 else os.environ.get("TMPDIR", "/tmp")
os.makedirs(out, exist_ok=True)

def gradient(w, h, colour=True):
    im = Image.new("RGB" if colour else "L", (w, h))
    px = im.load()
    for y in range(h):
        for x in range(w):
            if colour:
                px[x, y] = (x * 255 // max(1, w - 1), y * 255 // max(1, h - 1), (x * y) % 251)
            else:
                px[x, y] = (x + y) * 255 // max(1, w + h - 2)
    # a hard-edged block, so a decoder that blurs or shifts is visible
    for y in range(h // 4, h // 2):
        for x in range(w // 4, w // 2):
            px[x, y] = (255, 0, 0) if colour else 255
    return im

cases = [
    ("colour_420.jpg", gradient(64, 48), {"quality": 90, "subsampling": 2}),
    ("colour_444.jpg", gradient(56, 40), {"quality": 90, "subsampling": 0}),
    ("grey_400.jpg", gradient(48, 40, False).convert("L"), {"quality": 90}),
]
for name, im, kw in cases:
    path = os.path.join(out, name)
    im.save(path, "JPEG", **kw)
    raw = Image.open(path).convert("RGB")
    raw.save(os.path.join(out, name + ".rgb"), "PPM")  # PPM: header + raw RGB, parsed by the gate
    print(f"wrote {path}")

# a progressive file: must be REFUSED, not mis-decoded
gradient(40, 32).save(os.path.join(out, "progressive.jpg"), "JPEG", progressive=True)
print("wrote progressive.jpg (must be refused)")

# a truncated file: the entropy-coded tail removed
data = open(os.path.join(out, "colour_420.jpg"), "rb").read()
open(os.path.join(out, "truncated.jpg"), "wb").write(data[: len(data) // 2])
print("wrote truncated.jpg (must be refused by name)")

open(os.path.join(out, "notjpeg.jpg"), "wb").write(b"this is not a JPEG at all")
print("wrote notjpeg.jpg")
