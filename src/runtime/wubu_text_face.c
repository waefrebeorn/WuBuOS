/* See wubu_text_face.h. One concern: turn a string + ppem into pixels on a framebuffer. */
#include "wubu_text_face.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "wubu_text_cache.h"
#include "wubu_truetype.h"
#include "wubu_utf8.h"

#define TF_ATLAS_W 512
#define TF_ATLAS_H 512
#define TF_SUBPP   WUBU_TEXT_SUBPIX     /* pen positions are quantised to 1/TF_SUBPP px */

struct WubuTextFace {
    WubuTtf *ttf;
    WubuTextCache *cache;
    uint8_t *owned;          /* file bytes, NULL when the caller owns them */
    size_t owned_len;
    int upem;
    uint32_t gid_question;
};

static void tf_free(WubuTextFace *f)
{
    if (!f) return;
    if (f->cache) wubu_text_cache_free(f->cache);
    if (f->ttf) wubu_ttf_close(f->ttf);
    free(f->owned);
    free(f);
}

static WubuTextFace *tf_adopt(WubuTtf *ttf, uint8_t *owned, size_t owned_len, const char **why)
{
    WubuTextFace *f = (WubuTextFace *)calloc(1, sizeof(*f));
    if (!f) {
        if (ttf) wubu_ttf_close(ttf);
        free(owned);
        if (why) *why = "out of memory";
        return NULL;
    }
    f->ttf = ttf;
    f->owned = owned;
    f->owned_len = owned_len;
    f->upem = (int)wubu_ttf_units_per_em(ttf);
    f->gid_question = wubu_ttf_glyph_index(ttf, '?');
    f->cache = wubu_text_cache_new(TF_ATLAS_W, TF_ATLAS_H);
    if (!f->cache) {
        tf_free(f);
        if (why) *why = "glyph cache allocation failed";
        return NULL;
    }
    return f;
}

WubuTextFace *wubu_text_face_open_memory(const uint8_t *data, size_t len, const char **why)
{
    if (!data || len == 0) {
        if (why) *why = "no font bytes";
        return NULL;
    }
    WubuTtf *ttf = wubu_ttf_open(data, len, why);
    if (!ttf) return NULL;
    return tf_adopt(ttf, NULL, 0, why);
}

WubuTextFace *wubu_text_face_open_file(const char *path, const char **why)
{
    if (!path) {
        if (why) *why = "no path";
        return NULL;
    }
    FILE *fp = fopen(path, "rb");
    if (!fp) {
        if (why) *why = "cannot open font file";
        return NULL;
    }
    if (fseek(fp, 0, SEEK_END) != 0) {
        fclose(fp);
        if (why) *why = "cannot seek font file";
        return NULL;
    }
    long n = ftell(fp);
    if (n <= 0 || n > (16 << 20)) {
        fclose(fp);
        if (why) *why = "font file size implausible";
        return NULL;
    }
    rewind(fp);
    uint8_t *buf = (uint8_t *)malloc((size_t)n);
    if (!buf) {
        fclose(fp);
        if (why) *why = "out of memory reading font";
        return NULL;
    }
    if (fread(buf, 1, (size_t)n, fp) != (size_t)n) {
        fclose(fp);
        free(buf);
        if (why) *why = "short read on font file";
        return NULL;
    }
    fclose(fp);

    WubuTtf *ttf = wubu_ttf_open(buf, (size_t)n, why);
    if (!ttf) {
        free(buf);
        return NULL;
    }
    return tf_adopt(ttf, buf, (size_t)n, why);
}

void wubu_text_face_close(WubuTextFace *f) { tf_free(f); }

/* Advance in quarter pixels. Rounding once per glyph in quarter units, then accumulating as
 * integers, keeps a 40-character line within a pixel of FreeType's fractional sum -- rounding each
 * glyph to whole pixels and adding those drifts visibly by the end of a line. */
static int tf_advance_q4(const WubuTextFace *f, uint32_t gid, int ppem)
{
    long units = (long)wubu_ttf_advance_units(f->ttf, gid);
    if (f->upem <= 0) return 0;
    long num = units * (long)ppem * TF_SUBPP + f->upem / 2;
    return (int)(num / f->upem);
}

