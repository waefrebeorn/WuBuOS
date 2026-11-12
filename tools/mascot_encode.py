#!/usr/bin/env python3
"""mascot_encode.py -- turn a clipart sheet into a WuBu Buddy sprite.

WuBuOS draws the desktop mascot with vbe_* primitives only: no image
decoder, no asset loader in the WM.  So the clipart is baked into a C table
at build time -- the same discipline as an embedded font.

This tool takes one pose out of a sticker sheet (the mascot's green hat is
the anchor -- every pose wears one), trims it, makes the white background
transparent, resizes it, and emits an RGBA8888 table.

    tools/mascot_encode.py SHEET.jpg --out src/gui/wubu_buddy_sprite.h \
        --size 96 [--hat x,y]

The output is a generated asset: do not hand-edit it.
Pure PIL, no numpy.  C11 project, Python glue.
"""

import argparse
import os
import sys

try:
    from PIL import Image
except ImportError:
    sys.exit("mascot_encode: Pillow is required (python3 -m pip install Pillow)")


def green_hat(im, px, W, H):
    """Find one mascot: cluster green pixels, take the largest cluster.

    A sticker sheet has several poses, each wearing a green hat.  Grid
    clustering by 120px buckets separates them; the largest cluster is the
    pose we bake.
    """
    buckets = {}
    for y in range(0, H, 2):
        for x in range(0, W, 2):
            r, g, b = px[x, y]
            if g > 90 and g - r > 25 and g - b > 25:
                buckets.setdefault((x // 120, y // 120), []).append((x, y))
    if not buckets:
        return None
    pts = max(buckets.values(), key=len)
    xs = [p[0] for p in pts]
    ys = [p[1] for p in pts]
    cx = sum(xs) // len(xs)
    cy = sum(ys) // len(ys)
    return cx, cy, min(xs), max(xs), min(ys), max(ys), len(pts)


def content_bbox(im, px, x0, y0, x1, y1, thresh=235):
    """Trim to non-white content inside the box."""
    minx, miny, maxx, maxy = x1, y1, x0, y0
    found = False
    for y in range(max(0, y0), min(im.size[1], y1 + 1)):
        for x in range(max(0, x0), min(im.size[0], x1 + 1)):
            r, g, b = px[x, y]
            if min(r, g, b) < thresh:
                found = True
                if x < minx: minx = x
                if x > maxx: maxx = x
                if y < miny: miny = y
                if y > maxy: maxy = y
    if not found:
        return None
    return minx, miny, maxx, maxy


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("sheet")
    ap.add_argument("--out", required=True)
    ap.add_argument("--size", type=int, default=96)
    ap.add_argument("--hat", help="x,y of the pose to use (default: largest hat cluster)")
    args = ap.parse_args()

    im = Image.open(args.sheet).convert("RGB")
    W, H = im.size
    px = im.load()

    hat = green_hat(im, px, W, H)
    if hat is None:
        sys.exit("mascot_encode: no green hat found in %s" % args.sheet)
    cx, cy, hx0, hx1, hy0, hy1, n = hat
    hat_w = hx1 - hx0
    print("hat cluster: n=%d centroid=(%d,%d) w=%d" % (n, cx, cy, hat_w))

    if args.hat:
        cx, cy = (int(v) for v in args.hat.split(","))
        print("using requested pose at (%d,%d)" % (cx, cy))

    # The mascot is roughly 3.2 hat-widths tall and ~2.4 hat-widths wide,
    # and the hat sits near the top of the body.  Crop a box that contains
    # this pose and nothing beside it.
    side_w = max(8, int(hat_w * 2.4))
    side_h = max(8, int(hat_w * 3.4))
    x0 = cx - side_w // 2
    y0 = cy - int(hat_w * 0.4)
    x1 = x0 + side_w
    y1 = y0 + side_h

    box = content_bbox(im, px, x0, y0, x1, y1)
    if box is None:
        sys.exit("mascot_encode: empty crop around (%d,%d)" % (cx, cy))
    bx0, by0, bx1, by1 = box
    print("content bbox: %d,%d - %d,%d (%dx%d)" %
          (bx0, by0, bx1, by1, bx1 - bx0 + 1, by1 - by0 + 1))

    crop = im.crop((bx0, by0, bx1 + 1, by1 + 1))

    # The sheet is crowded: neighbouring poses bleed into the crop.  The
    # mascot is enclosed by a black outline, but at this resolution the
    # outline is dashed -- so dilate the ink a little to close the gaps
    # first, then flood the near-white background in from the crop border
    # and keep whatever the flood could not reach.
    cw, ch = crop.size
    cpx = crop.load()
    ink = bytearray(cw * ch)         # 1 = ink (outline / colour), 0 = white
    for y in range(ch):
        for x in range(cw):
            r, g, b = cpx[x, y]
            if min(r, g, b) < 235:
                ink[y * cw + x] = 1
    orig = bytes(ink)                # connectivity uses the true ink
    for _ in range(1):               # close dashed outlines
        src = ink
        for y in range(ch):
            for x in range(cw):
                if src[y * cw + x]:
                    for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                        nx, ny = x + dx, y + dy
                        if 0 <= nx < cw and 0 <= ny < ch:
                            ink[ny * cw + nx] = 1
    bg = bytearray(cw * ch)          # 1 = background, 0 = mascot
    stack = []
    for x in range(cw):
        for y in (0, ch - 1):
            if not ink[y * cw + x] and not bg[y * cw + x]:
                bg[y * cw + x] = 1; stack.append((x, y))
    for y in range(ch):
        for x in (0, cw - 1):
            if not ink[y * cw + x] and not bg[y * cw + x]:
                bg[y * cw + x] = 1; stack.append((x, y))
    while stack:
        x, y = stack.pop()
        for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
            nx, ny = x + dx, y + dy
            if 0 <= nx < cw and 0 <= ny < ch and not bg[ny * cw + nx] and not ink[ny * cw + nx]:
                bg[ny * cw + nx] = 1
                stack.append((nx, ny))
    for i in range(cw * ch):
        if bg[i]:
            cpx[i % cw, i // cw] = (255, 255, 255)

    # Neighbouring poses are closed contours too, so the flood above keeps
    # them.  Keep only the solid component the pose itself belongs to: flood
    # the non-background pixels outward from the centroid.
    seed = None
    for r in range(0, max(cw, ch)):
        found = False
        for dy in range(-r, r + 1):
            for dx in range(-r, r + 1):
                nx, ny = cx - bx0 + dx, cy - by0 + dy
                if 0 <= nx < cw and 0 <= ny < ch and not bg[ny * cw + nx]:
                    seed = (nx, ny); found = True; break
            if found: break
        if found: break
    if seed is not None:
        keep = bytearray(cw * ch)
        stack = [seed]
        keep[seed[1] * cw + seed[0]] = 1
        while stack:
            x, y = stack.pop()
            for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                nx, ny = x + dx, y + dy
                if 0 <= nx < cw and 0 <= ny < ch and not bg[ny * cw + nx] and not keep[ny * cw + nx] \
                        and (orig[ny * cw + nx] or ink[ny * cw + nx]):
                    keep[ny * cw + nx] = 1
                    stack.append((nx, ny))
        for i in range(cw * ch):
            if not keep[i]:
                cpx[i % cw, i // cw] = (255, 255, 255)

    # Square canvas, white -> transparent, then resize with the alpha.
    side2 = max(cw, ch)
    canvas = Image.new("RGB", (side2, side2), (255, 255, 255))
    canvas.paste(crop, ((side2 - cw) // 2, (side2 - ch) // 2))
    rgba = canvas.convert("RGBA")
    rp = rgba.load()
    for y in range(side2):
        for x in range(side2):
            r, g, b, a = rp[x, y]
            if min(r, g, b) >= 240:          # white background
                rp[x, y] = (r, g, b, 0)
            elif min(r, g, b) >= 225:        # near-white: soften the edge
                rp[x, y] = (r, g, b, 96)

    sprite = rgba.resize((args.size, args.size), Image.LANCZOS)
    sp = sprite.load()

    out = args.out
    tmp = out + ".tmp"
    with open(tmp, "w") as f:
        f.write("/* Generated by tools/mascot_encode.py -- do not edit. */\n")
        f.write("/* WuBu Buddy, baked from the official clipart sheet.       */\n")
        f.write("#ifndef WUBU_BUDDY_SPRITE_H\n#define WUBU_BUDDY_SPRITE_H\n\n")
        f.write("#define WUBU_BUDDY_SPRITE_SIZE %d\n\n" % args.size)
        f.write("static const unsigned char wubu_buddy_rgba[%d * %d * 4] = {\n" %
                (args.size, args.size))
        row = []
        for i in range(args.size * args.size):
            r, g, b, a = sp[i % args.size, i // args.size]
            row.append("0x%02x,0x%02x,0x%02x,0x%02x," % (r, g, b, a))
            if len(row) == 4:
                f.write("    " + "".join(row) + "\n")
                row = []
        if row:
            f.write("    " + "".join(row) + "\n")
        f.write("};\n\n#endif /* WUBU_BUDDY_SPRITE_H */\n")
    os.replace(tmp, out)
    print("wrote %s (%dx%d RGBA, %d bytes of table)" %
          (out, args.size, args.size, args.size * args.size * 4))


if __name__ == "__main__":
    main()
