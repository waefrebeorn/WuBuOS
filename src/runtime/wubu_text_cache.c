/* wubu_text_cache.c -- glyph cache with subpixel-bucketed keys over a shelf-packed atlas. */
#include <stdlib.h>
#include <string.h>

#include "wubu_text_cache.h"

#define SLOTS 1024

typedef struct {
    uint32_t gid;
    int16_t ppem;
    int8_t sub;
    int8_t used;
    int16_t x, y, w, h;
    int16_t bx, by;      /* bearing of the box from the pen origin, needed to place the glyph */
} Entry;

typedef struct {
    int y, h, x;   /* shelf band: top, height, next free x */
} Shelf;

struct WubuTextCache {
    int aw, ah;
    uint8_t *atlas;
    uint8_t *tmp;          /* scratch for one rasterised glyph before packing */
    int tmp_w, tmp_h;
    Entry *slots;
    Shelf *shelves;
    int nshelves;
    unsigned hits, misses, evictions, glyphs;
};

static unsigned slot_hash(uint32_t gid, int ppem, int sub)
{
    uint32_t h = gid * 2654435761u;
    h ^= (uint32_t)ppem * 40503u;
    h ^= (uint32_t)(sub + 1) * 2246822519u;
    return h & (SLOTS - 1);
}

WubuTextCache *wubu_text_cache_new(int atlas_w, int atlas_h)
{
    if (atlas_w < 64 || atlas_h < 64) return NULL;
    if (atlas_w > 4096 || atlas_h > 4096) return NULL;
    if ((atlas_w & (atlas_w - 1)) || (atlas_h & (atlas_h - 1))) return NULL;

    WubuTextCache *c = calloc(1, sizeof *c);
    if (!c) return NULL;
    c->aw = atlas_w;
    c->ah = atlas_h;
    c->atlas = calloc((size_t)atlas_w * (size_t)atlas_h, 1);
    c->slots = calloc(SLOTS, sizeof *c->slots);
    c->shelves = calloc(64, sizeof *c->shelves);
    c->tmp_w = 256;
    c->tmp_h = 256;
    c->tmp = malloc((size_t)c->tmp_w * (size_t)c->tmp_h);
    if (!c->atlas || !c->slots || !c->shelves || !c->tmp) {
        free(c->atlas); free(c->slots); free(c->shelves); free(c->tmp); free(c);
        return NULL;
    }
    return c;
}

void wubu_text_cache_free(WubuTextCache *c)
{
    if (!c) return;
    free(c->atlas); free(c->slots); free(c->shelves); free(c->tmp);
    free(c);
}

void wubu_text_cache_reset(WubuTextCache *c)
{
    if (!c) return;
    memset(c->atlas, 0, (size_t)c->aw * (size_t)c->ah);
    memset(c->slots, 0, SLOTS * sizeof *c->slots);
    c->nshelves = 0;
    c->glyphs = 0;
}

void wubu_text_cache_stats(const WubuTextCache *c, WubuTextCacheStats *out)
{
    if (!out) return;
    memset(out, 0, sizeof *out);
    if (!c) return;
    out->hits = c->hits;
    out->misses = c->misses;
    out->evictions = c->evictions;
    out->glyphs = c->glyphs;
    out->atlas_total_bytes = (unsigned)((size_t)c->aw * (size_t)c->ah);
    for (int i = 0; i < c->nshelves; i++)
        out->atlas_used_bytes += (unsigned)c->shelves[i].h * (unsigned)c->aw;
}

/* Shelf packing: keep a horizontal strip per glyph height and fill it left to right. Glyph heights
 * cluster tightly (ascending vs descending, one or two ppem bands), so bands waste little. */
