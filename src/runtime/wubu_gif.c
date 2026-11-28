/* wubu_gif.c -- see wubu_gif.h. LZW + palette + interlace, no allocation. */
#include "wubu_gif.h"
#include <string.h>
#include <stdlib.h>

static unsigned rd16(const uint8_t *p) { return (unsigned)p[0] | ((unsigned)p[1] << 8); }

typedef struct { const uint8_t *p; size_t n, pos; } Cur;

static int byte_at(Cur *c, uint8_t *out);

/* LZW data arrives as a chain of length-prefixed sub-blocks. Reading it through a
 * chain (instead of copying it into one buffer) is why this decoder has no size cap:
 * an 8 KB scratch buffer silently limited frames to 64x64. */
typedef struct {
    Cur c;
    size_t block_left;
    int done;
} Sub;

static int sub_byte(Sub *s, uint8_t *out) {
    for (;;) {
        if (s->block_left) {
            if (byte_at(&s->c, out)) return -1;
            s->block_left--;
            return 0;
        }
        uint8_t n;
        if (byte_at(&s->c, &n)) return -1;
        if (n == 0) { s->done = 1; return -1; }
        s->block_left = n;
    }
}

static int byte_at(Cur *c, uint8_t *out) {
    if (c->pos >= c->n) return -1;
    *out = c->p[c->pos++];
    return 0;
}

/* A dictionary read through prefix chains: LZW codes are emitted into a table of
 * (prefix, suffix) pairs, and decoding a code means walking back to a root. */
#define GIF_MAX_CODES 4096

typedef struct {
    uint16_t prefix[GIF_MAX_CODES];
    uint8_t  suffix[GIF_MAX_CODES];
    uint8_t  first[GIF_MAX_CODES];
    int      next;
    int      size;
} Dict;

static int lzw_decode(Sub *sub, int min_size,
                       void (*emit)(void *, uint8_t), void *ctx, size_t px_limit,
                       size_t *outlen) {
    Dict d;
    uint8_t stack[GIF_MAX_CODES];
    int clear = 1 << min_size, eoi = clear + 1;
    int code_size = min_size + 1, prev = -1;
    memset(&d, 0, sizeof d);
    d.next = eoi + 1;
    for (int i = 0; i < clear; i++) { d.prefix[i] = 0xFFFF; d.suffix[i] = (uint8_t)i; d.first[i] = (uint8_t)i; }

    uint32_t bitbuf = 0;
    int bitcnt = 0;
    for (;;) {
        while (bitcnt < code_size) {
            uint8_t b;
            if (sub_byte(sub, &b)) return *outlen ? WUBU_GIF_OK : WUBU_GIF_TRUNCATED;
            bitbuf |= (uint32_t)b << bitcnt;
            bitcnt += 8;
        }
        int code = (int)(bitbuf & ((1u << code_size) - 1));
        bitbuf >>= code_size;
        bitcnt -= code_size;
        if (code == clear) { code_size = min_size + 1; d.next = eoi + 1; prev = -1; continue; }
        if (code == eoi) break;
        if (code > d.next || (code == d.next && prev < 0)) return WUBU_GIF_CORRUPT;

        int emit_code = code;
        size_t sp = 0;
        if (code == d.next) {                      /* the KwKwK case */
            if (prev < 0) return WUBU_GIF_CORRUPT;
            stack[sp++] = d.first[prev];
            emit_code = prev;
        }
        while (emit_code >= clear) {
            if (emit_code >= d.next || sp >= sizeof stack) return WUBU_GIF_CORRUPT;
            stack[sp++] = d.suffix[emit_code];
            emit_code = d.prefix[emit_code];
        }
        stack[sp++] = (uint8_t)emit_code;

        if (d.next < GIF_MAX_CODES && prev >= 0) {
            d.prefix[d.next] = (uint16_t)prev;
            d.suffix[d.next] = stack[sp - 1];
            d.first[d.next] = d.first[prev];
            d.next++;
            if (d.next == (1 << code_size) && code_size < 12) code_size++;
        }
        while (sp) {
            if (*outlen >= px_limit) return WUBU_GIF_OVERFLOW;
            emit(ctx, stack[--sp]);
            (*outlen)++;
        }
        prev = code;
    }
    return WUBU_GIF_OK;
}

