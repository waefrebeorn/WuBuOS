#!/usr/bin/env python3
"""Prepare a generated buddy sprite for the deck.

Input must ALREADY be keyed (see tools/buddy_key_rembg.py -- AI segmentation).
Pipeline here: crop to content -> fit into 96x96 -> requantize to a small
palette -> BINARY alpha.

Binary alpha matters: LANCZOS downscaling leaves ~900 soft edge pixels per
sprite, which breaks exact baking (src/runtime/wubu_buddy_rig_data.h reproduces
the source only when alpha is 0/255).

Usage: buddy_sprite_prep.py IN.png OUT.png [colors]
"""
import sys
from PIL import Image

OUT_W = OUT_H = 96


def main():
    if len(sys.argv) < 3:
        print(__doc__)
        return 2
    src, dst = sys.argv[1], sys.argv[2]
    colors = int(sys.argv[3]) if len(sys.argv) > 3 else 48

    rgba = Image.open(src).convert("RGBA")

    # crop to opaque content
    bb = rgba.getchannel("A").getbbox()
    if bb:
        rgba = rgba.crop(bb)
    cw, ch = rgba.size
    scale = min(OUT_W / cw, OUT_H / ch)
    nw, nh = max(1, int(cw * scale)), max(1, int(ch * scale))
    rgba = rgba.resize((nw, nh), Image.LANCZOS)

    # centre on the 96x96 cell
    canvas = Image.new("RGBA", (OUT_W, OUT_H), (0, 0, 0, 0))
    canvas.paste(rgba, ((OUT_W - nw) // 2, (OUT_H - nh) // 2))

    # requantize colours, then force binary alpha
    r, g, b, a = canvas.split()
    q = Image.merge("RGB", (r, g, b)).quantize(colors=colors, method=Image.MEDIANCUT)
    qr, qg, qb = q.convert("RGB").split()
    ap = a.load()
    for y in range(OUT_H):
        for x in range(OUT_W):
            ap[x, y] = 255 if ap[x, y] > 127 else 0
    out = Image.merge("RGBA", (qr, qg, qb, a))
    out.save(dst)
    n = sum(1 for y in range(OUT_H) for x in range(OUT_W) if ap[x, y])
    print(f"{dst}: {cw}x{ch} -> {nw}x{nh}, {len(q.getpalette())//3} colours, opaque={n}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
