#!/usr/bin/env python3
"""a11y_design_svg.py — emit the WuBuOS accessibility-cluster design as SVG.

The user's design hint (2026-08-07): "i want you to svg…" — deliver the
button design as an SVG, not just pixel frames. This script renders the
EXACT geometry from src/gui/wubu_a11y.c so the SVG can never drift from
the code (mirror-file #5 — the other four are wubu_a11y.c,
wubu_a11y_test.c, wubu_desktop_shot.c, wubu_a11y_shot.c).

Design spec (canonical, reference-traced):
  * Cluster floats DIRECTLY on the window face (no panel background).
  * Yellow X (TL): round-tip CRESCENT = BEAN, band hugs the 9->12 o'clock
    upper-left arc, hollow opens DOWN-RIGHT (dir +0.707,+0.707, offset
    0.62r). Click = minimize, drag = ROTATE. Dark handle slot on belly.
  * Green A (TR): BIGGEST circle r20 (2:1 vs red). Click = maximize,
    drag = move. Dark folded-corner grab glyph.
  * Red B (BR): smallest circle r10. Click = end session, full drag =
    purge+close, partial drag = STIM (pulse, stays alive). Dark X glyph.
  * Purple Y: TWO round-tip crescents at the WINDOW's bottom-left AND
    bottom-right corners (not in the cluster), same 9->12 bean, edge-
    detection reveal (invisible beyond 46px, ramps 18->46px, fully shown
    while resizing). Diagonal grip in the belly.
  * All shadows are crescent-shaped for the beans (a circular shadow under
    a crescent reads as a pale disc), soft circular for the orbs, cast
    down-right (lighting from upper-left, neumorphic).

Usage:  python3 scripts/a11y_design_svg.py [out.svg]
"""
import sys

# ---- palette (mirror of wubu_a11y.c defines) -------------------------
GREEN   = "#42A85A"; GREEN_DARK   = "#1B5E20"
YELLOW  = "#DE9E44"; YELLOW_DARK  = "#7A4A10"
RED     = "#E53935"; RED_DARK     = "#8E1A1A"
PURPLE  = "#9C27B0"; PURPLE_DARK  = "#6A1B9A"
SHADOW  = "#C9BFE8"
SILVER  = "#C0C0C0"; SILVER_EDGE = "#808080"
NAVY    = "#000080"; WHITE       = "#FFFFFF"

# ---- geometry (mirror of wubu_a11y.c — research-corrected 2026-08-07)
# Real GameCube hardware: A button 16.747mm, B button 6.449mm → 2.6:1.
# Reference trace (1280x1156 canvas): green(465,464) LEFT of red(741,465),
# SAME cy = horizontal pair. Yellow crescent (370,250) up-left of green.
# All buttons >= WCAG AA 24px minimum.
A_R = 31   # green A: 62px diameter >= WCAG AAA 44px
B_R = 12   # red B:   24px = WCAG AA 2.5.8 min
Y_R = 14   # yellow crescent: 28px
P_R = 14   # purple crescent: 28px
HOLLOW_DX, HOLLOW_DY = 0.707, 0.707   # down-right

# Reference-proportional demo window (canvas 1280x1156 → ratio 1.108:1)
WX, WY, WW, WH = 40, 40, 540, 488
TBH = 22
# Cluster floats INSIDE the window below the title bar (NOT in the corner)
def orb_y():  return (WX + int(WW * 0.289), WY + int(WH * 0.216))
def orb_a():  return (WX + int(WW * 0.363), WY + int(WH * 0.401))
def orb_b():  return (WX + int(WW * 0.579), WY + int(WH * 0.402))
def orb_p_bl(): return (WX + 22, WY + WH - 26)
def orb_p_br(): return (WX + WW - 22, WY + WH - 26)

def svg_circle(cx, cy, r, fill, stroke=None, sw=0, opacity=1.0):
    s = f'<circle cx="{cx}" cy="{cy}" r="{r}" fill="{fill}"'
    if stroke: s += f' stroke="{stroke}" stroke-width="{sw}"'
    if opacity < 1.0: s += f' fill-opacity="{opacity:.2f}"'
    return s + '/>'

def svg_crescent(cx, cy, r, fill, opacity=1.0, off=0.62):
    """Round-tip crescent = circle A minus circle B offset 0.62r along the
    hollow direction (down-right). evenodd keeps the rounded tips where the
    two circles cross."""
    ox, oy = HOLLOW_DX * r * off, HOLLOW_DY * r * off
    path = (f'<path fill-rule="evenodd" fill="{fill}"'
            + (f' fill-opacity="{opacity:.2f}"' if opacity < 1.0 else '')
            + f' d="M {cx - r} {cy} a {r} {r} 0 1 0 {2*r} 0 a {r} {r} 0 1 0 {-2*r} 0 Z '
            + f'M {cx - r + ox} {cy + oy} a {r} {r} 0 1 0 {2*r} 0 a {r} {r} 0 1 0 {-2*r} 0 Z"/>')
    return path

def glyph_grab(cx, cy, r, col):
    """Dark folded-corner diagonal (draw_grab_glyph: 2x3 blocks stepping
    up-right)."""
    s = r - 5
    parts = []
    for i in range(0, s + 1):
        x, y = cx - s + i, cy + s - 2 - i
        parts.append(f'<rect x="{x}" y="{y}" width="2" height="3" fill="{col}"/>')
    return "\n".join(parts)

