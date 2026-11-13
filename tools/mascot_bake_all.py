#!/usr/bin/env python3
"""mascot_bake_all.py -- bake every buddy mood into one C header.

The desktop buddy has more than a dozen moods (working, overheating, error,
success, busy, relaxed, surfing, racing, reading...) but it used to draw the SAME
single sprite for all of them.  This tool composes the individual pose bakes into
a single generated header:

    src/gui/wubu_buddy_sprite.h
        wubu_buddy_rgba[]                  the default pose (back-compat)
        wubu_buddy_mood_rgba[STATE_COUNT]  one pose per mood
        wubu_buddy_mood_size               side length shared by all tables

Each entry below is SHEET_FILE:POSE_INDEX, using the row-major pose numbering that
tools/mascot_encode.py --pose reports.

    tools/mascot_bake_all.py                 # uses the map baked into this file
    tools/mascot_bake_all.py --size 96       # override sprite resolution

The mood ordering MUST match WUBU_BUDDY_STATE_* in wubu_buddy_sprite.h, because
the C side indexes the table by state id.  If a state is missing here it falls
back to the default pose rather than reading past the array.
"""

import argparse
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from mascot_encode import find_hats, content_bbox  # noqa: E402

try:
    from PIL import Image
except ImportError:
    sys.exit("mascot_bake_all: Pillow required")

HERE = os.path.dirname(os.path.abspath(__file__))
ASSETS = os.path.join(HERE, "assets")

# C name -> (sheet key, pose index).  ORDER MATTERS: index i is baked as
# BUDDY_STATE_<ENUM_i>, and wubu_bonzi.c indexes by WUBU_BUDDY_STATE_*, so this
# list must stay in enum order (wubu_bonzi.h).  Verified against a numbered overlay
# of each sheet -- every marker sat on a mascot, none on empty space.
MOOD_POSES = [
    ("DEFAULT",   "emotions:6"),  # standing neutrally -- the desktop idle look
    ("WORK",      "moods:3"),     # typing, coffee mugs
    ("ERROR",     "debug:5"),     # staring at a red X
    ("SUCCESS",   "debug:6"),     # thumbs up, green tick
    ("IDLE",      "ops:5"),       # resting on the rubber duck
    ("OVERHEAT",  "ops:2"),       # extinguishing the smoking server
    ("BUSY",      "ops:4"),       # phone showing 99+
    ("RELAXED",   "moods:6"),     # beach chair, coconut
    ("SURF",      "base:1"),      # surfing the wave
    ("RACE",      "base:3"),      # driving the kart
    ("READ",      "base:6"),      # reading by the bookstack
    ("RUN",       "emotions:1"),  # sprinting, motion lines
    ("CLIMB",     "emotions:2"),  # climbing the ladder (progress/upgrade)
    ("BLOCKED",   "emotions:3"),  # pushing against a wall
    ("CRY",       "emotions:4"),  # tears, hand on forehead
    ("DESPAIR",   "emotions:5"),  # sitting, hugging knees
    ("GAME",      "emotions:7"),  # jumping with a controller
    ("COIN",      "emotions:8"),  # celebrating with a gold coin
    ("FLAG",      "emotions:9"),  # waving the flag (done/deployed)
    ("CAMPFIRE",  "base:4"),      # the previously shipped desktop pose
    ("STUDY",     "life:1"),      # at the desk, puzzled (learning/upgrading)
    ("COOK",      "life:3"),      # apron and pan (building/cooking a package)
    ("LOUNGE",    "life:4"),      # armchair with a book (relaxed, second pose)
    ("HERO",      "life:5"),      # cape and raised fist (deployed / shipped it)
    ("COLD",      "life:6"),      # snow and a warm mug (idle-too-long / cold box)
    ("TRAVEL",    "stickers:6"),  # camera and suitcase (portable / backed up)
    ("BANANA",    "stickers:4"),  # slipped on a peel (comical crash)
    ("EXCITED",   "expressions:2"),  # jumping for joy
    ("BOW",       "expressions:6"),  # eyes closed, thanks / done politely
    ("SIGN",      "signs:1"),     # blank sign -- a message carrier
    ("KEY",       "signs:4"),     # golden key (auth / signing / admin)
    ("STAR",      "signs:5"),     # red star with sparkles (important / favourite)
    ("POINT",     "signs:2"),     # pointing forward (attention)
    ("SEARCH",    "explore:5"),   # magnifying glass, one eye winked (find/inspect)
    ("EXPLORE",   "explore:7"),   # sitting over a spread map (navigate/browse)
]

SHEETS = {
    "base":  "wubu_buddy_sheet.jpg",
    "moods": "wubu_buddy_sheet_moods.jpg",
    "ops":   "wubu_buddy_sheet_ops.jpg",
    "debug": "wubu_buddy_sheet_debug.jpg",
    "emotions": "wubu_buddy_sheet_emotions.jpg",
    "life": "wubu_buddy_sheet_life.jpg",
    "stickers": "wubu_buddy_sheet_stickers.jpg",
    "expressions": "wubu_buddy_sheet_expressions.jpg",
    "signs": "wubu_buddy_sheet_signs.jpg",
    "explore": "wubu_buddy_sheet_explore.jpg",
}


