#!/usr/bin/env python3
"""WuBu Buddy FULL RIG + FACIAL RIG (anatomical, unibody-aware).

The canonical mascot is a UNIBODY: arms emerge from the torso sides and legs
from the pelvis -- no pixel gaps, so connected-component separation fuses.
This rig therefore segments by ANATOMICAL REGIONS (a joints map of fractional
boxes over the opaque bbox), matching the reference anatomy:

  ears   top band above/through the cap           (wobble/tilt)
  cap    green band on the forehead               (with RED W)
  head   face circle                             (tilt/bob; carries FACE layers)
  torso  pear body                               (breath squash)
  armL   left outer column of the shoulder band  (shoulder swing)
  armR   right outer column of the shoulder band (shoulder swing)
  legL   bottom-left band                        (hip step)
  legR   bottom-right band                       (hip step)

FACIAL RIG: the head's face box is redrawn per expression with CANONICAL dot
eyes + mouth -- open / blink / happy / wink / sad / wide x smile / open /
frown. Expressions are independent of pose, so any pose x any face.

Every frame writes the sprite's own pixels moved by per-part transforms.
"""
import os
from PIL import Image

DECK = "tools/assets/wubu_buddy_deck"
OUT = os.path.join(DECK, "rig")
CELL = 96
SRC = "canon_idle"

def load(name, size=(96, 96)):
    im = Image.open(os.path.join(DECK, name + ".png")).convert("RGBA")
    return im.resize(size, Image.NEAREST)

def opaque_box(im):
    W, H = im.size
    px = im.load()
    xs = [x for y in range(H) for x in range(W) if px[x, y][3] > 100]
    if not xs:
        return None
    ys = [y for y in range(H) for x in range(W) if px[x, y][3] > 100]
    return min(xs), min(ys), max(xs), max(ys)

# --- anatomical joints: fractional boxes over the opaque bbox -------------
JOINTS = {
    "ears":  (0.00, 0.00, 1.00, 0.22),
    "cap":   (0.22, 0.08, 0.78, 0.30),
    "head":  (0.08, 0.14, 0.92, 0.50),
    "torso": (0.06, 0.48, 0.94, 0.80),
    "legs":  (0.04, 0.78, 0.96, 1.00),
}
ARM_BAND = (0.50, 0.74)      # vertical span (fractions of full height) for arms
ARM_EDGE = 0.24              # outer fraction of torso width treated as arm

def factor(im):
    box = opaque_box(im)
    if not box:
        return None
    x0, y0, x1, y1 = box
    W, H = im.size
    bw, bh = x1 - x0 + 1, y1 - y0 + 1

    def sub(fx0, fy0, fx1, fy1):
        ax = max(0, x0 + int(bw * fx0)); ay = max(0, y0 + int(bh * fy0))
        bx = min(W, x0 + int(bw * fx1)); by = min(H, y0 + int(bh * fy1))
        if bx <= ax or by <= ay:
            return None, (ax, ay)
        return im.crop((ax, ay, bx, by)), (ax, ay)

    parts = {}
    for name, (a, b, c, d) in JOINTS.items():
        img, off = sub(a, b, c, d)
        if img:
            parts[name] = img
            parts[name + "_off"] = off

    # arms = outer columns of the shoulder band, carved from the torso band
    tx0, ty0 = parts["torso_off"]
    tw, th = parts["torso"].size
    ty0f, ty1f = JOINTS["torso"][1], JOINTS["torso"][3]
    band_top = int(th * (ARM_BAND[0] - ty0f) / (ty1f - ty0f))
    band_h = int(th * (ARM_BAND[1] - ARM_BAND[0]) / (ty1f - ty0f))
    edge = max(1, int(tw * ARM_EDGE))
    for side, sx in (("armL", 0), ("armR", tw - edge)):
        arm = parts["torso"].crop((sx, band_top, sx + edge, band_top + band_h))
        parts[side] = arm
        parts[side + "_off"] = (tx0 + sx, ty0 + band_top)

    # legs = bottom band split at centre
    lx0, ly0 = parts["legs_off"]
    lw, lh = parts["legs"].size
    mid = lw // 2
    parts["legL"] = parts["legs"].crop((0, 0, mid, lh))
    parts["legL_off"] = (lx0, ly0)
    parts["legR"] = parts["legs"].crop((mid, 0, lw, lh))
    parts["legR_off"] = (lx0 + mid, ly0)
    parts["box"] = box
    return parts

def _paste(cnv, img, off, dx=0, dy=0):
    cnv.paste(img, (int(off[0] + dx), int(off[1] + dy)), img)

def render_walk(parts, f):
    cnv = Image.new("RGBA", (CELL, CELL), (0, 0, 0, 0))
    f %= 8
    bob = 1 if f % 2 == 0 else 0
    step = 2 if f in (0, 1, 6, 7) else -2
    _paste(cnv, parts["torso"], parts["torso_off"], dy=bob)
    _paste(cnv, parts["head"], parts["head_off"], dy=bob)
    _paste(cnv, parts["cap"], parts["cap_off"], dy=bob)
    _paste(cnv, parts["ears"], parts["ears_off"], dy=bob)
    _paste(cnv, parts["armL"], parts["armL_off"], dy=bob + step)
    _paste(cnv, parts["armR"], parts["armR_off"], dy=bob - step)
    _paste(cnv, parts["legL"], parts["legL_off"], dy=-step)
    _paste(cnv, parts["legR"], parts["legR_off"], dy=+step)
    return cnv

