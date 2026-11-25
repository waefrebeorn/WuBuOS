#!/usr/bin/env python3
"""gate_gui_fluent.py -- the Win11 desktop standard, enforced instead of remembered.

docs/compendium/04-roadmap/win11-desktop-standard.md says, in one table:

  * compositor/WM/input      -> C11
  * shell surfaces           -> .WET applets
  * every number             -> include/wubu_gui_metrics.h
  * every surface            -> gated on a widget or a pixel

A standard nobody checks is a preference. This gate checks the two rules that can be
checked mechanically and that were being broken while the standard was written:

  1. NO INVENTED SURFACE COLOURS. A GUI .c file may not contain its own 0xRRGGBB
     surface literal; colours come from the theme (wubu_theme.c) or the metrics header.
     Ratchet: tools/gui_fluent.baseline holds today's count, and the gate FAILS if the
     count GROWS, so new code cannot dig the hole deeper while the old literal is
     retired file by file.

  2. NO INVENTED GEOMETRY. The Fluent numbers (taskbar 48, title 32, radius 8,
     hairline 1, snap gap 8) must be read from WUBU_METRIC_*; a bare 48/32/8 next to a
     taskbar/title/radius word is the "almost Win11" bug in code form.

Usage: gate_gui_fluent.py [--baseline N] [--write-baseline]
"""
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
BASE = ROOT / "tools" / "gui_fluent.baseline"

# Owners that MAY name raw colours: the palette itself and the metrics vocabulary.
COLOUR_OWNERS = {ROOT / "src/gui/wubu_theme.c", ROOT / "include/wubu_gui_metrics.h"}
SCAN_DIRS = [ROOT / "src/gui", ROOT / "src/apps"]

COLOUR = re.compile(r"0x[0-9A-Fa-f]{6}\b")
# geometry invented next to the words that own it
GEOM = re.compile(r"\b(48|32|8|4)\b\s*[;,)]?.*\b(taskbar|title_?h|title_bar|radius|hairline|snap_gap)\b",
                  re.IGNORECASE)


def files():
    for d in SCAN_DIRS:
        if not d.exists():
            continue
        for p in sorted(d.rglob("*.c")):
            # TESTS ARE EXEMPT ON PURPOSE. A test that verifies "this pixel is theme
            # navy 0x000080" must contain that exact number; forcing it through a
            # metric would replace a check of the real value with a check of a constant
            # that the same edit defines. Product code has no such excuse.
            if p.name.endswith("_test.c"):
                continue
            if p in COLOUR_OWNERS:
                continue
            yield p


def scan():
    colour_hits, geom_hits = [], []
    for p in files():
        try:
            text = p.read_text(errors="replace")
        except OSError:
            continue
        for i, line in enumerate(text.splitlines(), 1):
            if line.lstrip().startswith(("*", "/*", "//")):
                continue
            if COLOUR.search(line):
                colour_hits.append(f"{p.relative_to(ROOT)}:{i}")
            if GEOM.search(line):
                geom_hits.append(f"{p.relative_to(ROOT)}:{i}")
    return colour_hits, geom_hits


def main(argv):
    colour_hits, geom_hits = scan()
    total = len(colour_hits) + len(geom_hits)

    if "--write-baseline" in argv:
        BASE.write_text(str(total) + "\n")
        print(f"gui fluent: baseline written = {total}")
        return 0

    baseline = int(BASE.read_text().strip()) if BASE.exists() else None
    if baseline is None:
        print(f"gui fluent: NO BASELINE. Colour literals={len(colour_hits)} "
              f"geometry literals={len(geom_hits)} total={total}")
        print("gui fluent: create one with --write-baseline, and only ever lower it")
        return 1

    print(f"gui fluent: colour literals={len(colour_hits)} geometry literals={len(geom_hits)} "
          f"total={total} baseline={baseline}")
    if total > baseline:
        print(f"gui fluent: FAIL -- the standard grew by {total - baseline}. "
              "Use wubu_gui_metrics.h / the theme instead of a literal:")
        for h in (colour_hits + geom_hits)[:12]:
            print("  ", h)
        return 1
    if total < baseline:
        print(f"gui fluent: PASS (and {baseline - total} literal(s) retired -- "
              f"lower tools/gui_fluent.baseline to {total} to keep the ratchet tight)")
        return 0
    print("gui fluent: PASS")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