def cell_box(hats, idx, W, H):
    """Bound one pose by the MIDPLANES between it and its neighbours.

    A fixed crop sized from the hat works for an isolated sticker but not for a
    grid sheet: on a 2x3 sheet the 3.4-hat-height box reaches into the next row and
    swallows the top of the pose below -- which is how a partial second rabbit head
    ended up inside the WORK sprite.  Midplanes use the detected hat layout itself,
    so a crop can never cross into another pose's cell.
    """
    cx, cy = hats[idx][0], hats[idx][1]
    left = 0; right = W; top = 0; bottom = H
    for j, h in enumerate(hats):
        if j == idx:
            continue
        dx = abs(h[0] - cx); dy = abs(h[1] - cy)
        if dy < 120:                       # same row -> horizontal midplane
            if h[0] < cx: left  = max(left,  (h[0] + cx) // 2)
            else:         right = min(right, (h[0] + cx) // 2)
        if dx < 160:                       # same column -> vertical midplane
            if h[1] < cy: top    = max(top,    (h[1] + cy) // 2)
            else:         bottom = min(bottom, (h[1] + cy) // 2)
    return left, top, right, bottom


def bake_pose(sheet_path, cx, cy, hat_w, size, hats=None, idx_all=0):
    """Bake one pose to an RGBA sprite.

    Isolation is a three-stage pipeline, and the order matters:

      1. CELL      -- crop the pose's grid cell, bounded by midplanes between the
                      detected hats.  A box sized from the hat alone clips wide
                      poses (WORK sits at a desk with two mugs and a monitor).
      2. FLOOD     -- background is whatever reaches the crop border through
                      non-ink pixels, so enclosed white fur stays opaque.
      3. COMPONENT -- keep only the blob connected to the hat centroid, which
                      drops any neighbour sharing the cell.

    Alpha comes from the mask alone, never from pixel colour: the rabbit's fur is
    the same #FFFFFF as the sheet background, so a colour key would erase it.
    """
    im = Image.open(sheet_path).convert("RGB")
    px = im.load()
    if hats is None:
        hats = find_hats(im, px, *im.size)

    cl, ct, cr, cb = cell_box(hats, idx_all, im.size[0], im.size[1])
    bx0, by0 = cl, ct
    bx1, by1 = cr - 1, cb - 1

    crop = im.crop((bx0, by0, bx1 + 1, by1 + 1))
    cw, ch = crop.size
    cpx = crop.load()

    ink = bytearray(cw * ch)
    for y in range(ch):
        for x in range(cw):
            r, g, b = cpx[x, y]
            if min(r, g, b) < 235:
                ink[y * cw + x] = 1
    orig = bytes(ink)
    for _ in range(1):                      # close dashed outlines
        src = ink
        for y in range(ch):
            for x in range(cw):
                if src[y * cw + x]:
                    for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                        nx, ny = x + dx, y + dy
                        if 0 <= nx < cw and 0 <= ny < ch:
                            ink[ny * cw + nx] = 1

    bg = bytearray(cw * ch)
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

    seed = None
    for r in range(0, max(cw, ch)):
        hit = False
        for dy in range(-r, r + 1):
            for dx in range(-r, r + 1):
                nx, ny = cx - bx0 + dx, cy - by0 + dy
                if 0 <= nx < cw and 0 <= ny < ch and not bg[ny * cw + nx]:
                    seed = (nx, ny); hit = True; break
            if hit:
                break
        if hit:
            break
    if seed is not None:
        def _flood(acceptable):
            k = bytearray(cw * ch)
            st = [seed]
            k[seed[1] * cw + seed[0]] = 1
            while st:
                x, y = st.pop()
                for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                    nx, ny = x + dx, y + dy
                    if 0 <= nx < cw and 0 <= ny < ch and not bg[ny * cw + nx] \
                            and not k[ny * cw + nx] and acceptable(ny * cw + nx):
                        k[ny * cw + nx] = 1
                        st.append((nx, ny))
            return k

        # Prefer UNDILATED ink: dilation closes dashed outlines, but it also
        # bridges a pose to a neighbour whose prop sits close by -- that is how a
        # golden sliver from the treasure pose survived inside WORK.  If going
        # strict fragments the character, fall back to the dilated version.
        total_nonbg = sum(1 for i in range(cw * ch) if not bg[i])
        for acceptable in (orig.__getitem__,
                           lambda i: bool(orig[i] or ink[i])):
            keep = _flood(acceptable)
            if sum(keep) >= total_nonbg * 0.9:
                break
        # Morphological OPEN: erode, then dilate.  A neighbour's prop that touches
        # the pose by a one-pixel bridge (the golden sliver from the treasure pose
        # in WORK) survives both floods; opening severs exactly those thin necks
        # and restores everything thicker.
        def _erode(src):
            out = bytearray(cw * ch)
            for y in range(1, ch - 1):
                for x in range(1, cw - 1):
                    i = y * cw + x
                    if src[i] and src[i - 1] and src[i + 1] and src[i - cw] and src[i + cw]:
                        out[i] = 1
            return out

        def _dilate(src):
            out = bytearray(src)
            for y in range(ch):
                for x in range(cw):
                    i = y * cw + x
                    if src[i]:
                        for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                            nx, ny = x + dx, y + dy
                            if 0 <= nx < cw and 0 <= ny < ch:
                                out[ny * cw + nx] = 1
            return out

        opened = _dilate(_erode(keep))
        if sum(opened) >= sum(keep) * 0.5:      # only accept a sane result
            keep = opened

        for i in range(cw * ch):
            if not keep[i]:
                bg[i] = 1

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
        tight = Image.new("RGBA", (scw, sch), (255, 255, 255, 0))
        tp = tight.load()
        for yy in range(sch):
            for xx in range(scw):
                if not bg[(yy + ky0) * cw + (xx + kx0)]:
                    r, g, b = subpx[xx, yy]
                    tp[xx, yy] = (r, g, b, 255)
        crop = tight
        cw, ch = scw, sch
    else:
        crop = crop.convert("RGBA")

    side2 = int(max(cw, ch) * 1.08)
    ox, oy = (side2 - cw) // 2, (side2 - ch) // 2
    rgba = Image.new("RGBA", (side2, side2), (255, 255, 255, 0))
    rp = rgba.load()
    cpx = crop.load()
    for y in range(side2):
        for x in range(side2):
            sx, sy = x - ox, y - oy
            if 0 <= sx < cw and 0 <= sy < ch and cpx[sx, sy][3] > 0:
                r, g, b, _a = cpx[sx, sy]
                rp[x, y] = (r, g, b, 255)

    return rgba.resize((size, size), Image.LANCZOS)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default=os.path.join(HERE, "..", "src", "gui",
                                                  "wubu_buddy_states.h"))
    ap.add_argument("--size", type=int, default=96)
    args = ap.parse_args()

    cache = {}
    baked = []
    for cname, spec in MOOD_POSES:
        sheet_key, idx = spec.split(":")
        idx = int(idx)
        if sheet_key not in cache:
            path = os.path.join(ASSETS, SHEETS[sheet_key])
            im = Image.open(path)
            hats = find_hats(im, im.load(), *im.size)
            cache[sheet_key] = (path, hats)
            print("%-6s %d poses" % (sheet_key, len(hats)))
        path, hats = cache[sheet_key]
        if not 1 <= idx <= len(hats):
            raise SystemExit("%s: pose %d out of range (1..%d)" %
                             (spec, idx, len(hats)))
        cx, cy, hx0, hx1, hy0, hy1, n = hats[idx - 1]
        sprite = bake_pose(path, cx, cy, hx1 - hx0, args.size, hats, idx - 1)
        baked.append((cname, sprite))
        opaque = sum(1 for px in sprite.getdata() if px[3] > 200)
        print("  BUDDY_STATE_%-9s <- %s:%d  (%d/%d opaque)" %
              (cname, sheet_key, idx, opaque, args.size * args.size))

    tmp = args.out + ".tmp"
    with open(tmp, "w") as f:
        f.write("/* Generated by tools/mascot_bake_all.py -- do not edit. */\n")
        f.write("/* WuBu Buddy states, baked from tools/assets/*.jpg.           */\n")
        f.write("/* Background is removed by BORDER FLOOD FILL, never by colour: */\n")
        f.write("/* the rabbit's fur is the same white as the sheet background. */\n")
        f.write("#ifndef WUBU_BUDDY_STATES_H\n#define WUBU_BUDDY_STATES_H\n\n")
        f.write("#define BUDDY_STATE_SIZE %d\n\n" % args.size)
        for cname, img in baked:
            f.write("static const unsigned char BUDDY_STATE_%s[%d * %d * 4] = {\n" %
                    (cname, args.size, args.size))
            write_table(f, img, args.size)
        f.write("\n#endif /* WUBU_BUDDY_STATES_H */\n")
    os.replace(tmp, args.out)
    print("wrote %s (%d states, %dx%d each)" %
          (args.out, len(baked), args.size, args.size))


def write_table(f, img, size):
    row = []
    for i in range(size * size):
        r, g, b, a = img.getpixel((i % size, i // size))
        row.append("0x%02x,0x%02x,0x%02x,0x%02x," % (r, g, b, a))
        if len(row) == 4:
            f.write("    " + "".join(row) + "\n")
            row = []
    if row:
        f.write("    " + "".join(row) + "\n")
    f.write("};\n")


if __name__ == "__main__":
    main()