/*
 * wubu_palette.c -- see wubu_palette.h.
 */
#include "wubu_palette.h"

#include <stddef.h>

/* 4 bits per channel -> 16*16*16 = 4096 buckets.  Small enough to live on the
 * stack, coarse enough that a photograph's noise does not fragment the
 * histogram into thousands of singleton colours. */
#define PAL_BUCKETS 4096

typedef struct {
    uint64_t r, g, b; /* running sums, so the representative is the AVERAGE of
                         everything in the bucket rather than its corner */
    uint32_t count;
} PalBucket;

static inline int pal_bucket_of(uint32_t rgb) {
    unsigned r = ((rgb >> 16) & 0xFF) >> 4;
    unsigned g = ((rgb >>  8) & 0xFF) >> 4;
    unsigned b = ( rgb        & 0xFF) >> 4;
    return (int)((r << 8) | (g << 4) | b);
}


/* The bucket's average colour. */
static inline uint32_t pal_bucket_avg(const PalBucket *bk) {
    uint32_t n = bk->count ? bk->count : 1;
    return ((uint32_t)(bk->r / n) << 16) |
           ((uint32_t)(bk->g / n) <<  8) |
            (uint32_t)(bk->b / n);
}

/* Saturation as the largest minus the smallest channel, 0..255.  A vivid
 * accent needs HEADROOM from grey; a photograph of a grey wall has none. */
static inline int pal_saturation(uint32_t rgb) {
    unsigned r = (rgb >> 16) & 0xFF;
    unsigned g = (rgb >>  8) & 0xFF;
    unsigned b =  rgb        & 0xFF;
    unsigned hi = r > g ? (r > b ? r : b) : (g > b ? g : b);
    unsigned lo = r < g ? (r < b ? r : b) : (g < b ? g : b);
    return (int)(hi - lo);
}

bool wubu_palette_extract(const uint32_t *px, int w, int h, int step,
                          WubuPalette *out) {
    if (!px || !out || w <= 0 || h <= 0 || step < 1) return false;

    static PalBucket buckets[PAL_BUCKETS];
    for (int i = 0; i < PAL_BUCKETS; i++)
        buckets[i] = (PalBucket){ .r = 0, .g = 0, .b = 0, .count = 0 };

    uint32_t samples = 0;
    for (int y = 0; y < h; y += step) {
        const uint32_t *row = px + (size_t)y * (size_t)w;
        for (int x = 0; x < w; x += step) {
            uint32_t c = row[x] & 0xFFFFFFu;
            int b = pal_bucket_of(c);
            buckets[b].r += (c >> 16) & 0xFF;
            buckets[b].g += (c >>  8) & 0xFF;
            buckets[b].b +=  c        & 0xFF;
            buckets[b].count++;
            samples++;
        }
    }
    if (samples == 0) return false;

    /* Dominant bucket -> base. */
    uint32_t base_count = 0;
    uint32_t base = 0x808080u;
    for (int i = 0; i < PAL_BUCKETS; i++) {
        if (buckets[i].count > base_count) {
            base_count = buckets[i].count;
            base = pal_bucket_avg(&buckets[i]);
        }
    }

    /* Most saturated bucket with a real presence -> accent.  2% keeps a
     * single vivid pixel (a logo, a cursor) from restyling the desktop. */
    uint32_t need = samples / 50u;
    if (need < 8) need = 8;

    int best = -1, best_sat = -1;
    int second = -1, second_sat = -1;
    for (int i = 0; i < PAL_BUCKETS; i++) {
        if (buckets[i].count < need) continue;
        uint32_t col = pal_bucket_avg(&buckets[i]);
        int sat = pal_saturation(col);
        if (sat <= 0) continue;                 /* grey carries no hue */
        if (sat > best_sat) {
            second = best; second_sat = best_sat;
            best = i; best_sat = sat;
        } else if (sat > second_sat && i != best) {
            second = i; second_sat = sat;
        }
    }

    if (best < 0) {
        /* No vivid colour with presence: the scheme IS the base. */
        out->base = base;
        out->accent = base;
        out->accent2 = base;
        out->vibrant = 0;
        return false;
    }

    out->base = base;
    out->accent = pal_bucket_avg(&buckets[best]);
    out->accent2 = (second >= 0) ? pal_bucket_avg(&buckets[second])
                                 : pal_bucket_avg(&buckets[best]);
    out->vibrant = 1;
    return true;
}

uint32_t wubu_palette_shade(uint32_t color, int percent) {
    if (percent < -100) percent = -100;
    if (percent >  100) percent =  100;

    int r = (int)((color >> 16) & 0xFF);
    int g = (int)((color >>  8) & 0xFF);
    int b = (int)( color        & 0xFF);

    /* Positive: toward white.  Negative: toward black. */
    if (percent >= 0) {
        int d = percent * 255 / 100;
        r += (255 - r) * d / 255;
        g += (255 - g) * d / 255;
        b += (255 - b) * d / 255;
    } else {
        int d = -percent * 255 / 100;
        r -= r * d / 255;
        g -= g * d / 255;
        b -= b * d / 255;
    }
    return ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
}
