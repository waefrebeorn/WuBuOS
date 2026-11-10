/*
 * wubu_palette.h -- derive a colour scheme from an image.
 *
 * Shoji-bar-4's "dynamic colour": the scheme is made from a seed colour, and
 * the seed can come from the WALLPAPER.  This is the seed extractor.
 *
 * Pure C11 over stdint: no GUI, no image decoding, no allocator.  Feed it the
 * decoded pixels (XRGB8888, as WubuWallpaper.pixels) and it returns the two
 * colours a scheme needs -- a dominant base and a vivid accent.
 *
 * The algorithm is deliberately simple and deterministic:
 *   1. histogram the image into 4-bit-per-channel buckets (4096 entries)
 *   2. the dominant bucket is the base
 *   3. the most saturated bucket holding at least 2% of the samples is the
 *      accent; with no such bucket the accent IS the base
 *
 * Determinism matters: the same wallpaper must yield the same scheme, or the
 * desktop would restyle itself differently on every boot.
 */
#ifndef WUBU_PALETTE_H
#define WUBU_PALETTE_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint32_t base;    /* the dominant colour (0xRRGGBB) */
    uint32_t accent;  /* the most vivid colour with enough presence       */
    uint32_t accent2; /* the second most vivid, for gradients             */
    int      vibrant; /* 1 when a real accent was found, 0 when == base   */
} WubuPalette;

/*
 * Extract a palette from a w*h image of 0xRRGGBB pixels.
 *
 * `step` samples every Nth pixel in both axes (1 = every pixel).  Large
 * images should pass a step that keeps the sample count near 4096; the result
 * does not depend on which pixels are skipped, only on the histogram.
 *
 * Returns false when the image is empty or degenerate (all one colour with no
 * saturation), in which case the caller should keep its current scheme.
 */
bool wubu_palette_extract(const uint32_t *px, int w, int h, int step,
                          WubuPalette *out);

/* Darken/lighten a 0xRRGGBB colour by a signed percent (-100..100). */
uint32_t wubu_palette_shade(uint32_t color, int percent);

#ifdef __cplusplus
}
#endif

#endif /* WUBU_PALETTE_H */
