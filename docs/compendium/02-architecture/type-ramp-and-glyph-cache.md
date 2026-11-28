# Type ramp and glyph caching (sourced requirements for SLERM-2027 stage 3)

Why this document exists: the shell currently draws text from a 13 px compiled bitmap cell, so
every string is monospaced and every size is the same size. Replacing it needs two things a
guess cannot supply — the reference type sizes, and a cache design that keeps redraw cheap.

## 1. The reference type ramp (Microsoft, official)

Source: <https://learn.microsoft.com/en-us/windows/apps/design/signature-experiences/typography>

| Role | Size / line height | Weight |
| --- | --- | --- |
| Small (caption) | 12/16 epx | Regular |
| Text (body) | 14/20 epx | Regular |
| Text semibold | 14/20 epx | Semibold |
| Text large | 18/24 epx | Regular |
| Text large semibold | 18/24 epx | Semibold |
| Display semibold | 20/28 epx | Semibold |
| Display semibold | 28/36 epx | Semibold |
| Display semibold | 40/52 epx | Semibold |
| Display semibold | 68/92 epx | Semibold |

The unit matters: Microsoft specifies **epx** (effective pixels), not raw device pixels. An epx is
the logical pixel *after* DPI scaling, so at 150 % scale a 14 epx body text rasterises at 21 device
pixels while remaining 14 in layout. Any implementation that treats these as device pixels will be
wrong on every scaled display — which is most laptops.

Note also that the ramp is not a set of arbitrary sizes: 12/14/18/20/28/40/68 with line heights
16/20/24/28/36/52/92. Line height is NOT size x 1.5; the ratios vary from 1.31 to 1.35, so the
ratio must be a table, not a multiplier, if the goal is to match.

## 2. Why a bitmap cell cannot be fixed by "adding sizes"

A compiled bitmap cell bakes three separate decisions into one asset: the size, the advance of
each glyph, and the shape. Our cell is 13 px, so:

- any UI text at 12 epx renders one pixel too large and with the wrong line height;
- any heading cannot be rendered at 20/28 at all;
- proportional advances are impossible, so "iiii" and "MMMM" occupy identical width.

Hence the goal is an outline rasteriser plus a cache, not a bigger bitmap.

## 3. Glyph cache design (what real toolkits do)

Two properties are non-negotiable, both visible in shipping implementations:

1. **The cache key includes the subpixel position.** Skia's glyph cache exposes variants that take
   the device position (`getUnicharMetrics(SkUnichar, SkFixed x, SkFixed y)`) precisely because a
   glyph rendered at x.00 and at x.25 differ; caching only by glyph id produces text that visibly
   "snaps" as it scrolls. Source:
   <https://chromium.googlesource.com/external/skia/src/+/refs/heads/master/core/SkGlyphCache.h>
2. **Rasterised glyphs go into an atlas, not into one allocation per glyph.** Per-glyph
   allocations fragment and cost a malloc per draw; atlas packing costs one allocation and one
   texture upload. A practical account of the trade-offs (and of kerning interacting with caching):
   <https://www.warp.dev/blog/adventures-text-rendering-kerning-glyph-atlases>

## 4. What WuBuOS will implement

- Cache key: `(gid, ppem, subpixel_bucket)` with a small fixed number of horizontal subpixel
  buckets (4 is the common choice: 0, 0.25, 0.5, 0.75). Vertical subpixel positioning is NOT
  cached: UI text sits on integer baselines, and the bucket count multiplies memory by its square.
- Storage: one 8-bit coverage atlas, power-of-two, shelf-packed; evict whole atlas and rebuild on
  overflow rather than per-glyph LRU (simpler, and a full rebuild is rare at UI sizes).
- Metrics come from `hmtx` (already implemented and gated in stage 1); advances stay in font units
  and are converted at draw time so the same cache serves every scale factor.
- Type ramp: the table above, in epx, converted to device pixels by the existing scale factor,
  with line heights taken from the table rather than computed.

## 5. What is NOT claimed

- No hinting. Our rasteriser does not grid-fit, so at 12-14 epx our stems will be fractionally
  thinner than the reference. That is a deliberate trade: hinting is a large amount of
  font-specific code, and the measured ink difference is recorded in the SLERM-2027 gate.
- No subpixel (RGB) antialiasing yet. Coverage is grayscale; ClearType-style filtering changes the
  image and would need its own oracle.
