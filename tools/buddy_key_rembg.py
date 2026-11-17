#!/usr/bin/env python3
"""AI background removal for WuBu Buddy sprites -- real alpha matting.

Uses rembg (u2netp, the ~4 MB lightweight model) instead of colour flood fill.
Flood fill is a hack: it eats dark outline pixels and smears edges when the
source has any gradient; segmentation gives a clean, defensible cutout.

Run:  ~/opt/rembg/bin/python tools/buddy_key_rembg.py OUT_DIR IN.png [IN.png ...]
Writes OUT_DIR/<stem>.keyed.png  (RGBA, background transparent).

The downstream tools/buddy_sprite_prep.py turns a keyed sprite into the final
96x96 deck PNG (crop, fit, requantize, binary alpha).
"""
import os
import sys

MODEL = "u2netp"          # lightweight (~4 MB); isnet-general-use is heavier


def key_one(session, src, dst):
    from rembg import remove
    from PIL import Image
    im = Image.open(src).convert("RGB")
    out = remove(im, session=session)
    out.save(dst)
    return out.size


def main():
    if len(sys.argv) < 3:
        print(__doc__)
        return 2
    outdir, inputs = sys.argv[1], sys.argv[2:]
    os.makedirs(outdir, exist_ok=True)
    try:
        from rembg import new_session
    except ImportError:
        sys.stderr.write(
            "rembg not installed. Create it once:\n"
            "  virtualenv -p /usr/bin/python3.12 ~/opt/rembg\n"
            "  ~/opt/rembg/bin/pip install rembg onnxruntime\n"
            "then run with ~/opt/rembg/bin/python\n")
        return 3
    session = new_session(MODEL)
    rc = 0
    for src in inputs:
        stem = os.path.splitext(os.path.basename(src))[0]
        dst = os.path.join(outdir, stem + ".keyed.png")
        try:
            size = key_one(session, src, dst)
            print(f"{dst}: {size[0]}x{size[1]}")
        except Exception as e:                       # fail-closed per file
            print(f"FAIL {src}: {e}")
            rc = 1
    return rc


if __name__ == "__main__":
    sys.exit(main())
