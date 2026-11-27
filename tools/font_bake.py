#!/usr/bin/env python3
"""Bake a real TrueType face into a C coverage table -- the font equivalent of
tools/icon_bake.py.

Why data and not a procedure: the rasteriser already renders glyphs from a bitmap table,
and that table is 1-bit. Every "the text looks blocky" report is that table. A face is a
thing that gets re-baked from a real font file with provenance, not a thing re-argued by
hand.

Contract (identical in spirit to the icon bake):
  * input is a real .ttf, path and pixel size recorded in the output header
  * coverage is 8-bit antialiased from the font's own outlines (FreeType via Pillow)
  * a glyph that renders EMPTY while its codepoint is not whitespace is a hard error,
    because a silently skipped codepoint is a missing character in the product
  * the bake prints an ASCII preview so a human can see the result without a build

Usage: python3 tools/font_bake.py [--font PATH] [--size PX] [--out PATH]
"""

import argparse
import os
import sys

try:
    from PIL import Image, ImageDraw, ImageFont
except ImportError:  # pragma: no cover
    sys.exit("font_bake: Pillow is required (python3 -m pip install pillow)")

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DEFAULT_FONT = os.path.join(ROOT, "docs/reference/win11/fonts/segoeuil.ttf")
DEFAULT_OUT = os.path.join(ROOT, "src/gui/fonts/wubu_font_text.h")

FIRST, LAST = 0x20, 0xFF


def render(font, ch):
    """Return (w, h, xoff, yoff, bytes) with yoff relative to the baseline."""
    ascent, descent = font.getmetrics()
    try:
        mask = font.getmask(ch, mode="L")
    except RuntimeError:
        # raqm cannot lay out a codepoint this face has no mapping for (soft hyphen, some format
        # characters). Treat it as an empty glyph: the caller decides whether empty is acceptable.
        return 0, 0, 0, 0, b""
    w, h = mask.size
    if w == 0 or h == 0:
        return 0, 0, 0, 0, b""
    # Pillow puts the glyph's top at y=0; translate to baseline-relative coords so the
    # renderer can blit one row at a time without re-deriving metrics per glyph.
    yoff = -(ascent - font.getbbox(ch)[1]) if ch.strip() else -ascent
    data = bytes(mask)
    return w, h, 0, yoff, data


def parse_existing(path):
    """Reuse the glyphs an existing header already carries: cp -> (w, h, xoff, yoff, bytes).

    Why: re-baking with a newer Pillow changes EVERY glyph's coverage (ASCII 'A' went 182 -> 127 at the same
    pixel), so a "just add Latin-1" bake silently restyled all existing text -- it changed a layout enough to
    fail an unrelated GUI gate. Extending a table must not restyle what is already shipped.
    """
    import re
    try:
        with open(path) as f:
            text = f.read()
    except OSError:
        return {}
    m = re.search(r"#define WUBU_FONT_TEXT_FIRST\s+0x([0-9A-Fa-f]+)", text)
    first = int(m.group(1), 16) if m else 0x20

    arrays = {}
    for m in re.finditer(r"static const uint8_t g_glyph_([0-9a-f]{2})\s*\[\d+\]\s*=\s*\{(.*?)\};",
                         text, re.S):
        cp = int(m.group(1), 16)
        arrays[cp] = bytes(int(v) for v in re.findall(r"\b(\d+)\b", m.group(2)))

    out = {}
    for m in re.finditer(r"\[\s*(\d+)\s*\]\s*=\s*\{\s*(-?\d+)\s*,\s*(-?\d+)\s*,\s*"
                         r"(-?\d+)\s*,\s*(-?\d+)\s*,\s*(?:g_glyph_([0-9a-f]{2})|NULL)\s*\}", text):
        cp = first + int(m.group(1))
        w, h, xoff, yoff = (int(m.group(i)) for i in range(2, 6))
        cov = arrays.get(int(m.group(6), 16), b"") if m.group(6) else b""
        out[cp] = (w, h, xoff, yoff, cov)
    return out


