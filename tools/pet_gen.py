#!/usr/bin/env python3
"""pet_gen.py -- deterministic pixel-art pets for the WuBu petdex.

The agent-oriented pixel tools all encode the same discipline: a recipe
determines the pixels, the palette is locked, and the result is inspectable
and patchable. This tool applies that discipline to WuBuOS, which has no
image decoder -- the pets are baked into an RGBA table at build time, the
same way as the mascot.

    tools/pet_gen.py --out src/gui/wubu_petdex.h [--size 24] [--pets 8]

Every pet is:
  * drawn on a SQUARE canvas with a 1px darker outline,
  * lit from one direction with a two-step shading ramp (lit / base / shade),
  * symmetric about the vertical axis unless the spec says otherwise,
  * animated: idle bob, blink, happy (4 frames).

Nothing here reads an image and nothing here needs a model: the same recipe
always produces the same pixels.

Pure PIL. C11 project, Python glue.
"""

import argparse
import os
import sys

try:
    from PIL import Image
except ImportError:
    sys.exit("pet_gen: Pillow is required (python3 -m pip install Pillow)")


# -- the palette -------------------------------------------------------------
# Locked: a pet is body, belly, outline, eye, and one accent. Nothing else.
# The shade is computed from the base so the ramp stays consistent.

def shade(rgb, f):
    return tuple(max(0, min(255, int(c * f))) for c in rgb)


def ramp(base):
    return {
        "lit":   shade(base, 1.25),
        "base":  tuple(base),
        "shade": shade(base, 0.62),
    }


# -- pet recipes -------------------------------------------------------------
# A recipe is a small drawing program over a grid of palette keys:
#   .  transparent   o  outline   l  lit   b  base   s  shade
#   e  eye           w  white     k  accent (scarf / hat)
# The grid is mirrored horizontally so the pet reads as a creature, and the
# eyes are drawn last so they never get covered.

def pet_grid(body):
    """A round-ish quadruped blob, 12 wide x 10 tall, as palette keys."""
    r = ramp(body)
    B, L, S = r["base"], r["lit"], r["shade"]
    return [
        list("....llll...."),
        list("..llBBBBll.."),
        list(".lBBBBBBBBl."),
        list("lBBBBBBBBBBl"),
        list("lBBBBBBBBBBl"),
        list("BBBBBBBBBBBB"),
        list("BBBBBBBBBBBB"),
        list("sBBBBBBBBBBl"),
        list(".sBBBBBBBBS."),
        list("..ssBBBBSS.."),
    ], B