typedef struct {
    unsigned w, h;
    const uint8_t *gct;
    unsigned gct_len;
    int have_gct;
    uint8_t bg[3];      /* background colour from the screen descriptor; used by disposal 2 */
} Screen;

static int parse_header(const uint8_t *d, size_t len, Screen *s, Cur *c, int *err) {
    if (len < 13) { *err = WUBU_GIF_TRUNCATED; return -1; }
    if (memcmp(d, "GIF87a", 6) != 0 && memcmp(d, "GIF89a", 6) != 0) { *err = WUBU_GIF_FORMAT; return -1; }
    s->w = rd16(d + 6);
    s->h = rd16(d + 8);
    if (s->w == 0 || s->h == 0) { *err = WUBU_GIF_CORRUPT; return -1; }
    uint8_t packed = d[10];
    s->have_gct = (packed & 0x80) != 0;
    if (s->have_gct) {
        s->gct_len = 3u << ((packed & 0x07) + 1);
        if (13 + s->gct_len > len) { *err = WUBU_GIF_TRUNCATED; return -1; }
        s->gct = d + 13;
    }
    /* GIF89a 20c: disposal 2 restores the frame rect to the BACKGROUND COLOUR, not to black.
     * Pillow composites those pixels as (bg_r, bg_g, bg_b, 0) -- transparent, but carrying the
     * background's RGB. Matching that exactly is what the oracle compares, byte for byte. */
    s->bg[0] = s->bg[1] = s->bg[2] = 0;
    if (s->have_gct) {
        unsigned bi = d[11];
        if (bi * 3 + 2 < s->gct_len) { s->bg[0] = s->gct[bi * 3]; s->bg[1] = s->gct[bi * 3 + 1]; s->bg[2] = s->gct[bi * 3 + 2]; }
    }
    c->p = d; c->n = len; c->pos = 13 + (s->have_gct ? s->gct_len : 0);
    return 0;
}

/* Walk blocks until the first image descriptor. Returns 0 with the descriptor's
 * fields, filling transparent_index when a graphic control extension said so. */
typedef struct {
    unsigned left, top, w, h;
    const uint8_t *lct;
    unsigned lct_len;
    int interlace;
    int transparent_index;   /* -1 when none */
    unsigned delay_cs;       /* graphic control delay, hundredths of a second */
    int disposal;            /* 0/1 leave, 2 restore background, 3 restore previous */
} Image;

static int skip_subblocks(Cur *c) {
    for (;;) {
        uint8_t n;
        if (byte_at(c, &n)) return WUBU_GIF_TRUNCATED;
        if (n == 0) return 0;
        if (c->pos + n > c->n) return WUBU_GIF_TRUNCATED;
        c->pos += n;
    }
}

