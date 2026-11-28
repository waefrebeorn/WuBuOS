/* wubu_text_cache_test.c -- SLERM-2027 stage 3b gate.
 *
 * A cache is only correct if a hit is indistinguishable from a miss, so the central check compares
 * cached pixels against a fresh rasterisation at the same subpixel pen position, byte for byte.
 * Everything else here guards the things a cache silently gets wrong: treating two subpixel buckets
 * as one glyph, losing the bearing, miscounting hits, or growing without bound.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "wubu_text_cache.h"
#include "wubu_truetype.h"

static int passed, failed;

static void ok(int cond, const char *what)
{
    if (cond) passed++;
    else { failed++; printf("  FAIL %s\n", what); }
}

static uint8_t *slurp(const char *path, size_t *n)
{
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return NULL; }
    long sz = ftell(f);
    rewind(f);
    if (sz <= 0) { fclose(f); return NULL; }
    uint8_t *b = malloc((size_t)sz);
    if (!b) { fclose(f); return NULL; }
    size_t got = fread(b, 1, (size_t)sz, f);
    fclose(f);
    if (got != (size_t)sz) { free(b); return NULL; }
    *n = got;
    return b;
}

static int same_bitmap(const WubuGlyphBitmap *g, const uint8_t *tight, int w, int h)
{
    if (g->w != w || g->h != h) return 0;
    for (int row = 0; row < h; row++)
        if (memcmp(g->cov + (size_t)row * (size_t)g->stride, tight + (size_t)row * (size_t)w,
                   (size_t)w) != 0)
            return 0;
    return 1;
}

int main(void)
{
    /* ---- construction rules ---- */
    ok(wubu_text_cache_new(512, 512) != NULL, "creates a 512x512 atlas");
    WubuTextCache *bad1 = wubu_text_cache_new(100, 100);
    ok(bad1 == NULL, "refuses a non-power-of-two atlas");
    if (bad1) wubu_text_cache_free(bad1);
    WubuTextCache *bad2 = wubu_text_cache_new(32, 32);
    ok(bad2 == NULL, "refuses an atlas below 64px");
    if (bad2) wubu_text_cache_free(bad2);

    size_t n = 0;
    uint8_t *data = slurp("third_party/fonts/selawik.ttf", &n);
    if (!data) { printf("  FAIL cannot read the vendored face\n"); return 1; }
    const char *why = NULL;
    WubuTtf *ttf = wubu_ttf_open(data, n, &why);
    if (!ttf) { printf("  FAIL open: %s\n", why ? why : "?"); free(data); return 1; }

    WubuTextCache *c = wubu_text_cache_new(512, 512);
    uint32_t gid = wubu_ttf_glyph_index(ttf, 'A');

    /* ---- first get is a miss, second is a hit, and both describe the same bytes ---- */
    WubuGlyphBitmap a, b;
    ok(wubu_text_cache_get(c, ttf, gid, 20, 0, &a) == 1, "rasterises 'A' on first request");
    ok(wubu_text_cache_get(c, ttf, gid, 20, 0, &b) == 1, "serves 'A' on second request");
    ok(a.cov == b.cov && a.w == b.w && a.h == b.h, "second request hits the same atlas entry");
    ok(a.advance_px > 0 && a.advance_px == b.advance_px, "advance is preserved across the cache");

    WubuTextCacheStats st;
    wubu_text_cache_stats(c, &st);
    ok(st.misses == 1 && st.hits == 1, "counts one miss then one hit");
    ok(st.glyphs == 1, "stores exactly one glyph");
    ok(st.atlas_used_bytes > 0 && st.atlas_used_bytes <= st.atlas_total_bytes, "atlas accounting is sane");

    /* ---- a hit must equal a fresh rasterisation at the same pen position ---- */
    static WubuTtfPoint pts[WUBU_TTF_MAX_POINTS];
    static uint16_t ends[WUBU_TTF_MAX_CONTOURS];
    static uint8_t tight[256 * 256];
    int np = 0, nc = 0;
    ok(wubu_ttf_outline(ttf, gid, pts, WUBU_TTF_MAX_POINTS, &np, ends, WUBU_TTF_MAX_CONTOURS, &nc) == 1,
       "outline is available for the comparison");
    int w = 0, h = 0, bx = 0, by = 0;
    ok(wubu_ttf_raster_off(pts, np, ends, nc, wubu_ttf_units_per_em(ttf), 20, 0.0f, 0.0f,
                           tight, 256, 256, &w, &h, &bx, &by) == 1, "direct rasterisation succeeds");
    ok(same_bitmap(&a, tight, w, h), "cached bitmap is byte-identical to a fresh raster");
    ok(a.bearing_x == bx && a.bearing_y == by, "cache reports the same bearing as the rasteriser");

    /* ---- subpixel buckets are separate glyphs, not one glyph served four times ---- */
    WubuGlyphBitmap q[WUBU_TEXT_SUBPIX];
    int distinct = 1, differ = 0;
    for (int s = 0; s < WUBU_TEXT_SUBPIX; s++) {
        ok(wubu_text_cache_get(c, ttf, gid, 20, s, &q[s]) == 1, "serves each subpixel bucket");
        for (int j = 0; j < s; j++)
            if (q[s].cov == q[j].cov) distinct = 0;
    }
    ok(distinct, "each subpixel bucket owns its own entry");
    for (int s = 1; s < WUBU_TEXT_SUBPIX; s++) {
        int ww = q[0].w < q[s].w ? q[0].w : q[s].w;
        int hh = q[0].h < q[s].h ? q[0].h : q[s].h;
        for (int row = 0; row < hh; row++)
            if (memcmp(q[0].cov + (size_t)row * q[0].stride, q[s].cov + (size_t)row * q[s].stride,
                       (size_t)ww) != 0)
                differ = 1;
    }
    ok(differ, "subpixel positions produce different coverage, not a duplicate");
    ok(q[3].advance_px == q[0].advance_px, "advance does not change with subpixel bucket");

    wubu_text_cache_stats(c, &st);
    /* one miss for bucket 0 before the loop, a hit when the loop asks for bucket 0 again, then a
     * miss for each remaining bucket */
    ok(st.misses == WUBU_TEXT_SUBPIX && st.hits == 2, "miss/hit counts survive the bucket loop");

    /* ---- a tiny atlas must evict and keep working, not corrupt or silently return stale bytes ---- */
    WubuTextCache *small = wubu_text_cache_new(64, 64);
    ok(small != NULL, "creates a 64x64 atlas");
    int got = 0, gets = 0;
    for (unsigned cp = 0x41; cp < 0x5B; cp++) {
        WubuGlyphBitmap g;
        gets++;
        if (wubu_text_cache_get(small, ttf, wubu_ttf_glyph_index(ttf, cp), 20, 0, &g)) got++;
    }
    ok(got == gets, "every glyph is still served under pressure");
    WubuTextCacheStats sst;
    wubu_text_cache_stats(small, &sst);
    ok(sst.evictions > 0, "a 64x64 atlas actually evicted");
    ok(sst.hits + sst.misses == (unsigned)gets, "every request is counted exactly once");

    /* ---- reset is observable ---- */
    wubu_text_cache_reset(c);
    wubu_text_cache_stats(c, &st);
    ok(st.glyphs == 0 && st.atlas_used_bytes == 0, "reset drops every glyph");

    wubu_text_cache_free(small);
    wubu_text_cache_free(c);
    wubu_ttf_close(ttf);
    free(data);
    printf("wubu_text_cache: %d passed, %d failed\n", passed, failed);
    return failed ? 1 : 0;
}
