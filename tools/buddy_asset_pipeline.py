#!/usr/bin/env python3
"""Desktop-pet asset pipeline: chroma key -> normalize -> fixed canvas.

Standard workflow (dsh-pet toolkit): clips are generated on a green screen,
keyed with HSV hue, then normalised to one fixed canvas with the character
centred at a consistent scale, so every clip registers identically in the
player.

  python3 tools/buddy_asset_pipeline.py IN_DIR OUT_DIR [--canvas 2160x1215]
      [--green] [--height-frac 0.72]

--green keys a green screen by HSV hue/saturation; without it the source's own
alpha is kept (correct for the white-on-white reference art, which was keyed by
connectivity -- never by AI saliency).
"""
import argparse
import glob
import os

import numpy as np
from PIL import Image


def chroma_green(rgb, h_lo=70, h_hi=175, s_min=55, v_min=40):
    """HSV green-screen matte. Typical chroma green sits near hue 120-140deg,
    so the band must cover yellow-green through blue-green."""
    hsv = _rgb_to_hsv(rgb)
    h, s, v = hsv[:, :, 0], hsv[:, :, 1], hsv[:, :, 2]
    green = (h_lo <= h) & (h <= h_hi) & (s >= s_min) & (v >= v_min)
    return ~green


def _rgb_to_hsv(rgb):
    """Vectorised RGB->HSV. h in degrees 0..360, s and v in 0..255."""
    arr = rgb.astype(np.float32) / 255.0
    r, g, b = arr[..., 0], arr[..., 1], arr[..., 2]
    mx = arr.max(axis=-1)
    mn = arr.min(axis=-1)
    d = mx - mn
    h = np.zeros_like(mx)
    nz = d > 1e-6
    rd = np.where(nz, (mx - r) / np.where(nz, d, 1), 0.0)
    gd = np.where(nz, (mx - g) / np.where(nz, d, 1), 0.0)
    bd = np.where(nz, (mx - b) / np.where(nz, d, 1), 0.0)
    h = np.where(mx == r, bd - gd, np.where(mx == g, 2.0 + rd - bd, 4.0 + gd - rd))
    h = (h * 60.0) % 360.0
    s = np.where(mx > 1e-6, d / np.where(mx > 1e-6, mx, 1), 0.0) * 255.0
    return np.dstack([h, s, mx * 255.0])


def normalise(im, canvas, height_frac):
    """Scale so opaque height = height_frac*canvas_h, centre on the canvas."""
    cw, ch = canvas
    alpha = im.getchannel("A")
    bb = alpha.getbbox()
    if bb is None:
        return None
    im = im.crop(bb)
    target_h = int(ch * height_frac)
    scale = target_h / im.size[1]
    im = im.resize((max(1, int(im.size[0] * scale)), target_h), Image.LANCZOS)
    out = Image.new("RGBA", (cw, ch), (0, 0, 0, 0))
    out.paste(im, ((cw - im.size[0]) // 2, (ch - im.size[1]) // 2), im)
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("indir")
    ap.add_argument("outdir")
    ap.add_argument("--canvas", default="2160x1215")
    ap.add_argument("--green", action="store_true")
    ap.add_argument("--height-frac", type=float, default=0.72)
    a = ap.parse_args()
    cw, ch = (int(v) for v in a.canvas.lower().split("x"))
    os.makedirs(a.outdir, exist_ok=True)

    n = 0
    for path in sorted(glob.glob(os.path.join(a.indir, "*.png"))):
        im = Image.open(path).convert("RGBA")
        if a.green:
            rgb = np.asarray(im.convert("RGB"))
            alpha = (chroma_green(rgb) * 255).astype(np.uint8)
            im = Image.fromarray(np.dstack([rgb, alpha]), "RGBA")
        out = normalise(im, (cw, ch), a.height_frac)
        if out is None:
            continue
        name = os.path.splitext(os.path.basename(path))[0] + ".png"
        out.save(os.path.join(a.outdir, name))
        n += 1
    print(f"normalised {n} frames -> {a.outdir} (canvas {cw}x{ch})")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