static int find_image_ex(Cur *c, Image *img, int *ended, int *err) {
    img->transparent_index = -1;
    img->delay_cs = 0;
    img->disposal = 0;
    for (;;) {
        uint8_t tag;
        if (byte_at(c, &tag)) { *err = WUBU_GIF_TRUNCATED; return -1; }
        if (tag == 0x3B) { *ended = 1; return 1; }             /* trailer: no more images */
        if (tag == 0x21) {
            uint8_t label;
            if (byte_at(c, &label)) { *err = WUBU_GIF_TRUNCATED; return -1; }
            if (label == 0xF9) {                                   /* graphic control */
                uint8_t size;
                if (byte_at(c, &size)) { *err = WUBU_GIF_TRUNCATED; return -1; }
                if (size >= 4) {
                    /* Layout is packed, delay(2), transparent index -- reading the
                     * index right after `packed` picks up the delay's low byte, which is
                     * 0, so every frame looked like "colour 0 is transparent". */
                    uint8_t packed, tidx;
                    if (byte_at(c, &packed)) { *err = WUBU_GIF_TRUNCATED; return -1; }
                    if (c->pos + 2 > c->n) { *err = WUBU_GIF_TRUNCATED; return -1; }
                    unsigned dly = rd16(c->p + c->pos);
                    c->pos += 2;                                    /* delay */
                    img->delay_cs = dly;
                    img->disposal = (packed >> 2) & 0x7;
                    if (byte_at(c, &tidx)) { *err = WUBU_GIF_TRUNCATED; return -1; }
                    if (c->pos + size - 4 > c->n) { *err = WUBU_GIF_TRUNCATED; return -1; }
                    c->pos += size - 4;
                    if (packed & 0x01) img->transparent_index = tidx;
                } else {
                    if (c->pos + size > c->n) { *err = WUBU_GIF_TRUNCATED; return -1; }
                    c->pos += size;
                }
                if (skip_subblocks(c)) { *err = WUBU_GIF_TRUNCATED; return -1; }
            } else {
                if (skip_subblocks(c)) { *err = WUBU_GIF_TRUNCATED; return -1; }
            }
            continue;
        }
        if (tag != 0x2C) { *err = WUBU_GIF_CORRUPT; return -1; }
        if (c->pos + 9 > c->n) { *err = WUBU_GIF_TRUNCATED; return -1; }
        img->left = rd16(c->p + c->pos);
        img->top  = rd16(c->p + c->pos + 2);
        img->w    = rd16(c->p + c->pos + 4);
        img->h    = rd16(c->p + c->pos + 6);
        uint8_t packed = c->p[c->pos + 8];
        c->pos += 9;
        img->interlace = (packed & 0x40) != 0;
        img->lct = NULL; img->lct_len = 0;
        if (packed & 0x80) {
            img->lct_len = 3u << ((packed & 0x07) + 1);
            if (c->pos + img->lct_len > c->n) { *err = WUBU_GIF_TRUNCATED; return -1; }
            img->lct = c->p + c->pos;
            c->pos += img->lct_len;
        }
        return 0;
    }
}

static int find_image(Cur *c, Image *img, int *err) {
    int ended = 0;
    int rc = find_image_ex(c, img, &ended, err);
    if (rc == 1) { *err = WUBU_GIF_CORRUPT; return -1; }
    return rc;
}

int wubu_gif_info(const void *data, size_t len, WubuGif *info, int *err) {
    if (err) *err = WUBU_GIF_OK;
    if (!data || !info || len < 13) { if (err) *err = WUBU_GIF_ARGS; return -1; }
    Screen s; Cur c; int e = 0;
    if (parse_header((const uint8_t *)data, len, &s, &c, &e)) { if (err) *err = e; return -1; }
    Image img;
    if (find_image(&c, &img, &e)) { if (err) *err = e; return -1; }
    info->width = (int)s.w; info->height = (int)s.h; info->channels = 4;
    info->interlaced = img.interlace;
    info->transparent = img.transparent_index >= 0;
    return 0;
}

/* Row order for an interlaced frame: four passes over the frame's own rows. */
static unsigned row_for(unsigned pass_row, unsigned h, int interlace) {
    if (!interlace) return pass_row;
    static const unsigned start[4] = { 0, 4, 2, 1 }, step[4] = { 8, 8, 4, 2 };
    unsigned row = 0, count = 0;
    for (int p = 0; p < 4; p++)
        for (unsigned y = start[p]; y < h; y += step[p]) {
            if (count == pass_row) return y;
            count++;
        }
    (void)row;
    return pass_row;
}

typedef struct {
    uint8_t *out;
    const Screen *s;
    const Image *img;
    const uint8_t *pal;
    size_t pal_len;
    size_t i;          /* index counter, in stream order */
} Emit;