static void tf_blend(uint32_t *fb, int fb_w, int fb_h, int x, int y,
                     const WubuGlyphBitmap *b, uint32_t rgb)
{
    const uint32_t r = (rgb >> 16) & 0xFF, g = (rgb >> 8) & 0xFF, bl = rgb & 0xFF;
    for (int row = 0; row < b->h; row++) {
        int py = y + b->bearing_y + row;
        if (py < 0 || py >= fb_h) continue;
        const uint8_t *src = b->cov + (size_t)row * (size_t)b->stride;
        for (int col = 0; col < b->w; col++) {
            int px = x + b->bearing_x + col;
            if (px < 0 || px >= fb_w) continue;
            unsigned a = src[col];
            if (!a) continue;
            uint32_t dst = fb[(size_t)py * (size_t)fb_w + (size_t)px];
            unsigned dr = (dst >> 16) & 0xFF, dg = (dst >> 8) & 0xFF, db = dst & 0xFF;
            unsigned nr = (r * a + dr * (255 - a)) / 255;
            unsigned ng = (g * a + dg * (255 - a)) / 255;
            unsigned nb = (bl * a + db * (255 - a)) / 255;
            fb[(size_t)py * (size_t)fb_w + (size_t)px] = (nr << 16) | (ng << 8) | nb;
        }
    }
}

int wubu_text_face_draw(WubuTextFace *f, uint32_t *fb, int fb_w, int fb_h,
                        int x, int baseline, const char *utf8, int ppem, uint32_t rgb)
{
    if (!f || !fb || !utf8 || ppem <= 0 || ppem > 512) return -1;

    size_t len = strlen(utf8);
    size_t i = 0;
    int pen_q4 = x * TF_SUBPP;

    while (i < len) {
        unsigned cp = 0;
        size_t used = wubu_utf8_decode(utf8 + i, len - i, &cp);
        if (used == 0) return -1;      /* malformed input is refused, not guessed at */
        i += used;

        uint32_t gid = wubu_ttf_glyph_index(f->ttf, cp);
        if (!gid) gid = f->gid_question;
        if (!gid) {
            pen_q4 += tf_advance_q4(f, 0, ppem);
            continue;
        }

        int adv = tf_advance_q4(f, gid, ppem);
        WubuGlyphBitmap b;
        int bucket = pen_q4 & (TF_SUBPP - 1);
        if (wubu_text_cache_get(f->cache, f->ttf, gid, ppem, bucket, &b))
            tf_blend(fb, fb_w, fb_h, pen_q4 / TF_SUBPP, baseline, &b, rgb);
        pen_q4 += adv;
    }
    /* Advance, not the absolute pen: measure() reports an advance, and a caller that adds one to x
     * must never have to subtract the origin back out. */
    return pen_q4 / TF_SUBPP - x;
}

int wubu_text_face_measure(WubuTextFace *f, const char *utf8, int ppem)
{
    if (!f || !utf8 || ppem <= 0 || ppem > 512) return -1;
    size_t len = strlen(utf8);
    size_t i = 0;
    int q4 = 0;
    while (i < len) {
        unsigned cp = 0;
        size_t used = wubu_utf8_decode(utf8 + i, len - i, &cp);
        if (used == 0) return -1;
        i += used;
        uint32_t gid = wubu_ttf_glyph_index(f->ttf, cp);
        if (!gid) gid = f->gid_question;
        if (gid) q4 += tf_advance_q4(f, gid, ppem);
    }
    return q4 / TF_SUBPP;
}

void wubu_text_face_stats(const WubuTextFace *f, unsigned *hits, unsigned *misses, unsigned *glyphs)
{
    WubuTextCacheStats st;
    memset(&st, 0, sizeof(st));
    if (f && f->cache) wubu_text_cache_stats(f->cache, &st);
    if (hits) *hits = st.hits;
    if (misses) *misses = st.misses;
    if (glyphs) *glyphs = st.glyphs;
}
