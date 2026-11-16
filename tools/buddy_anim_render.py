#!/usr/bin/env python3
"""Render real animation frames from the created WuBu Buddy pose sprites.

Rig-lite: the buddy is a blob mascot (no separable limbs to swing), so motion
is whole-sprite + squash/stretch + bob + angle, composited with per-frame
offsets -- the desktop-pets/2D-anim-primer approach -- PLUS crossfade pose
transitions (the 'animated transition between poses' requirement).

Usage:
  python3 tools/buddy_anim_render.py            # renders tools/assets/wubu_buddy_deck/strips/*.png
"""
import os, sys
from PIL import Image

DECK = "tools/assets/wubu_buddy_deck"
OUT = os.path.join(DECK, "strips")

def load(name, size=(96, 96)):
    p = os.path.join(DECK, name + ".png")
    im = Image.open(p).convert("RGBA")
    return im.resize(size, Image.NEAREST)

def transform(frame, dx=0, dy=0, sx=1.0, sy=1.0, alpha=1.0, angle=0):
    """Apply a squash/stretch (sx,sy about center), offset, and alpha to a
    96x96 RGBA frame. Renders into a fresh transparent canvas."""
    from PIL import Image as I
    W = frame.size[0]
    cnv = I.new("RGBA", frame.size, (0, 0, 0, 0))
    # scale about center
    import math
    if angle:
        frame = frame.rotate(angle, resample=I.BICUBIC, center=(W // 2, W // 2))
    w, h = frame.size
    nw, nh = int(w * sx), int(h * sy)
    if nw < 1: nw = 1
    if nh < 1: nh = 1
    scaled = frame.resize((nw, nh), I.NEAREST)
    ox = (W - nw) // 2 + dx
    oy = (W - nh) // 2 + dy
    if alpha < 1.0:
        r, g, b, a = scaled.split()
        a = a.point(lambda v: int(v * alpha))
        scaled = I.merge("RGBA", (r, g, b, a))
    cnv.paste(scaled, (ox, oy), scaled)
    return cnv

def compose(strip, frame, at):
    """Paste an RGBA frame (in-place) preserving transparency via its own alpha."""
    strip.paste(frame, at, frame)

def idle_strip(n_frames=10, fps_label="10"):
    frames = []
    # breath: gentle squash/stretch cycle (1.2s-ish): sy 0.985..1.015
    for i in range(n_frames):
        t = i / float(n_frames - 1)
        sy = 1.0 + 0.02 * (((t * 2) % 2) - 1.0)   # triangle 0.98..1.02
        sx = 1.0 - 0.02 * (((t * 2) % 2) - 1.0)
        dy = int(-10 * (((t * 2) % 2) - 1.0))       # head bob slightly
        frames.append(transform(load("idle"), sx=sx, sy=sy, dy=dy))
    return frames

def walk_strip(n=8):
    # bob + slight wobble alternation (waddle), lean 2px
    out = []
    for i in range(n):
        dy = -3 if (i % 2) == 0 else 0
        sx = 1.0 + (0.02 if (i % 2) == 1 else -0.02)
        out.append(transform(load("walk"), dy=dy, sx=sx, sy=1.0, angle=1 if i % 2 else -1))
    return out

def transition(a_name, b_name, n=8):
    """Crossfade + squash from pose A to B (animated transition)."""
    A, B = load(a_name), load(b_name)
    out = []
    for i in range(n):
        t = i / float(n - 1)
        ease = t * t * (3 - 2 * t)                    # smoothstep
        sy = 1.0 + 0.05 * (1 - ease)                   # squish at onset
        fa = transform(A, sx=1.0 - ease * 0.04, sy=sy, alpha=1.0 - ease)
        fb = transform(B, sx=1.0 - (1 - ease) * 0.04, sy=1.0, alpha=ease)
        # blend
        from PIL import Image as I
        W = A.size[0]
        blend = I.new("RGBA", (N := A.size[0], N))
        blend.paste(fa, (0, 0), fa)
        blend.paste(fb, (0, 0), fb)
        out.append(blend)
    return out

def make_strip(rows, label=None, cell=96, gap=2):
    cols = max(len(r) for r in rows)
    h = sum(cell + gap for _ in rows) + gap
    w = cols * cell + (cols + 1) * gap
    strip = Image.new("RGBA", (w, h), (16, 16, 16))
    y = gap
    for row in rows:
        x = gap
        for fr in row:
            strip.paste(fr, (x, y), fr)
            x += cell + gap
        y += cell + gap
    return strip

def main():
    os.makedirs(OUT, exist_ok=True)
    strips = {
        "idle_breath": idle_strip(),
        "walk_waddle": walk_strip(),
        "transition_idle_to_work": transition("idle", "work"),
        "transition_idle_to_celebrate": transition("idle", "celebrate"),
    }
    for name, frames in strips.items():
        strip = make_strip([frames])
        out = os.path.join(OUT, name + ".png")
        strip.save(out)
        print(f"{name}: {len(frames)} frames -> {out}")
    # also a combined contact sheet of all strips
    sheet = make_strip(list(strips.values()))
    out = os.path.join(OUT, "all_strips.png")
    sheet.save(out)
    print("combined ->", out)

if __name__ == "__main__":
    main()
