#!/usr/bin/env python3
"""mascot_encode.py -- turn a clipart sheet into a WuBu Buddy sprite.

WuBuOS draws the desktop mascot with vbe_* primitives only: no image
decoder, no asset loader in the WM.  So the clipart is baked into a C table
at build time -- the same discipline as an embedded font.

    tools/mascot_encode.py SHEET.jpg --out src/gui/wubu_buddy_sprite.h \
        --size 96 [--pose 1] [--hat x,y]

The output is a generated asset: do not hand-edit it.
Pure PIL, no numpy.  C11 project, Python glue.

------------------------------------------------------------------------------
BACKGROUND REMOVAL -- read this before changing it

The rabbit's fur is pure #FFFFFF, exactly the same value as the sheet's white
background.  An earlier version of this tool keyed transparency on COLOUR
("any pixel that is white-ish becomes transparent"), which erased the mascot's
body and left only ghostly semi-transparent outlines -- the desktop buddy looked
like a watermark instead of a rabbit.

Colour cannot distinguish the two whites here.  Only CONNECTIVITY can: the fur
is enclosed by the character's black outline, while the background touches the
image border.  So the background is found by flooding inward from the crop's
border and everything the flood cannot reach stays opaque.  The final alpha is
written from THAT mask, never from the pixel value.
------------------------------------------------------------------------------
"""

import argparse
import os
import sys

try:
    from PIL import Image
except ImportError:
    sys.exit("mascot_encode: Pillow is required (python3 -m pip install Pillow)")


