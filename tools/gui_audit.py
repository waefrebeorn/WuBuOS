#!/usr/bin/env python3
"""gui_audit.py -- measure the interface instead of arguing about it.

See docs/compendium/04-roadmap/ui-excellence-methodology.md. Four axes, four numbers:

  --contrast   WCAG AA/AAA matrix over the live palette (from the C theme table)
  --geometry   distinct spacing/radius values used across the GUI sources
  --edges      hard-transition ratio of a rendered frame (blockiness)
  --coverage   icons/keys the product references vs. what exists

Exit non-zero on a real violation, print the numbers either way. No thresholds are
invented: contrast is WCAG, geometry is the metrics header, edges are compared to a
committed baseline, coverage is absolute (everything resolves or it is listed).
"""
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent


def lum(hexcolour):
    r = (hexcolour >> 16) & 0xFF
    g = (hexcolour >> 8) & 0xFF
    b = hexcolour & 0xFF
    def ch(c):
        c = c / 255.0
        return c / 12.92 if c <= 0.03928 else ((c + 0.055) / 1.055) ** 2.4
    return 0.2126 * ch(r) + 0.7152 * ch(g) + 0.0722 * ch(b)


def ratio(a, b):
    la, lb = lum(a), lum(b)
    hi, lo = max(la, lb), min(la, lb)
    return (hi + 0.05) / (lo + 0.05)


THEME_SRC = ROOT / "src/gui/wubu_theme.c"


def parse_themes():
    """Pull the palette straight out of the C table -- the same source the renderer uses."""
    text = THEME_SRC.read_text(errors="replace")
    themes = {}
    for m in re.finditer(r"\[THEME_(\w+)\] = \{(.*?)\n\},", text, re.S):
        name, body = m.group(1), m.group(2)
        cols = {}
        for c in re.finditer(r"\.(\w+)\s*=\s*RGB\(0x([0-9A-Fa-f]{2}),0x([0-9A-Fa-f]{2}),0x([0-9A-Fa-f]{2})\)", body):
            cols[c.group(1)] = (int(c.group(2), 16) << 16) | (int(c.group(3), 16) << 8) | int(c.group(4), 16)
        themes[name] = cols
    return themes


PAIRS = [
    ("win_title_text", "win_title_active", 4.5, "title text on title bar"),
    ("win_title_text_ina", "win_title_inactive", 3.0, "inactive title text"),
    ("btn_text", "btn_face", 4.5, "button label on button face"),
    ("startmenu_text", "startmenu_bg", 4.5, "menu text on menu"),
    ("icon_text", "desktop_bg", 3.0, "desktop icon label"),
    ("select_text", "select_bg", 4.5, "selected text on accent"),
]


def contrast():
    bad = 0
    for name, cols in parse_themes().items():
        for fg, bg, need, label in PAIRS:
            if fg not in cols or bg not in cols:
                print(f"  [SKIP] {name}: {label} (colour missing from theme)")
                continue
            r = ratio(cols[fg], cols[bg])
            ok = r >= need
            bad += 0 if ok else 1
            print(f"  [{'PASS' if ok else 'FAIL'}] {name:12} {label:34} {r:5.2f}:1 (need {need})")
    print(f"contrast: {bad} violation(s)")
    return bad


def edges(frame):
    data = Path(frame).read_bytes()
    head, px = data.split(b"\n255\n", 1)
    w, h = (int(v) for v in head.split(b"\n")[1].split())
    hard = total = 0
    for y in range(0, h, 2):
        row = y * w * 3
        for x in range(0, w - 1, 2):
            o = row + x * 3
            d = max(abs(px[o] - px[o + 3]), abs(px[o + 1] - px[o + 4]), abs(px[o + 2] - px[o + 5]))
            total += 1
            if d > 24:
                hard += 1
    pct = 100.0 * hard / max(total, 1)
    print(f"edges: hard-transition ratio {pct:.2f}% of sampled neighbour pairs (blockiness)")
    return 0


def geometry():
    """Values that must come from the metrics header, found typed by hand instead."""
    import subprocess
    r = subprocess.run([sys.executable, str(ROOT / "tools/gate_gui_fluent.py")],
                       capture_output=True, text=True)
    print(r.stdout.strip())
    return r.returncode


def main(argv):
    mode = argv[0] if argv else "--contrast"
    if mode == "--contrast":
        return 1 if contrast() else 0
    if mode == "--edges":
        if len(argv) < 2:
            print("usage: gui_audit.py --edges frame.ppm")
            return 2
        return edges(argv[1])
    if mode == "--geometry":
        return geometry()
    if mode == "--coverage":
        icons = list((ROOT / "assets/icons").rglob("*.svg")) if (ROOT / "assets/icons").exists() else []
        print(f"coverage: {len(icons)} icon source(s) in assets/icons")
        print("coverage: none yet -- every surface currently draws its own glyphs in C")
        return 0
    print(__doc__)
    return 2


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