def render_wave(parts, f):
    cnv = Image.new("RGBA", (CELL, CELL), (0, 0, 0, 0))
    f %= 8
    bob = 1 if f % 2 == 0 else 0
    wave = 2 if f in (0, 1, 4, 5) else -2
    tilt = 2 if f in (0, 1, 2) else (-2 if f in (5, 6, 7) else 0)
    _paste(cnv, parts["torso"], parts["torso_off"], dy=bob)
    _paste(cnv, parts["head"], parts["head_off"], dy=bob, dx=tilt)
    _paste(cnv, parts["cap"], parts["cap_off"], dy=bob, dx=tilt)
    _paste(cnv, parts["ears"], parts["ears_off"], dy=bob, dx=tilt)
    _paste(cnv, parts["armL"], parts["armL_off"], dy=bob - wave)
    _paste(cnv, parts["armR"], parts["armR_off"], dy=bob + wave)
    _paste(cnv, parts["legL"], parts["legL_off"])
    _paste(cnv, parts["legR"], parts["legR_off"])
    return cnv

# ----------------------------- FACIAL RIG ---------------------------------
def apply_face(head, expr):
    h = head.copy()
    px = h.load()
    Wf, Hf = h.size
    if Wf < 10 or Hf < 10:
        return h
    ex = Wf // 2
    ey = int(Hf * 0.48)
    gap = max(2, int(Wf * 0.17))
    r_dot = max(1, min(2, Wf // 22))       # canonical small dot
    r_wide = max(2, min(3, Wf // 16))
    dark = (24, 22, 22, 255)
    my = ey + int(Hf * 0.24)
    mw = max(2, int(Wf * 0.14))

    def disc(cx, cy, r):
        for dx in range(-r, r + 1):
            for dy in range(-r, r + 1):
                if dx * dx + dy * dy <= r * r + 1:
                    x, y = cx + dx, cy + dy
                    if 0 <= x < Wf and 0 <= y < Hf:
                        px[x, y] = dark

    def line(cx, cy, half):
        for dx in range(-half, half + 1):
            x, y = cx + dx, cy
            if 0 <= x < Wf and 0 <= y < Hf:
                px[x, y] = dark

    def arc(cx, cy, r, up):
        k = 0.6 if r > 2 else 0.4
        for dx in range(-r, r + 1):
            y = cy - int(round(abs(dx) * k)) if up else cy + int(round(abs(dx) * k))
            x = cx + dx
            if 0 <= x < Wf and 0 <= y < Hf:
                px[x, y] = dark

    # ---- eyes ----
    if expr == "blink":
        line(ex - gap, ey, gap); line(ex + gap, ey, gap)
    elif expr == "happy":
        arc(ex - gap, ey + r_dot, r_dot + 1, up=True)
        arc(ex + gap, ey + r_dot, r_dot + 1, up=True)
    elif expr == "wink":
        line(ex - gap, ey, gap); disc(ex + gap, ey, r_dot)
    else:                                     # open, sad, wide
        r = r_wide if expr == "wide" else r_dot
        disc(ex - gap, ey, r); disc(ex + gap, ey, r)

    # ---- mouth ----
    if expr in ("smile", "happy"):
        arc(ex, my, mw, up=True)              # corners rise -> smile
    elif expr in ("frown", "sad"):
        arc(ex, my, mw, up=False)             # corners drop -> frown
    elif expr == "open":
        for dx in range(-mw // 2, mw // 2 + 1):
            for dy in range(0, 3):
                x, y = ex + dx, my + dy
                if 0 <= x < Wf and 0 <= y < Hf:
                    px[x, y] = dark
    else:
        line(ex, my, mw // 2)
    return h

def render_head(parts, expr):
    cnv = Image.new("RGBA", (CELL, CELL), (0, 0, 0, 0))
    h = apply_face(parts["head"], expr)
    off = parts["head_off"]
    cnv.paste(h, (off[0], off[1]), h)
    return cnv

def strip(rows, cell=CELL):
    cols = max(len(r) for r in rows)
    s = Image.new("RGB", (cols * cell + (cols + 1) * 2,
                          len(rows) * cell + (len(rows) + 1) * 2), (18, 18, 20))
    y = 2
    for row in rows:
        x = 2
        for fr in row:
            s.paste(fr.convert("RGB"), (x, y))
            x += cell + 2
        y += cell + 2
    return s

def main():
    os.makedirs(OUT, exist_ok=True)
    parts = factor(load(SRC))
    if not parts:
        print("no sprite"); return
    walk = [render_walk(parts, f) for f in range(8)]
    wave = [render_wave(parts, f) for f in range(8)]
    faces = [render_head(parts, e) for e in ("open", "blink", "happy",
                                             "wink", "sad", "open")]
    strip([walk, wave]).save(os.path.join(OUT, "body_rig.png"))
    strip([faces]).save(os.path.join(OUT, "face_rig.png"))
    print("parts:", sorted(k for k in parts if isinstance(parts[k], Image.Image)))
    print("saved body_rig (walk+wave) + face_rig (6 expressions)")

if __name__ == "__main__":
    main()