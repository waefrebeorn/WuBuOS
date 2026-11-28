/* wubu_text_cache.h -- glyph cache over a coverage atlas (SLERM-2027 stage 3b).
 *
 * Text drawing must not rasterise per frame: a window title is ~20 glyphs, a file list is ~500,
 * and each rasterisation walks every contour of the outline. So rasterised coverage is cached.
 *
 * The key is (glyph id, ppem, subpixel bucket), not glyph id alone. Two glyphs at x.00 and x.25
 * are different bitmaps, and caching only by id makes text visibly snap sideways while scrolling
 * (Skia's glyph cache takes the device position for exactly this reason:
 * chromium.googlesource.com/external/skia/src/+/refs/heads/master/core/SkGlyphCache.h).
 *
 * Storage is one 8-bit coverage atlas, shelf-packed, with whole-atlas replacement on overflow.
 * Per-glyph malloc would fragment and cost an allocation per draw; an atlas costs one allocation
 * and one upload. Bucket count is 4 (quarters of a pixel): vertical subpixel positioning is not
 * cached because UI text sits on integer baselines and the bucket count would square.
 */
#ifndef WUBU_TEXT_CACHE_H
#define WUBU_TEXT_CACHE_H

#include <stdint.h>

#include "wubu_truetype.h"

#define WUBU_TEXT_SUBPIX 4

typedef struct WubuTextCache WubuTextCache;

/* A cached glyph, valid until the next get() that evicts the atlas. cov is 8-bit coverage with
 * stride w, positioned so that (bearing_x, bearing_y) is the offset of the top-left corner from
 * the pen origin (y down). */
typedef struct {
    const uint8_t *cov;
    int w, h;
    int stride;          /* bytes per atlas row: cov[row*stride + x] */
    int bearing_x, bearing_y;
    int advance_px;
    int subpixel;
} WubuGlyphBitmap;

typedef struct {
    unsigned hits, misses, evictions, glyphs;
    unsigned atlas_used_bytes, atlas_total_bytes;
} WubuTextCacheStats;

/* atlas_w/atlas_h must be powers of two and >= 64; 512x512 is 256 KB of coverage, enough for
 * several hundred UI glyphs at once. Returns NULL when allocation fails or the size is invalid. */
WubuTextCache *wubu_text_cache_new(int atlas_w, int atlas_h);
void wubu_text_cache_free(WubuTextCache *c);

/* Get (rasterising if needed) the bitmap for gid at ppem and subpixel quarter. subpixel must be
 * 0..WUBU_TEXT_SUBPIX-1. Returns 1 on success, 0 if the glyph has no outline at that size or the
 * atlas could not be made to fit it. */
int wubu_text_cache_get(WubuTextCache *c, const WubuTtf *ttf, uint32_t gid, int ppem, int subpixel,
                        WubuGlyphBitmap *out);

void wubu_text_cache_stats(const WubuTextCache *c, WubuTextCacheStats *out);

/* Drop every cached glyph. Called automatically on atlas overflow; exposed for scale changes. */
void wubu_text_cache_reset(WubuTextCache *c);

#endif /* WUBU_TEXT_CACHE_H */