static void emit_index(void *ctx, uint8_t ci) {
    Emit *e = (Emit *)ctx;
    unsigned x = (unsigned)(e->i % (size_t)e->img->w);
    unsigned ry = (unsigned)(e->i / (size_t)e->img->w);
    e->i++;
    unsigned dy = row_for(ry, e->img->h, e->img->interlace);
    unsigned y = e->img->top + dy, dx = e->img->left + x;
    if (x >= e->img->w || ry >= e->img->h || y >= e->s->h || dx >= e->s->w) return;
    uint8_t *o = e->out + ((size_t)y * e->s->w + dx) * 4;
    if ((int)ci == e->img->transparent_index) { o[0] = o[1] = o[2] = o[3] = 0; return; }
    size_t off = (size_t)ci * 3;
    if (off + 2 >= e->pal_len) { o[3] = 0; return; }     /* index outside the palette */
    o[0] = e->pal[off]; o[1] = e->pal[off + 1]; o[2] = e->pal[off + 2]; o[3] = 255;
}

int wubu_gif_decode(const void *data, size_t len, uint8_t *out, size_t outcap,
                    WubuGif *info, int *err) {
    if (err) *err = WUBU_GIF_OK;
    if (!data || !out || !info || len < 13) { if (err) *err = WUBU_GIF_ARGS; return -1; }
    Screen s; Cur c; int e = 0;
    if (parse_header((const uint8_t *)data, len, &s, &c, &e)) { if (err) *err = e; return -1; }
    Image img;
    if (find_image(&c, &img, &e)) { if (err) *err = e; return -1; }
    if (img.w == 0 || img.h == 0) { if (err) *err = WUBU_GIF_CORRUPT; return -1; }
    size_t need = (size_t)s.w * s.h * 4;
    if (outcap < need) { if (err) *err = WUBU_GIF_OVERFLOW; return -1; }
    memset(out, 0, need);                       /* wherever the frame is not: transparent */

    const uint8_t *pal = img.lct ? img.lct : s.gct;
    size_t pal_len = img.lct ? img.lct_len : s.gct_len;
    if (!pal || pal_len == 0) { if (err) *err = WUBU_GIF_CORRUPT; return -1; }

    uint8_t min_size;
    if (byte_at(&c, &min_size)) { if (err) *err = WUBU_GIF_TRUNCATED; return -1; }
    if (min_size < 1 || min_size > 11) { if (err) *err = WUBU_GIF_CORRUPT; return -1; }

    Sub sub = { c, 0, 0 };
    Emit em = { out, &s, &img, pal, pal_len, 0 };
    size_t produced = 0;
    size_t frame_px = (size_t)img.w * img.h;
    int rc = lzw_decode(&sub, min_size, emit_index, &em, frame_px, &produced);
    if (rc != WUBU_GIF_OK) { if (err) *err = rc; return -1; }
    if (produced != frame_px) { if (err) *err = WUBU_GIF_TRUNCATED; return -1; }

    info->width = (int)s.w; info->height = (int)s.h; info->channels = 4;
    info->interlaced = img.interlace;
    info->transparent = img.transparent_index >= 0;
    return 0;
}


/* ---- animation ---------------------------------------------------------- */
/* A GIF is a canvas plus frames that are PATCHED onto it. Decoding one frame and
 * stopping (which is what wubu_gif_decode does) is right for a still, and wrong for
 * an animation: frame 2 is often only the changed rectangle. Compositing needs the
 * frame's disposal method, which says what happens to the canvas AFTER it is shown. */

static int skip_image_data(Cur *c) {
    uint8_t min_size;
    if (byte_at(c, &min_size)) return WUBU_GIF_TRUNCATED;
    if (min_size < 1 || min_size > 11) return WUBU_GIF_CORRUPT;
    return skip_subblocks(c);
}

/* Like emit_index, but a transparent pixel LEAVES the canvas alone. Clearing it, which is
 * right for a single frame on a blank canvas, erases whatever an earlier frame drew. */