static int pack(WubuTextCache *c, int w, int h, int *ox, int *oy)
{
    for (int i = 0; i < c->nshelves; i++) {
        Shelf *s = &c->shelves[i];
        if (h <= s->h && s->x + w <= c->aw) {
            *ox = s->x;
            *oy = s->y;
            s->x += w;
            return 1;
        }
    }
    int top = 0;
    for (int i = 0; i < c->nshelves; i++)
        if (c->shelves[i].y + c->shelves[i].h > top) top = c->shelves[i].y + c->shelves[i].h;
    if (top + h > c->ah) return 0;
    if (c->nshelves >= 64) return 0;
    c->shelves[c->nshelves].y = top;
    c->shelves[c->nshelves].h = h;
    c->shelves[c->nshelves].x = w;   /* advance past the glyph that created the band */
    *ox = 0;
    *oy = top;
    c->nshelves++;
    return 1;
}

int wubu_text_cache_get(WubuTextCache *c, const WubuTtf *ttf, uint32_t gid, int ppem, int subpixel,
                        WubuGlyphBitmap *out)
{
    if (!c || !ttf || !out || ppem <= 0) return 0;
    if (subpixel < 0 || subpixel >= WUBU_TEXT_SUBPIX) return 0;

    unsigned idx = slot_hash(gid, ppem, subpixel);
    for (unsigned probe = 0; probe < SLOTS; probe++) {
        Entry *e = &c->slots[(idx + probe) & (SLOTS - 1)];
        if (!e->used) break;
        if (e->used && e->gid == gid && e->ppem == ppem && e->sub == subpixel) {
            c->hits++;
            out->cov = c->atlas + (size_t)e->y * (size_t)c->aw + (size_t)e->x;
            out->w = e->w;
            out->h = e->h;
            out->stride = c->aw;
            out->subpixel = subpixel;
            out->advance_px = wubu_ttf_advance_px(ttf, gid, ppem);
            out->bearing_x = e->bx;
            out->bearing_y = e->by;
            return 1;
        }
    }

    /* miss: build it */
    static WubuTtfPoint pts[WUBU_TTF_MAX_POINTS];
    static uint16_t ends[WUBU_TTF_MAX_CONTOURS];
    int np = 0, nc = 0;
    if (!wubu_ttf_outline(ttf, gid, pts, WUBU_TTF_MAX_POINTS, &np, ends, WUBU_TTF_MAX_CONTOURS, &nc))
        return 0;

    int w = 0, h = 0, bx = 0, by = 0;
    float pen = (float)subpixel / (float)WUBU_TEXT_SUBPIX;
    if (!wubu_ttf_raster_off(pts, np, ends, nc, wubu_ttf_units_per_em(ttf), ppem, pen, 0.0f,
                             c->tmp, c->tmp_w, c->tmp_h, &w, &h, &bx, &by))
        return 0;

    if (c->glyphs >= SLOTS - 1) { c->evictions++; wubu_text_cache_reset(c); }

    int ox = 0, oy = 0;
    if (!pack(c, w, h, &ox, &oy)) {
        /* Whole-atlas replacement: cheap, rare at UI sizes, and simpler than per-glyph LRU. */
        c->evictions++;
        wubu_text_cache_reset(c);
        if (!pack(c, w, h, &ox, &oy)) return 0;
    }
    for (int row = 0; row < h; row++)
        memcpy(c->atlas + (size_t)(oy + row) * (size_t)c->aw + (size_t)ox,
               c->tmp + (size_t)row * (size_t)w, (size_t)w);

    /* store entry */
    unsigned slot = idx;
    for (unsigned probe = 0; probe < SLOTS; probe++) {
        Entry *e = &c->slots[(slot + probe) & (SLOTS - 1)];
        if (!e->used) {
            e->used = 1;
            e->gid = gid;
            e->ppem = (int16_t)ppem;
            e->sub = (int8_t)subpixel;
            e->x = (int16_t)ox;
            e->y = (int16_t)oy;
            e->w = (int16_t)w;
            e->h = (int16_t)h;
            e->bx = (int16_t)bx;
            e->by = (int16_t)by;
            c->glyphs++;
            break;
        }
    }

    c->misses++;
    out->cov = c->atlas + (size_t)oy * (size_t)c->aw + (size_t)ox;
    out->w = w;
    out->h = h;
    out->stride = c->aw;
    out->bearing_x = bx;
    out->bearing_y = by;
    out->subpixel = subpixel;
    out->advance_px = wubu_ttf_advance_px(ttf, gid, ppem);
    return 1;
}