def find_hats(im, px, W, H):
    """Locate every green hat on the sheet as a connected cluster.

    Returns a list of (cx, cy, minx, maxx, miny, maxy, n), sorted row-major so
    --pose 1..N addresses them predictably.
    """
    seeds = []
    seen = set()
    for y in range(0, H, 2):
        for x in range(0, W, 2):
            if (x, y) in seen:
                continue
            r, g, b = px[x, y]
            if not (g > 90 and g - r > 25 and g - b > 25):
                continue
            # BFS the cluster
            stack = [(x, y)]
            seen.add((x, y))
            pts = []
            while stack:
                sx, sy = stack.pop()
                pts.append((sx, sy))
                for dx in (-2, 0, 2):
                    for dy in (-2, 0, 2):
                        nx, ny = sx + dx, sy + dy
                        if 0 <= nx < W and 0 <= ny < H and (nx, ny) not in seen:
                            r2, g2, b2 = px[nx, ny]
                            if g2 > 90 and g2 - r2 > 25 and g2 - b2 > 25:
                                seen.add((nx, ny))
                                stack.append((nx, ny))
            if len(pts) < 120:           # noise / a stray green pixel
                continue
            xs = [p[0] for p in pts]
            ys = [p[1] for p in pts]
            seeds.append((sum(xs) // len(xs), sum(ys) // len(ys),
                          min(xs), max(xs), min(ys), max(ys), len(pts)))
    # Merge fragments of the SAME hat.  The white "W" logo splits a hat into two
    # green lobes at this resolution, so the raw component list contains several
    # entries per mascot; anything whose centre is close to another is one pose.
    merged = []
    for s in sorted(seeds, key=lambda z: -z[6]):
        placed = False
        for m in merged:
            if abs(m[0] - s[0]) < 90 and abs(m[1] - s[1]) < 90:
                # keep the larger fragment's box, accumulate the count
                m[2] = min(m[2], s[2]); m[3] = max(m[3], s[3])
                m[4] = min(m[4], s[4]); m[5] = max(m[5], s[5])
                m[6] += s[6]
                m[0] = (m[0] + s[0]) // 2
                m[1] = (m[1] + s[1]) // 2
                placed = True
                break
        if not placed:
            merged.append(list(s))
    # A real hat is a decent fraction of the sheet cell; drop anything too small.
    widest = max((m[3] - m[2]) for m in merged) if merged else 0
    merged = [m for m in merged if (m[3] - m[2]) >= widest * 0.45]
    # Row-major ordering with REAL row clustering.  Rounding cy/200 mis-sorts any
    # sheet whose hats sit at slightly different heights -- one such sheet split its
    # top row across two "rows", which silently renumbered every pose after it.
    rows = []
    for m in sorted(merged, key=lambda z: z[1]):
        for row in rows:
            if abs(row[0] - m[1]) <= 80:      # same visual row
                row[1].append(m)
                break
        else:
            rows.append([m[1], [m]])
    ordered = []
    for _cy, items in rows:
        ordered.extend(sorted(items, key=lambda z: z[0]))
    return [tuple(m) for m in ordered]


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
    ap.add_argument("--pose", type=int, default=None,
                    help="1-based pose index, row-major over the sheet")
    ap.add_argument("--hat", help="x,y of the pose to use (overrides --pose)")
    args = ap.parse_args()

    im = Image.open(args.sheet).convert("RGB")
    W, H = im.size
    px = im.load()

    hats = find_hats(im, px, W, H)
    if not hats:
        sys.exit("mascot_encode: no green hat found in %s" % args.sheet)
    print("poses found: %d" % len(hats))
    for i, h in enumerate(hats, 1):
        print("  pose %d: centroid=(%d,%d) hat_w=%d" %
              (i, h[0], h[1], h[3] - h[2]))

    if args.hat:
        cx, cy = (int(v) for v in args.hat.split(","))
        hat_w = next((h[3] - h[2] for h in hats
                      if abs(h[0] - cx) < 200 and abs(h[1] - cy) < 200),
                     max(h[3] - h[2] for h in hats))
        print("using requested pose at (%d,%d)" % (cx, cy))
    else:
        idx = args.pose if args.pose is not None else 1
        if not 1 <= idx <= len(hats):
            sys.exit("mascot_encode: --pose %d out of range (1..%d)" %
                     (idx, len(hats)))
        cx, cy, hx0, hx1, hy0, hy1, n = hats[idx - 1]
        hat_w = hx1 - hx0
        print("using pose %d at (%d,%d)" % (idx, cx, cy))

    # The mascot is roughly 3.4 hat-widths tall and ~2.4 hat-widths wide, and
    # the hat sits near the top of the body.  Crop a box that contains this pose
    # and nothing beside it.
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

    # The sheet is crowded: neighbouring poses bleed into the crop.  The mascot
    # is enclosed by a black outline, but at this resolution the outline is
    # dashed -- so dilate the ink a little to close the gaps first, then flood
    # the near-white background in from the crop border and keep whatever the
    # flood could not reach.
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

    # Neighbouring poses are closed contours too, so the flood above keeps them.
    # Keep only the solid component the pose itself belongs to: flood the
    # non-background pixels outward from the centroid.
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
                bg[i] = 1                 # treat neighbours as background

    # Square canvas.  Everything NOT reached by the flood stays opaque --
    # including white fur, which is the whole point.
    # Re-trim to the CONNECTED COMPONENT, not raw colour -- see
    # mascot_bake_all.py for why (a neighbouring pose's hat is non-white and
    # would otherwise be swallowed into this sprite).
    kx0, ky0, kx1, ky1 = cw, ch, 0, 0
    found = False
    for yy in range(ch):
        for xx in range(cw):
            if not bg[yy * cw + xx]:
                found = True
                if xx < kx0: kx0 = xx
                if xx > kx1: kx1 = xx
                if yy < ky0: ky0 = yy
                if yy > ky1: ky1 = yy
    if found and (kx1 - kx0) > 4 and (ky1 - ky0) > 4:
        sub = crop.crop((kx0, ky0, kx1 + 1, ky1 + 1))
        scw, sch = sub.size
        subpx = sub.load()
        rgba0 = Image.new("RGBA", (scw, sch), (255, 255, 255, 0))
        rp0 = rgba0.load()
        for yy in range(sch):
            for xx in range(scw):
                if not bg[(yy + ky0) * cw + (xx + kx0)]:
                    r, g, b = subpx[xx, yy]
                    rp0[xx, yy] = (r, g, b, 255)
        crop = rgba0
        cw, ch = scw, sch
    else:
        crop = crop.convert("RGBA")

    side2 = int(max(cw, ch) * 1.08)
    ox = (side2 - cw) // 2
    oy = (side2 - ch) // 2
    rgba = Image.new("RGBA", (side2, side2), (255, 255, 255, 0))
    rp = rgba.load()
    cpx = crop.load()          # crop may have been replaced by the component trim
    for y in range(side2):
        for x in range(side2):
            sx, sy = x - ox, y - oy
            if 0 <= sx < cw and 0 <= sy < ch and cpx[sx, sy][3] > 0:
                r, g, b, _a = cpx[sx, sy]
                rp[x, y] = (r, g, b, 255)

    sprite = rgba.resize((args.size, args.size), Image.LANCZOS)
    assert sprite is not None
    sp = sprite.load()
    assert sp is not None

    out = args.out
    tmp = out + ".tmp"
    with open(tmp, "w") as f:
        f.write("/* Generated by tools/mascot_encode.py -- do not edit. */\n")
        f.write("/* WuBu Buddy, baked from tools/assets/wubu_buddy_sheet.jpg.   */\n")
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

    opaque = sum(1 for i in range(args.size * args.size) if sp[i % args.size, i // args.size][3] > 200)
    print("wrote %s (%dx%d RGBA, %d bytes of table, %d/%d opaque px)" %
          (out, args.size, args.size, args.size * args.size * 4,
           opaque, args.size * args.size))


if __name__ == "__main__":
    main()