def ascii_preview(glyphs, ascent, text):
    """Preview the BAKED table, not Pillow's own renderer.

    The point of the preview is to catch a wrong table before a build does: the icon bake
    proved that (its first close glyph was a 1px hairline, visible only in this output).
    Rendering with Pillow's text() would preview Pillow, not what we ship.
    """
    table = {cp: g for cp, _ch, *rest in glyphs for g in [rest]}
    pen = 0
    placed = []
    for ch in text:
        g = table.get(ord(ch))
        if not g:
            continue
        w, h, xoff, yoff, data = g
        if data:
            placed.append((pen + xoff, yoff, w, h, data))
        pen += (w if w else 4) + 1

    width = max(1, pen + 2)
    height = ascent + 8
    canvas = [[0] * width for _ in range(height)]
    for x, y, w, h, data in placed:
        top = ascent + y
        for gy in range(h):
            row = canvas[top + gy] if 0 <= top + gy < height else None
            if row is None:
                continue
            for gx in range(w):
                px = x + gx
                if 0 <= px < width:
                    row[px] = max(row[px], data[gy * w + gx])

    ramp = " .:-=+*#%@"
    out = []
    for row in canvas:
        # Full resolution, not downsampled: a 13px light face loses its whole shape at
        # 2:1, and the preview exists to be LOOKED AT.
        line = "".join(ramp[min(len(ramp) - 1, v * len(ramp) // 256)] for v in row)
        if line.strip():
            out.append(line.rstrip())
    return "\n".join(out)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--font", default=DEFAULT_FONT)
    ap.add_argument("--size", type=int, default=13)
    ap.add_argument("--out", default=DEFAULT_OUT)
    ap.add_argument("--prefix", default="",
                    help="symbol prefix so two faces can live in one translation unit (e.g. bold_)")
    ap.add_argument("--preserve", default=None,
                    help="existing header whose glyphs must be REUSED unchanged (only new codepoints bake)")
    a = ap.parse_args()

    if not os.path.isfile(a.font):
        sys.exit(f"font_bake: no such font: {a.font}")
    font = ImageFont.truetype(a.font, a.size)
    ascent, descent = font.getmetrics()

    glyphs = []
    for cp in range(FIRST, LAST + 1):
        ch = chr(cp)
        w, h, xoff, yoff, data = render(font, ch)
        # NBSP and soft hyphen are invisible BY DESIGN; everything else that renders empty is a hole.
        if w == 0 and ch.strip() and cp not in (0x00A0, 0x00AD):
            sys.exit(f"font_bake: U+{cp:04X} '{ch}' rendered empty -- refusing to ship a "
                     f"table with a hole in it")
        glyphs.append((cp, ch, w, h, xoff, yoff, data))

    if a.preserve:
        old = parse_existing(a.preserve)
        kept = 0
        merged = []
        for cp, ch, w, h, xoff, yoff, data in glyphs:
            if cp in old:
                ow, oh, ox, oy, od = old[cp]
                merged.append((cp, ch, ow, oh, ox, oy, od))
                kept += 1
            else:
                merged.append((cp, ch, w, h, xoff, yoff, data))
        glyphs = merged
        print(f"font_bake: preserved {kept} glyph(s) from {os.path.basename(a.preserve)} "
              f"(their coverage is NOT re-rendered)")

    os.makedirs(os.path.dirname(a.out), exist_ok=True)
    with open(a.out, "w") as f:
        f.write("/* GENERATED by tools/font_bake.py -- do not edit by hand.\n")
        f.write(f" * face: {os.path.relpath(a.font, ROOT)}\n")
        f.write(f" * px: {a.size}  ascent: {ascent}  descent: {descent}\n")
        f.write(" * coverage: 8-bit antialiased, one byte per pixel, row-major\n */\n")
        guard = "WUBU_FONT_TEXT_%sH" % (a.prefix.upper().replace("_", "") or "")
        f.write(f"#ifndef {guard}\n#define {guard}\n\n")
        f.write("#include <stdint.h>\n#include <stddef.h>   /* NULL for blank glyphs */\n\n")
        f.write(f"#ifndef WUBU_FONT_TEXT_PX\n#define WUBU_FONT_TEXT_PX      {a.size}\n#endif\n")
        f.write(f"#ifndef WUBU_FONT_TEXT_ASCENT\n#define WUBU_FONT_TEXT_ASCENT  {ascent}\n#endif\n")
        f.write(f"#ifndef WUBU_FONT_TEXT_DESCENT\n#define WUBU_FONT_TEXT_DESCENT {descent}\n#endif\n")
        f.write(f"#ifndef WUBU_FONT_TEXT_LINE_H\n#define WUBU_FONT_TEXT_LINE_H  {ascent + descent}\n#endif\n")
        f.write(f"#ifndef WUBU_FONT_TEXT_FIRST\n#define WUBU_FONT_TEXT_FIRST   0x{FIRST:02X}\n#endif\n")
        f.write(f"#ifndef WUBU_FONT_TEXT_LAST\n#define WUBU_FONT_TEXT_LAST    0x{LAST:02X}\n#endif\n\n")
        f.write("#ifndef WUBU_GLYPH_AA_TYPEDEF\n#define WUBU_GLYPH_AA_TYPEDEF\n")
        f.write("typedef struct {\n")
        f.write("    int8_t        w, h;      /* glyph box */\n")
        f.write("    int8_t        xoff, yoff;/* placement from pen / baseline */\n")
        f.write("    const uint8_t *cov;      /* w*h coverage, or NULL for a blank */\n")
        f.write("} WubuGlyphAA;\n#endif\n\n")
        for cp, ch, w, h, xoff, yoff, data in glyphs:
            if not data:
                continue
            f.write(f"static const uint8_t g_glyph_{a.prefix}{cp:02x}[{w * h}] = {{")
            for i, b in enumerate(data):
                f.write(("\n    " if i % 16 == 0 else " "))
                f.write(f"{b:3d},")
            f.write("\n};\n")
        table = "g_font_glyphs" if not a.prefix else f"g_font_glyphs_{a.prefix}"
        f.write(f"\nstatic const WubuGlyphAA {table}[] = {{\n")
        for cp, ch, w, h, xoff, yoff, data in glyphs:
            cov = f"g_glyph_{a.prefix}{cp:02x}" if data else "NULL"
            f.write(f"    [{cp - FIRST}] = {{ {w}, {h}, {xoff}, {yoff}, {cov} }},"
                    f"  /* '{(ch if ch != chr(92) else '/')}' */\n")
        f.write("};\n\n")
        f.write(f"#define WUBU_FONT_TEXT_COUNT {LAST - FIRST + 1}\n\n")
        f.write("#endif\n")

    total = sum(len(g[6]) for g in glyphs)
    print(f"font_bake: {a.font} @ {a.size}px -> {a.out}")
    print(f"font_bake: {len([g for g in glyphs if g[6]])} glyphs, {total} coverage bytes, "
          f"line height {ascent + descent}")
    print(ascii_preview(glyphs, ascent, "WuBuOS Ag ily1|{}"))


if __name__ == "__main__":
    main()