def draw_pet(spec, frame, size):
    """Render one pet to an RGBA image of size x size."""
    grid, body = pet_grid(spec["body"])

    # Frame variations: bob shifts the body, blink closes the eyes,
    # happy lifts the mouth and adds sparkle.
    bob = 0
    blink = False
    happy = False
    if frame == 1:
        bob = 1
    elif frame == 2:
        blink = True
    elif frame == 3:
        happy = True

    h = len(grid)
    w = max(len(row) for row in grid)
    scale = max(1, size // max(w, h))
    pad = (size - w * scale) // 2

    img = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    px = img.load()

    r = ramp(spec["body"])
    accent = tuple(spec.get("accent", (220, 60, 90)))
    ar = ramp(accent)

    for gy, row in enumerate(grid):
        for gx, key in enumerate(row):
            if key == ".":
                continue
            # mirror
            mx = (w - 1 - gx) if spec.get("mirror", True) else gx
            dx = pad + mx * scale
            dy = pad + (gy + bob) * scale
            if key == "o":
                col = (30, 22, 40, 255)
            elif key == "l":
                col = r["lit"] + (255,)
            elif key == "s":
                col = r["shade"] + (255,)
            elif key == "b":
                col = r["base"] + (255,)
            elif key == "k":
                col = ar["base"] + (255,)
            else:
                continue
            for yy in range(scale):
                for xx in range(scale):
                    x, y = dx + xx, dy + yy
                    if 0 <= x < size and 0 <= y < size:
                        px[x, y] = col

    # Eyes: two dark dots with a white catchlight, last so nothing covers them.
    ey = pad + (4 + bob) * scale
    for ex in (pad + 3 * scale, pad + (w - 4) * scale):
        for yy in range(scale):
            for xx in range(scale):
                x, y = ex + xx, ey + yy
                if 0 <= x < size and 0 <= y < size:
                    px[x, y] = (25, 20, 30, 255)
        if not blink and scale >= 1:
            cx, cy = ex, ey
            if 0 <= cx < size and 0 <= cy < size:
                px[cx, cy] = (255, 255, 255, 255)

    # Mouth: a smile, wider when happy.
    my = pad + (6 + bob) * scale
    mw = 2 if happy else 1
    for i in range(-mw, mw + 1):
        x = pad + (w // 2 + i) * scale
        for yy in range(scale):
            for xx in range(scale):
                if 0 <= x + xx < size and 0 <= my + yy < size:
                    px[x + xx, my + yy] = (25, 20, 30, 255)

    # Happy sparkle.
    if happy:
        for sx, sy in ((pad + scale, pad + bob * scale),
                       (pad + (w - 2) * scale, pad + bob * scale)):
            if 0 <= sx < size and 0 <= sy < size:
                px[sx, sy] = (255, 220, 90, 255)

    return img


# -- the petdex --------------------------------------------------------------

PETS = [
    ("WUBU",      (150, 200, 90),  (220, 60, 90)),   # the green buddy
    ("MINT",     (120, 200, 170),  (240, 130, 60)),
    ("BERRY",    (190, 90, 140),   (120, 200, 90)),
    ("COBALT",   (80, 120, 220),   (250, 200, 80)),
    ("EMBER",    (220, 110, 60),   (90, 70, 120)),
    ("FROST",    (170, 210, 235),  (70, 90, 140)),
    ("PLUM",     (150, 100, 180),  (190, 230, 120)),
    ("SUNNY",    (240, 200, 90),   (220, 90, 80)),
]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", required=True)
    ap.add_argument("--size", type=int, default=24)
    ap.add_argument("--pets", type=int, default=len(PETS))
    args = ap.parse_args()

    pets = PETS[:max(1, args.pets)]
    frames = 4

    parts = ["/* Generated by tools/pet_gen.py -- do not edit. */\n",
             "/* The WuBu petdex: deterministic pixel pets, 4 frames each.  */\n",
             "#ifndef WUBU_PETDEX_H\n#define WUBU_PETDEX_H\n\n",
             "#define WUBU_PET_SIZE %d\n" % args.size,
             "#define WUBU_PET_FRAMES %d\n" % frames,
             "#define WUBU_PET_COUNT %d\n\n" % len(pets)]

    for name, body, accent in pets:
        arr = []
        for f in range(frames):
            img = draw_pet({"body": body, "accent": accent}, f, args.size)
            px = img.load()
            flat = []
            for y in range(args.size):
                for x in range(args.size):
                    r, g, b, a = px[x, y]
                    flat.append("0x%02x,0x%02x,0x%02x,0x%02x," % (r, g, b, a))
            arr.append("".join(flat))
        parts.append("static const unsigned char wubu_pet_%s[%d * %d * %d * 4] = {\n"
                     % (name.lower(), frames, args.size, args.size))
        # one line per frame, wrapped
        for i, flat in enumerate(arr):
            parts.append("    /* frame %d */ %s\n" % (i, flat))
        parts.append("};\n\n")

    parts.append("typedef struct {\n")
    parts.append("    const char *name;\n")
    parts.append("    const unsigned char *rgba;\n")
    parts.append("} WubuPet;\n\n")
    parts.append("static const WubuPet wubu_petdex[WUBU_PET_COUNT] = {\n")
    for name, _, _ in pets:
        parts.append('    { "%s", wubu_pet_%s },\n' % (name, name.lower()))
    parts.append("};\n\n#endif /* WUBU_PETDEX_H */\n")

    out = args.out
    tmp = out + ".tmp"
    open(tmp, "w").write("".join(parts))
    os.replace(tmp, out)
    print("wrote %s (%d pets, %d frames, %dx%d)" %
          (out, len(pets), frames, args.size, args.size))


if __name__ == "__main__":
    main()