static void emit_over(void *ctx, uint8_t ci) {
    Emit *e = (Emit *)ctx;
    unsigned x = (unsigned)(e->i % (size_t)e->img->w);
    unsigned ry = (unsigned)(e->i / (size_t)e->img->w);
    e->i++;
    if ((int)ci == e->img->transparent_index) return;
    unsigned dy = row_for(ry, e->img->h, e->img->interlace);
    unsigned y = e->img->top + dy, dx = e->img->left + x;
    if (x >= e->img->w || ry >= e->img->h || y >= e->s->h || dx >= e->s->w) return;
    size_t off = (size_t)ci * 3;
    if (off + 2 >= e->pal_len) return;
    uint8_t *o = e->out + ((size_t)y * e->s->w + dx) * 4;
    o[0] = e->pal[off]; o[1] = e->pal[off + 1]; o[2] = e->pal[off + 2]; o[3] = 255;
}

/* Decode one frame's pixels onto the canvas; advances c past the frame's data. */
static int decode_over(Cur *c, const Screen *s, const Image *img, uint8_t *canvas) {
    const uint8_t *pal = img->lct ? img->lct : s->gct;
    size_t pal_len = img->lct ? img->lct_len : s->gct_len;
    if (!pal || pal_len == 0) return WUBU_GIF_CORRUPT;
    uint8_t min_size;
    if (byte_at(c, &min_size)) return WUBU_GIF_TRUNCATED;
    if (min_size < 1 || min_size > 11) return WUBU_GIF_CORRUPT;
    Sub sub = { *c, 0, 0 };
    Emit em = { canvas, s, img, pal, pal_len, 0 };
    size_t produced = 0, frame_px = (size_t)img->w * img->h;
    int rc = lzw_decode(&sub, min_size, emit_over, &em, frame_px, &produced);
    /* lzw_decode stops as soon as it has produced w*h pixels, which is usually BEFORE the
     * end-of-information code and the zero terminator. Leaving the cursor there made the next
     * image descriptor be read out of compressed data -- frame 0 decoded, frames 1+ "damaged". */
    { uint8_t sink; while (sub_byte(&sub, &sink) == 0) { } }
    *c = sub.c;                                   /* now c sits after the terminator */
    if (rc != WUBU_GIF_OK) return rc;
    if (produced != frame_px) return WUBU_GIF_TRUNCATED;
    return WUBU_GIF_OK;
}

/* Clear or restore the frame rectangle. Disposal 2 means "the area returns to the
 * background", which in RGBA is fully transparent. */
static void clear_rect(uint8_t *canvas, const Screen *s, const Image *img) {
    for (unsigned y = img->top; y < img->top + img->h && y < s->h; y++)
        for (unsigned x = img->left; x < img->left + img->w && x < s->w; x++) {
            uint8_t *o = canvas + ((size_t)y * s->w + x) * 4;
            o[0] = s->bg[0]; o[1] = s->bg[1]; o[2] = s->bg[2]; o[3] = 0;
        }
}

/* The canvas before any frame is the GIF's background: its RGB with alpha 0. Starting from
 * black instead made disposal 3 restore black where Pillow (and the spec) say background --
 * a difference invisible on screen but plainly visible in the bytes. */
static void fill_bg(uint8_t *canvas, const Screen *s, size_t px) {
    for (size_t i = 0; i < px; i++) {
        canvas[i * 4 + 0] = s->bg[0];
        canvas[i * 4 + 1] = s->bg[1];
        canvas[i * 4 + 2] = s->bg[2];
        canvas[i * 4 + 3] = 0;
    }
}

static void restore_rect(uint8_t *canvas, const uint8_t *saved, const Screen *s, const Image *img) {
    for (unsigned y = img->top; y < img->top + img->h && y < s->h; y++) {
        size_t row = ((size_t)y * s->w + img->left) * 4;
        unsigned n = img->w;
        if (img->left + n > s->w) n = s->w - img->left;
        memcpy(canvas + row, saved + row, (size_t)n * 4);
    }
}

