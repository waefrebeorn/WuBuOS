#!/usr/bin/env python3
"""Slice the WuBu Buddy reference sheets into individual sprite PNGs.

The supplied reference sheets are professional mascot grids: several poses per
sheet on a plain background.  We segment them with connected components, MERGE
nearby fragments into one pose group (a floating 'Zzz' or a prop belongs to the
pose next to it), crop each group tightly, and key the background out with an AI
matting model (rembg -- no colour flood fill).

Run with the rembg venv python (it carries numpy/scipy/PIL/rembg):
  ~/opt/rembg/bin/python tools/buddy_slice_sheets.py OUT_DIR SHEET.jpg [...]

Output: OUT_DIR/<sheet-stem>__<NN>.png  (RGBA, transparent background)
"""
import os
import sys

import numpy as np
from PIL import Image
from scipy import ndimage

WHITE_MIN = 245        # pixel counts as "paper" if every channel >= this
MIN_AREA = 250         # ignore dust/specks smaller than this
BIG_AREA = 6000        # a component this large is a POSE SEED
SEED_MERGE_GAP = 25    # seeds closer than this are one pose (car+driver, bed+sleeper)


def pose_groups(mask):
    """Return tight boxes around each pose, row-major.

    Seeds are the large connected components.  Two failure modes matter:
      * a pose whose parts don't touch (car + driver, bed + sleeper) -> separate
        components that must FUSE (bbox gap <= SEED_MERGE_GAP);
      * props and detached bits ('Zzz', a floating tool) -> fragments that must
        ATTACH to their nearest pose.
    Distinct poses sit ~57 px apart on these sheets, which is why naive
    proximity merging fused whole sheets."""
    lab, n = ndimage.label(mask)
    if n == 0:
        return []
    objs = ndimage.find_objects(lab)
    comps = []
    for i, sl in enumerate(objs):
        if sl is None:
            continue
        ys, xs = sl
        area = int((lab[sl] == i + 1).sum())
        if area < MIN_AREA:
            continue
        comps.append(dict(box=[xs.start, ys.start, xs.stop, ys.stop],
                          area=area,
                          cx=(xs.start + xs.stop) / 2.0,
                          cy=(ys.start + ys.stop) / 2.0))

    seeds = [c for c in comps if c["area"] >= BIG_AREA]
    frags = [c for c in comps if c["area"] < BIG_AREA]
    if not seeds:                       # fallback: everything is a pose
        seeds, frags = comps, []

    # ---- fuse seeds that are really one pose ----
    parent = list(range(len(seeds)))

    def find(a):
        while parent[a] != a:
            parent[a] = parent[parent[a]]
            a = parent[a]
        return a

    def gap(b1, b2):
        dx = max(0, max(b1[0], b2[0]) - min(b1[2], b2[2]))
        dy = max(0, max(b1[1], b2[1]) - min(b1[3], b2[3]))
        return max(dx, dy)

    for i in range(len(seeds)):
        for j in range(i + 1, len(seeds)):
            if gap(seeds[i]["box"], seeds[j]["box"]) <= SEED_MERGE_GAP:
                ri, rj = find(i), find(j)
                if ri != rj:
                    parent[rj] = ri

    clusters = {}
    for i, s in enumerate(seeds):
        clusters.setdefault(find(i), []).append(s["box"])
    groups = [[min(b[0] for b in bs), min(b[1] for b in bs),
               max(b[2] for b in bs), max(b[3] for b in bs)]
              for bs in clusters.values()]
    centres = [((g[0] + g[2]) / 2.0, (g[1] + g[3]) / 2.0) for g in groups]

    # ---- attach every fragment to its nearest pose ----
    for f in frags:
        best = min(range(len(centres)),
                   key=lambda k: (centres[k][0] - f["cx"]) ** 2 +
                                 (centres[k][1] - f["cy"]) ** 2)
        g, b = groups[best], f["box"]
        g[0] = min(g[0], b[0]); g[1] = min(g[1], b[1])
        g[2] = max(g[2], b[2]); g[3] = max(g[3], b[3])

    groups.sort(key=lambda b: (round(b[1] / 120), b[0]))   # row-ish order
    return groups


def main():
    if len(sys.argv) < 3:
        print(__doc__)
        return 2
    outdir, sheets = sys.argv[1], sys.argv[2:]
    os.makedirs(outdir, exist_ok=True)

    from rembg import new_session, remove
    session = new_session("u2netp")

    total = 0
    for path in sheets:
        stem = os.path.splitext(os.path.basename(path))[0]
        im = Image.open(path).convert("RGB")
        a = np.asarray(im)
        mask = a.min(axis=2) < WHITE_MIN
        boxes = pose_groups(mask)
        for k, (x0, y0, x1, y1) in enumerate(boxes):
            pad = 10
            x0 = max(0, x0 - pad); y0 = max(0, y0 - pad)
            x1 = min(im.size[0], x1 + pad); y1 = min(im.size[1], y1 + pad)
            cell = im.crop((x0, y0, x1, y1))
            cut = remove(cell, session=session)          # RGBA, matte alpha
            dst = os.path.join(outdir, f"{stem}__{k:02d}.png")
            cut.save(dst)
            total += 1
            print(f"  {os.path.basename(dst)}: {cut.size[0]}x{cut.size[1]}")
    print(f"sliced {total} poses -> {outdir}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