def glyph_close(cx, cy, r, col):
    """Bold dark X (draw_close_glyph: 2px diagonals)."""
    s = r - 4
    parts = []
    for i in range(-s, s + 1):
        parts.append(f'<rect x="{cx+i}" y="{cy+i}" width="2" height="2" fill="{col}"/>')
        parts.append(f'<rect x="{cx+i}" y="{cy-i-1}" width="2" height="2" fill="{col}"/>')
    return "\n".join(parts)

def glyph_resize(cx, cy, r, col):
    """Three diagonal grip lines (draw_resize_glyph)."""
    s = r - 5
    parts = []
    for i in range(3):
        gx, gy = cx + s - 2 - i * 3, cy - s + 2 + i * 3
        parts.append(f'<line x1="{gx}" y1="{gy}" x2="{gx+4}" y2="{gy+4}" '
                     f'stroke="{col}" stroke-width="2"/>')
    return "\n".join(parts)

def glyph_slot(cx, cy, col):
    """Yellow handle slot on the bean belly."""
    return (f'<rect x="{cx-10}" y="{cy-7}" width="10" height="4" rx="2" '
            f'fill="{col}"/>')

def main(out):
    yx, yy = orb_y()
    ax, ay = orb_a()
    bx, by = orb_b()
    p1x, p1y = orb_p_bl()
    p2x, p2y = orb_p_br()

    body = []
    body.append('<defs>')
    body.append(f'<linearGradient id="gA" x1="0" y1="0" x2="1" y2="1">'
                f'<stop offset="0" stop-color="{WHITE}" stop-opacity="0.43"/>'
                f'<stop offset="1" stop-color="{GREEN_DARK}" stop-opacity="0.55"/>'
                f'</linearGradient>')
    body.append(f'<linearGradient id="gB" x1="0" y1="0" x2="1" y2="1">'
                f'<stop offset="0" stop-color="{WHITE}" stop-opacity="0.43"/>'
                f'<stop offset="1" stop-color="{RED_DARK}" stop-opacity="0.55"/>'
                f'</linearGradient>')
    body.append('</defs>')
    # window
    body.append(f'<rect x="{WX}" y="{WY}" width="{WW}" height="{WH}" fill="{SILVER}"/>')
    body.append(f'<rect x="{WX}" y="{WY}" width="{WW}" height="{WH}" fill="none" '
                f'stroke="{SILVER_EDGE}" stroke-width="2"/>')
    body.append(f'<rect x="{WX}" y="{WY}" width="{WW}" height="{TBH}" fill="{NAVY}"/>')
    body.append(f'<text x="{WX+10}" y="{WY+15}" font-family="monospace" font-size="13" '
                f'fill="{WHITE}">A11y Demo (reference-proportional)</text>')

    # YELLOW bean (TL) — crescent shadow + body + slot
    body.append(svg_crescent(yx, yy + 2, Y_R, SHADOW, opacity=0.37))
    body.append(svg_crescent(yx, yy, Y_R, YELLOW))
    body.append(glyph_slot(yx, yy, YELLOW_DARK))
    # GREEN A (TR) — soft circular shadow + gradient orb + grab glyph
    body.append(svg_circle(ax + 3, ay + 4, A_R + 2, SHADOW, opacity=0.39))
    body.append(svg_circle(ax, ay, A_R, GREEN))
    body.append(svg_circle(ax, ay, A_R, "url(#gA)"))
    body.append(f'<circle cx="{ax}" cy="{ay}" r="{A_R}" fill="none" '
                f'stroke="{GREEN_DARK}" stroke-width="1.5"/>')
    body.append(glyph_grab(ax, ay, A_R, GREEN_DARK))
    # RED B (BR) — shadow + orb + X
    body.append(svg_circle(bx + 3, by + 4, B_R + 2, SHADOW, opacity=0.39))
    body.append(svg_circle(bx, by, B_R, RED))
    body.append(svg_circle(bx, by, B_R, "url(#gB)"))
    body.append(f'<circle cx="{bx}" cy="{by}" r="{B_R}" fill="none" '
                f'stroke="{RED_DARK}" stroke-width="1.5"/>')
    body.append(glyph_close(bx, by, B_R, RED_DARK))
    # PURPLE beans at window bottom corners — crescent shadow + body + grip
    for (pxx, pyy) in ((p1x, p1y), (p2x, p2y)):
        body.append(svg_crescent(pxx, pyy + 2, P_R, SHADOW, opacity=0.37))
        body.append(svg_crescent(pxx, pyy, P_R, PURPLE))
        body.append(glyph_resize(pxx - 3, pyy - 3, P_R, PURPLE_DARK))

    svg = [f'<svg xmlns="http://www.w3.org/2000/svg" width="640" height="420" '
           f'viewBox="0 0 640 420">']
    svg.append(f'<rect width="640" height="420" fill="#EFE7F7"/>')
    svg.extend(body)
    svg.append('</svg>')
    with open(out, "w") as f:
        f.write("\n".join(svg))
    print(f"wrote {out}")

if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else "a11y_design.svg")