int wubu_gif_frame_count(const void *data, size_t len, int *count, int *err) {
    if (err) *err = WUBU_GIF_OK;
    if (!data || !count || len < 13) { if (err) *err = WUBU_GIF_ARGS; return -1; }
    Screen s; Cur c; int e = 0;
    if (parse_header((const uint8_t *)data, len, &s, &c, &e)) { if (err) *err = e; return -1; }
    int n = 0;
    for (;;) {
        Image img; int ended = 0;
        int rc = find_image_ex(&c, &img, &ended, &e);
        if (rc < 0) { if (err) *err = e; return -1; }
        if (rc == 1) break;
        if ((e = skip_image_data(&c)) != WUBU_GIF_OK) { if (err) *err = e; return -1; }
        n++;
    }
    if (n == 0) { if (err) *err = WUBU_GIF_CORRUPT; return -1; }
    *count = n;
    return 0;
}

int wubu_gif_frame_delay(const void *data, size_t len, int index, int *err) {
    if (err) *err = WUBU_GIF_OK;
    if (!data || index < 0 || len < 13) { if (err) *err = WUBU_GIF_ARGS; return -1; }
    Screen s; Cur c; int e = 0;
    if (parse_header((const uint8_t *)data, len, &s, &c, &e)) { if (err) *err = e; return -1; }
    for (int i = 0;; i++) {
        Image img; int ended = 0;
        int rc = find_image_ex(&c, &img, &ended, &e);
        if (rc < 0) { if (err) *err = e; return -1; }
        if (rc == 1) { if (err) *err = WUBU_GIF_CORRUPT; return -1; }
        if (i == index) return (int)img.delay_cs * 10;      /* GIF counts hundredths */
        if ((e = skip_image_data(&c)) != WUBU_GIF_OK) { if (err) *err = e; return -1; }
    }
}

int wubu_gif_frame(const void *data, size_t len, int index, uint8_t *out, size_t outcap, int *err) {
    if (err) *err = WUBU_GIF_OK;
    if (!data || !out || index < 0 || len < 13) { if (err) *err = WUBU_GIF_ARGS; return -1; }
    Screen s; Cur c; int e = 0;
    if (parse_header((const uint8_t *)data, len, &s, &c, &e)) { if (err) *err = e; return -1; }
    size_t need = (size_t)s.w * s.h * 4;
    if (outcap < need) { if (err) *err = WUBU_GIF_OVERFLOW; return -1; }
    fill_bg(out, &s, (size_t)s.w * s.h);
    uint8_t *saved = (uint8_t *)malloc(need);   /* only needed for disposal 3 */
    for (int i = 0;; i++) {
        Image img; int ended = 0;
        int rc = find_image_ex(&c, &img, &ended, &e);
        if (rc < 0) { free(saved); if (err) *err = e; return -1; }
        if (rc == 1) { free(saved); if (err) *err = WUBU_GIF_CORRUPT; return -1; }   /* index past the end */
        if (img.w == 0 || img.h == 0) { free(saved); if (err) *err = WUBU_GIF_CORRUPT; return -1; }
        if (img.disposal == 3 && saved) memcpy(saved, out, need);
        if ((rc = decode_over(&c, &s, &img, out)) != WUBU_GIF_OK) { free(saved); if (err) *err = rc; return -1; }
        if (i == index) { free(saved); return 0; }              /* the canvas as SHOWN for this frame */
        if (img.disposal == 2) clear_rect(out, &s, &img);
        else if (img.disposal == 3 && saved) restore_rect(out, saved, &s, &img);
    }
}

const char *wubu_gif_reason(int err) {
    switch (err) {
        case WUBU_GIF_OK:        return "ok";
        case WUBU_GIF_ARGS:      return "bad arguments";
        case WUBU_GIF_FORMAT:    return "not a GIF";
        case WUBU_GIF_TRUNCATED: return "that GIF is truncated";
        case WUBU_GIF_CORRUPT:   return "that GIF is damaged";
        case WUBU_GIF_OVERFLOW:  return "that GIF is larger than this decoder accepts";
        default:                 return "invalid";
    }
}
