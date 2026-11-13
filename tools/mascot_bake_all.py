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
}


def bake_pose(sheet_path, cx, cy, hat_w, size):
    """Return RGBA bytes for one pose, cropped and border-flood-filled."""
    im = Image.open(sheet_path).convert("RGB")
    px = im.load()

    side_w = max(8, int(hat_w * 2.4))
    side_h = max(8, int(hat_w * 3.4))
    x0, y0 = cx - side_w // 2, cy - int(hat_w * 0.4)
    x1, y1 = x0 + side_w, y0 + side_h

    box = content_bbox(im, px, x0, y0, x1, y1)
    if box is None:
        raise SystemExit("empty crop around (%d,%d)" % (cx, cy))
    bx0, by0, bx1, by1 = box

    # Pad the trim by a few percent.  The trim hugs the last non-white pixel, so
    # without this the character's extremities sit flush on the sprite edge and
    # read as "cut off" even though nothing is missing -- measured on TRAVEL,
    # SIGN and KEY, all three spanned y 0..95 exactly.
    mw = max(2, int((bx1 - bx0) * 0.04))
    mh = max(2, int((by1 - by0) * 0.04))
    bx0 = max(0, bx0 - mw); by0 = max(0, by0 - mh)
    bx1 = min(im.size[0] - 1, bx1 + mw)
    by1 = min(im.size[1] - 1, by1 + mh)

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
        found = False
        for dy in range(-r, r + 1):
            for dx in range(-r, r + 1):
                nx, ny = cx - bx0 + dx, cy - by0 + dy
                if 0 <= nx < cw and 0 <= ny < ch and not bg[ny * cw + nx]:
                    seed = (nx, ny); found = True; break
            if found:
                break
        if found:
            break
    if seed is not None:
        keep = bytearray(cw * ch)
        stack = [seed]
        keep[seed[1] * cw + seed[0]] = 1
        while stack:
            x, y = stack.pop()
            for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                nx, ny = x + dx, y + dy
                if 0 <= nx < cw and 0 <= ny < ch and not bg[ny * cw + nx] \
                        and not keep[ny * cw + nx] \
                        and (orig[ny * cw + nx] or ink[ny * cw + nx]):
                    keep[ny * cw + nx] = 1
                    stack.append((nx, ny))
        for i in range(cw * ch):
            if not keep[i]:
                bg[i] = 1

    # Canvas is deliberately a little larger than the crop so extremities keep
    # a visible margin on EVERY side; a tall pose would otherwise fill the whole
    # height and its ears/feet would sit flush on the sprite edge.
    side2 = int(max(cw, ch) * 1.08)
    ox, oy = (side2 - cw) // 2, (side2 - ch) // 2
    rgba = Image.new("RGBA", (side2, side2), (255, 255, 255, 0))
    rp = rgba.load()
    for y in range(side2):
        for x in range(side2):
            sx, sy = x - ox, y - oy
            if 0 <= sx < cw and 0 <= sy < ch and not bg[sy * cw + sx]:
                r, g, b = cpx[sx, sy]
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
        sprite = bake_pose(path, cx, cy, hx1 - hx0, args.size)
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