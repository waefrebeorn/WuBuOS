#!/usr/bin/env python3
"""WCAG contrast audit for the shell's theme colours.

WCAG 2.x SC 1.4.3 requires 4.5:1 for normal text and 3:1 for large text (18pt ~= 24px, or 14pt bold
~= 18.5px). The ratios are thresholds and must not be rounded: 4.499 does not meet 4.5.

Why this lives in Python and not in the product: the shell never needs to COMPUTE a contrast ratio at
runtime -- it only needs its colours to meet one at review time. Implementing luminance in C would be
a second copy of a formula that nothing draws with, so the C side only reports hex values
(theme_contrast_dump) and this tool does the arithmetic once.

Exemptions are recorded, not silently skipped: WCAG exempts text in an inactive user interface
component, so body_text_ina is measured and printed but cannot fail the gate.

Usage: contrast_audit.py <dump-output-file> | contrast_audit.py -   (reads stdin)
Exit: 0 all met, 1 a pair failed, 2 usage/parse error.
"""
import sys

# Text/background pairs the shell actually draws, with the size class that decides the threshold and
# the reason for any exemption. Adding a theme means adding nothing here; adding a ROLE means adding a
# row, deliberately.
PAIRS = {
    "win_title_text/win_title_active":      ("normal", "window caption, 13px"),
    "win_title_text_ina/win_title_inactive": ("normal", "unfocused window caption is still read"),
    "body_text/win_face":                   ("normal", "body text, 13px"),
    "body_text_ina/win_face":               ("exempt", "WCAG 1.4.3: inactive UI component"),
    "select_text/select_bg":                ("large", "page title, 28px"),
    "btn_text/btn_face":                    ("normal", "button label"),
    "start_btn_text/start_btn_face":        ("normal", "start button"),
    "startmenu_text/startmenu_bg":          ("normal", "start menu item"),
    "icon_text/desktop_bg":                 ("normal", "desktop icon label"),
    # A button face is ~1.1:1 by DESIGN. The boundary a user sees is the outline around it, which
    # test_flat_controls proves exists at every draw site and which border_darkest/win_face measures
    # (3.12-17.14 in all seven themes). Recording the fill as a violation would demand a flat slab of
    # contrast -- not what 1.4.11 asks for.
    "btn_face/win_face":                    ("info", "outlined control: the boundary is border_darkest"),
    "border_dark/win_face":                 ("info", "inner bevel shading; the boundary is border_darkest"),
    "border_darkest/win_face":              ("nontext", "outer control outline"),
    "scroll_thumb/scroll_track":            ("nontext", "scrollbar thumb is the only indicator"),
    "icon_border/desktop_bg":               ("nontext", "icon plate outline"),
    "start_btn_face/taskbar_bg":            ("info", "outlined control: the boundary is border_darkest"),
    "start_mark/taskbar_bg":                ("nontext", "the Start mark panes drawn on the taskbar"),
    "border_darkest/win_face":              ("nontext", "the outline that IS the control boundary"),
    # The adjacency that matters is outline vs the surface OUTSIDE the control. Outline vs its own fill
    # only says the fill is a similar colour, which does not touch perceivability of the boundary.
    "border_darkest/btn_face":              ("info", "fill vs its own outline: not the perceived boundary"),
    "border_darkest/taskbar_bg":            ("nontext", "outline of the start button against the bar"),
}

THRESHOLD = {"normal": 4.5, "large": 3.0, "nontext": 3.0}   # WCAG 1.4.11


def channel(v):
    c = v / 255.0
    return c / 12.92 if c <= 0.03928 else ((c + 0.055) / 1.055) ** 2.4


def luminance(rgb):
    r, g, b = (rgb >> 16) & 0xFF, (rgb >> 8) & 0xFF, rgb & 0xFF
    return 0.2126 * channel(r) + 0.7152 * channel(g) + 0.0722 * channel(b)


def ratio(a, b):
    la, lb = luminance(a), luminance(b)
    hi, lo = max(la, lb), min(la, lb)
    return (hi + 0.05) / (lo + 0.05)


def load_baseline(path):
    """Known violations, deliberate debt. The gate fails on any violation NOT listed, and also fails
    when a listed one starts passing: a baseline that cannot shrink is a lie."""
    known = set()
    try:
        for ln in open(path):
            ln = ln.split("#", 1)[0].strip()
            if ln:
                known.add(ln)
    except FileNotFoundError:
        pass
    return known


def main(argv):
    src = argv[1] if len(argv) > 1 else "-"
    baseline_path = argv[2] if len(argv) > 2 else None
    text = sys.stdin.read() if src == "-" else open(src).read()

    rows = []
    for ln in text.splitlines():
        parts = ln.split()
        if len(parts) < 4:
            continue
        theme, pair, fg, bg = parts[0], parts[1], parts[2], parts[3]   # a trailing theme name is allowed
        try:
            rows.append((int(theme), pair, int(fg, 16), int(bg, 16)))
        except ValueError:
            continue
    if not rows:
        print("contrast_audit: no dump rows parsed", file=sys.stderr)
        return 2

    known = load_baseline(baseline_path) if baseline_path else set()
    fails = 0
    checked = 0
    failing = set()
    print("%-3s %-38s %-7s %6s %5s  %s" % ("th", "pair", "class", "ratio", "min", "verdict"))
    for theme, pair, fg, bg in sorted(rows):
        cls, why = PAIRS.get(pair, ("normal", "unlisted pair"))
        r = ratio(fg, bg)
        if cls in ("exempt", "info"):
            print("%-3d %-38s %-7s %6.2f %5s  %s (%s)" % (theme, pair, cls, r, "-", cls, why))
            continue
        checked += 1
        need = THRESHOLD[cls]
        ok = r >= need          # not rounded: 4.499 does not meet 4.5
        if not ok:
            fails += 1
            failing.add("%d %s" % (theme, pair))
        print("%-3d %-38s %-7s %6.2f %5.1f  %s%s" % (
            theme, pair, cls, r, need, "PASS" if ok else "FAIL",
            "" if ok else "  <-- " + why))

    print("\ncontrast_audit: %d pair(s) checked, %d failed, %d exempt" %
          (checked, fails, len(rows) - checked))

    new = failing - known
    stale = known - failing
    if known:
        print("baseline: %d known violation(s), %d new, %d now passing" %
              (len(known), len(new), len(stale)))
    for k in sorted(new):
        print("  NEW VIOLATION: %s" % k)
    for k in sorted(stale):
        print("  STALE BASELINE ENTRY (now passes -- remove it): %s" % k)
    if new or stale:
        return 1
    return 0 if fails == len(known) else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv))
