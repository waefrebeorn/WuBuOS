/* wubu_jpeg.c -- baseline JPEG decoder. See wubu_jpeg.h for the stated scope.
 *
 * Structure follows ITU T.81 rather than a compressed-blog version: markers, then
 * canonical Huffman decoding (Annex F), then dequantise + dezigzag + separable IDCT per
 * 8x8 block, then nearest-neighbour chroma upsampling. Restart intervals are honoured
 * because a truncated or damaged photo usually fails there first.
 */
#include "wubu_jpeg.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ZLEN 64
#define MAX_COMPS 4

static const int zz[ZLEN] = {
     0, 1, 8,16, 9, 2, 3,10,17,24,32,25,18,11, 4, 5,
    12,19,26,33,40,48,41,34,27,20,13, 6, 7,14,21,28,
    35,42,49,56,57,50,43,36,29,22,15,23,30,37,44,51,
    58,59,52,45,38,31,39,46,53,60,61,54,47,55,62,63
};

typedef struct {
    int      bits[17];      /* bits[l]: how many codes have length l (1..16) */
    int      mincode[17], maxcode[17], valptr[17];
    uint8_t  val[256];
    int      nval;
    int      valid;
} Huff;

typedef struct {
    int  id, hs, vs, tq, td, ta;
    int  dc_pred;
    int  pw, ph;            /* plane size in samples, padded to whole blocks */
    uint8_t *plane;
} Comp;

typedef struct {
    const uint8_t *p, *end;
    uint32_t buf;
    int      cnt;
    int      marker;        /* set when a real marker ended the entropy-coded segment */
    int      eof;           /* the buffer ran out: padding with zeros hides truncation */
} Bits;

/* ---------------------------------------------------------------- bit reader */

static int raw_byte(Bits *b) {
    if (b->p >= b->end) { b->eof = 1; return -1; }
    uint8_t c = *b->p++;
    if (c == 0xFF) {
        uint8_t nx = (b->p < b->end) ? *b->p : 0;
        if (nx == 0x00) { b->p++; return 0xFF; }        /* byte stuffing */
        b->marker = nx;                                 /* a real marker: stop */
        b->p--;                                         /* leave it for the marker reader */
        return -1;
    }
    return c;
}

static void fill(Bits *b) {
    while (b->cnt <= 24) {
        int c = raw_byte(b);
        if (c < 0) { b->buf <<= 8; b->cnt += 8; continue; }
        b->buf = (b->buf << 8) | (uint32_t)c;
        b->cnt += 8;
    }
}

static int get_bits(Bits *b, int n) {
    if (n == 0) return 0;
    fill(b);
    int v = (int)((b->buf >> (b->cnt - n)) & ((1u << n) - 1));
    b->cnt -= n;
    return v;
}

/* ---------------------------------------------------------------- Huffman */

static int huff_build(Huff *h) {
    int code = 0, k = 0;
    h->mincode[0] = h->maxcode[0] = -1;
    h->valptr[0] = 0;
    for (int l = 1; l <= 16; l++) {
        h->valptr[l] = k;
        h->mincode[l] = h->bits[l] ? code : 0;
        code += h->bits[l];
        h->maxcode[l] = h->bits[l] ? code - 1 : -1;
        if (code > (1 << l)) return 0;                  /* over-subscribed table */
        code <<= 1;
        k += h->bits[l];
    }
    return k == h->nval;                                /* every symbol reachable */
}

static int huff_decode(Bits *b, const Huff *h) {
    int code = 0;
    for (int l = 1; l <= 16; l++) {
        code = (code << 1) | get_bits(b, 1);
        if (h->bits[l] && code <= h->maxcode[l])
            return h->val[h->valptr[l] + code - h->mincode[l]];
    }
    return -1;
}

static int receive_extend(Bits *b, int n) {
    if (n == 0) return 0;
    int v = get_bits(b, n);
    int vt = 1 << (n - 1);
    if (v < vt) v -= (1 << n) - 1;
    return v;
}

/* ---------------------------------------------------------------- IDCT */

static int   idct_ready;
static float idct_cos[8][8];

static void idct_init(void) {
    if (idct_ready) return;
    for (int x = 0; x < 8; x++)
        for (int u = 0; u < 8; u++)
            idct_cos[x][u] = (float)cos((2 * x + 1) * u * 3.14159265358979323846 / 16.0);
    idct_ready = 1;
}

/* Separable IDCT on a dequantised block of coefficients already in natural order. */
static void idct_block(const float in[ZLEN], uint8_t out[ZLEN]) {
    float tmp[ZLEN], val;
    for (int y = 0; y < 8; y++)
        for (int x = 0; x < 8; x++) {
            val = 0.0f;
            for (int u = 0; u < 8; u++) {
                float cu = (u == 0) ? 0.3535533906f : 0.5f * idct_cos[x][u];
                val += cu * in[y * 8 + u];
            }
            tmp[y * 8 + x] = val;
        }
    for (int x = 0; x < 8; x++)
        for (int y = 0; y < 8; y++) {
            val = 0.0f;
            for (int v = 0; v < 8; v++) {
                float cv = (v == 0) ? 0.3535533906f : 0.5f * idct_cos[y][v];
                val += cv * tmp[v * 8 + x];
            }
            int s = (int)lroundf(val + 128.0f);
            out[y * 8 + x] = (uint8_t)(s < 0 ? 0 : (s > 255 ? 255 : s));
        }
}

/* Bilinear sample of a (possibly subsampled) plane. libjpeg upsamples chroma with a
 * triangle filter for a reason: nearest neighbour leaves a 2x2 stipple that is plainly
 * visible on any hard colour edge, and the gate measured exactly that (worst pixel 92
 * levels off against libjpeg before this existed). */
static float sample_bilinear(const Comp *c, float u, float v) {
    int x0 = (int)floorf(u), y0 = (int)floorf(v);
    float fx = u - (float)x0, fy = v - (float)y0;
    int x1 = x0 + 1, y1 = y0 + 1;
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > c->pw - 1) x1 = c->pw - 1;
    if (y1 > c->ph - 1) y1 = c->ph - 1;
    if (x0 > c->pw - 1) x0 = c->pw - 1;
    if (y0 > c->ph - 1) y0 = c->ph - 1;
    float a = c->plane[(size_t)y0 * c->pw + x0];
    float bq = c->plane[(size_t)y0 * c->pw + x1];
    float cq = c->plane[(size_t)y1 * c->pw + x0];
    float d = c->plane[(size_t)y1 * c->pw + x1];
    float top = a + (bq - a) * fx;
    float bot = cq + (d - cq) * fx;
    return top + (bot - top) * fy;
}

/* ---------------------------------------------------------------- decode */

const char *wubu_jpeg_sof_name(int m) {
    switch (m) {
        case 0xC0: return "baseline sequential";
        case 0xC1: return "extended sequential";
        case 0xC2: return "progressive";
        case 0xC3: return "lossless";
        case 0xC5: case 0xC6: return "differential";
        case 0xC9: case 0xCA: return "arithmetic-coded";
        case 0xCB: return "arithmetic lossless";
        default:   return "unknown";
    }
}

const char *wubu_jpeg_reason(int err) {
    switch (err) {
        case WUBU_JPEG_OK:          return "ok";
        case WUBU_JPEG_SHORT:       return "the file ends inside a structure";
        case WUBU_JPEG_NOT_JPEG:    return "not a JPEG stream";
        case WUBU_JPEG_UNSUPPORTED: return "this JPEG variant is not supported (progressive, arithmetic, 12-bit or CMYK)";
        case WUBU_JPEG_BAD_HUFFMAN: return "a Huffman table is incomplete or over-subscribed";
        case WUBU_JPEG_BAD_CODE:    return "the entropy-coded data uses a code no table covers";
        case WUBU_JPEG_BAD_TABLE:   return "a quantisation table is referenced but not defined";
        case WUBU_JPEG_ARGS:        return "bad arguments";
        case WUBU_JPEG_NOMEM:       return "out of memory";
        case WUBU_JPEG_SIZE:        return "the image size is zero or above the limit";
        default:                    return "invalid";
    }
}

void wubu_jpeg_free(WubuJpegImage *img) {
    if (!img) return;
    free(img->rgb);
    img->rgb = NULL;
    img->w = img->h = 0;
}

static int read_marker(Bits *b) {
    /* Skip fill bytes until 0xFF then the marker code. */
    while (b->p < b->end) {
        if (*b->p == 0xFF) {
            uint8_t m = (b->p + 1 < b->end) ? b->p[1] : 0;
            if (m == 0xFF) { b->p++; continue; }
            b->p += 2;
            return m;
        }
        b->p++;
    }
    return -1;
}

int wubu_jpeg_decode(const uint8_t *buf, size_t len, WubuJpegImage *out, size_t max_pixels, int *err) {
    if (err) *err = WUBU_JPEG_OK;
    if (!buf || !out) { if (err) *err = WUBU_JPEG_ARGS; return WUBU_JPEG_ARGS; }
    out->rgb = NULL; out->w = out->h = 0;
    if (len < 4 || buf[0] != 0xFF || buf[1] != 0xD8) { if (err) *err = WUBU_JPEG_NOT_JPEG; return WUBU_JPEG_NOT_JPEG; }
    if (!max_pixels) max_pixels = 64u * 1000u * 1000u;

    idct_init();
    Bits b; memset(&b, 0, sizeof b);
    b.p = buf + 2; b.end = buf + len;

    uint16_t qtab[4][ZLEN]; memset(qtab, 0, sizeof qtab);
    int qdef[4] = {0,0,0,0};
    Huff ht[2][4]; memset(ht, 0, sizeof ht);

    Comp comp[MAX_COMPS]; memset(comp, 0, sizeof comp);
    int ncomp = 0, width = 0, height = 0, hmax = 1, vmax = 1;
    int restart_interval = 0, have_sof = 0;
    int rc = WUBU_JPEG_OK;

    for (;;) {
        int m = read_marker(&b);
        if (m < 0) { rc = WUBU_JPEG_SHORT; break; }
        if (m == 0xD9) { if (!have_sof) { rc = WUBU_JPEG_NOT_JPEG; } break; }
        if (m == 0x01 || (m >= 0xD0 && m <= 0xD7)) continue;     /* standalone */
        if (b.p + 2 > b.end) { rc = WUBU_JPEG_SHORT; break; }
        size_t seglen = ((size_t)b.p[0] << 8) | b.p[1];
        if (seglen < 2 || b.p + seglen > b.end) { rc = WUBU_JPEG_SHORT; break; }
        const uint8_t *seg = b.p + 2;
        size_t slen = seglen - 2;
        const uint8_t *seg_end = b.p + seglen;

        switch (m) {
        case 0xDB: {                                             /* DQT */
            const uint8_t *q = seg;
            while (q < seg_end) {
                int pq = q[0] >> 4, tq = q[0] & 15;
                q++;
                if (tq > 3) { rc = WUBU_JPEG_BAD_TABLE; break; }
                for (int i = 0; i < ZLEN; i++) {
                    int v;
                    if (pq) { if (q + 2 > seg_end) { rc = WUBU_JPEG_SHORT; break; } v = (q[0] << 8) | q[1]; q += 2; }
                    else    { if (q + 1 > seg_end) { rc = WUBU_JPEG_SHORT; break; } v = *q++; }
                    qtab[tq][zz[i]] = (uint16_t)v;
                }
                qdef[tq] = 1;
            }
            break;
        }
        case 0xC4: {                                             /* DHT */
            const uint8_t *h = seg;
            while (h < seg_end) {
                int tc = h[0] >> 4, th = h[0] & 15;
                h++;
                if (tc > 1 || th > 3 || h + 16 > seg_end) { rc = WUBU_JPEG_BAD_HUFFMAN; break; }
                Huff *t = &ht[tc][th];
                memset(t, 0, sizeof *t);
                int total = 0;
                for (int l = 1; l <= 16; l++) { t->bits[l] = h[l-1]; total += h[l-1]; }
                h += 16;
                if (total > 256 || h + total > seg_end) { rc = WUBU_JPEG_BAD_HUFFMAN; break; }
                memcpy(t->val, h, (size_t)total);
                h += total;
                t->nval = total;
                if (!huff_build(t)) { rc = WUBU_JPEG_BAD_HUFFMAN; break; }
                t->valid = 1;
            }
            break;
        }
        case 0xDD:                                               /* DRI */
            if (slen < 2) { rc = WUBU_JPEG_SHORT; break; }
            restart_interval = (seg[0] << 8) | seg[1];
            break;
        case 0xC0: case 0xC1: case 0xC2: case 0xC3:
        case 0xC5: case 0xC6: case 0xC9: case 0xCA: case 0xCB: {
            if (m != 0xC0) { rc = WUBU_JPEG_UNSUPPORTED; break; }
            if (slen < 6) { rc = WUBU_JPEG_SHORT; break; }
            if (seg[0] != 8) { rc = WUBU_JPEG_UNSUPPORTED; break; }   /* 12-bit */
            height = (seg[1] << 8) | seg[2];
            width  = (seg[3] << 8) | seg[4];
            ncomp  = seg[5];
            if (ncomp != 1 && ncomp != 3) { rc = WUBU_JPEG_UNSUPPORTED; break; }  /* CMYK */
            if (slen < (size_t)(6 + ncomp * 3)) { rc = WUBU_JPEG_SHORT; break; }
            for (int i = 0; i < ncomp; i++) {
                comp[i].id = seg[6 + i*3];
                comp[i].hs = seg[7 + i*3] >> 4;
                comp[i].vs = seg[7 + i*3] & 15;
                comp[i].tq = seg[8 + i*3];
                if (comp[i].hs < 1 || comp[i].hs > 4 || comp[i].vs < 1 || comp[i].vs > 4) { rc = WUBU_JPEG_NOT_JPEG; break; }
                if (comp[i].hs > hmax) hmax = comp[i].hs;
                if (comp[i].vs > vmax) vmax = comp[i].vs;
            }
            have_sof = 1;
            break;
        }
        case 0xDA: {                                             /* SOS: decode the scan */
            if (!have_sof) { rc = WUBU_JPEG_NOT_JPEG; break; }
            if (slen < 1) { rc = WUBU_JPEG_SHORT; break; }
            int ns = seg[0];
            if (ns < 1 || ns > ncomp || slen < (size_t)(1 + ns * 2 + 3)) { rc = WUBU_JPEG_NOT_JPEG; break; }
            for (int i = 0; i < ns; i++) {
                int cid = seg[1 + i*2], t = seg[2 + i*2];
                int found = -1;
                for (int k = 0; k < ncomp; k++) if (comp[k].id == cid) found = k;
                if (found < 0) { rc = WUBU_JPEG_NOT_JPEG; break; }
                comp[found].td = t >> 4;
                comp[found].ta = t & 15;
            }
            if (rc) break;
            const uint8_t *tail = seg + 1 + ns * 2;
            if (tail[0] != 0 || tail[1] != 63 || tail[2] != 0) { rc = WUBU_JPEG_UNSUPPORTED; break; }

            if (!width || !height) { rc = WUBU_JPEG_SIZE; break; }
            if ((size_t)width * (size_t)height > max_pixels) { rc = WUBU_JPEG_SIZE; break; }
            int mx = (width + 8 * hmax - 1) / (8 * hmax);
            int my = (height + 8 * vmax - 1) / (8 * vmax);
            for (int i = 0; i < ncomp; i++) {
                if (!qdef[comp[i].tq]) { rc = WUBU_JPEG_BAD_TABLE; break; }
                if (!ht[0][comp[i].td].valid || !ht[1][comp[i].ta].valid) { rc = WUBU_JPEG_BAD_HUFFMAN; break; }
                comp[i].pw = mx * 8 * comp[i].hs;
                comp[i].ph = my * 8 * comp[i].vs;
                comp[i].plane = calloc((size_t)comp[i].pw * (size_t)comp[i].ph, 1);
                if (!comp[i].plane) { rc = WUBU_JPEG_NOMEM; break; }
                comp[i].dc_pred = 0;
            }
            if (rc) break;

            b.p = tail + 3;                                     /* entropy-coded data starts here */
            b.buf = 0; b.cnt = 0; b.marker = 0;
            int mcu = 0, since_restart = 0;
            for (int yy = 0; yy < my && !rc; yy++) {
                for (int xx = 0; xx < mx && !rc; xx++, mcu++) {
                    if (restart_interval && since_restart == restart_interval) {
                        /* align, then consume the RSTn marker and reset the predictors */
                        b.cnt = 0; b.buf = 0;
                        while (b.p < b.end) {
                            if (b.p[0] == 0xFF && b.p + 1 < b.end && b.p[1] >= 0xD0 && b.p[1] <= 0xD7) { b.p += 2; break; }
                            b.p++;
                        }
                        for (int i = 0; i < ncomp; i++) comp[i].dc_pred = 0;
                        since_restart = 0;
                    }
                    for (int ci = 0; ci < ncomp && !rc; ci++) {
                        for (int by = 0; by < comp[ci].vs && !rc; by++) {
                            for (int bx = 0; bx < comp[ci].hs && !rc; bx++) {
                                float coef[ZLEN]; memset(coef, 0, sizeof coef);
                                int t = huff_decode(&b, &ht[0][comp[ci].td]);
                                if (t < 0) { rc = WUBU_JPEG_BAD_CODE; break; }
                                int diff = receive_extend(&b, t);
                                comp[ci].dc_pred += diff;
                                coef[0] = (float)(comp[ci].dc_pred * qtab[comp[ci].tq][0]);
                                for (int k = 1; k < ZLEN; ) {
                                    int rs = huff_decode(&b, &ht[1][comp[ci].ta]);
                                    if (rs < 0) { rc = WUBU_JPEG_BAD_CODE; break; }
                                    int r = rs >> 4, s = rs & 15;
                                    if (s == 0) {
                                        if (r == 15) { k += 16; continue; }   /* ZRL */
                                        break;                                /* EOB */
                                    }
                                    k += r;
                                    if (k > 63) { rc = WUBU_JPEG_BAD_CODE; break; }
                                    coef[zz[k]] = (float)(receive_extend(&b, s) * qtab[comp[ci].tq][zz[k]]);
                                    k++;
                                }
                                if (rc) break;
                                uint8_t px[ZLEN];
                                idct_block(coef, px);
                                int ox = (xx * comp[ci].hs + bx) * 8;
                                int oy = (yy * comp[ci].vs + by) * 8;
                                for (int r = 0; r < 8; r++)
                                    memcpy(comp[ci].plane + (size_t)(oy + r) * comp[ci].pw + ox, px + r * 8, 8);
                            }
                        }
                    }
                    since_restart++;
                }
            }
            (void)mcu;
            if (b.eof) rc = WUBU_JPEG_SHORT;     /* zeros were padding, not data */
            break;
        }
        default:
            break;                                              /* APPn, COM, others: skip */
        }
        if (rc) break;
        if (m == 0xDA) break;                                   /* one scan is the whole image */
        b.p = seg_end;
    }

    if (!rc && !have_sof) rc = WUBU_JPEG_NOT_JPEG;

    if (!rc) {
        size_t npix = (size_t)width * (size_t)height;
        out->rgb = malloc(npix * 3);
        if (!out->rgb) rc = WUBU_JPEG_NOMEM;
        else {
            out->w = width; out->h = height;
            for (int y = 0; y < height; y++) {
                for (int x = 0; x < width; x++) {
                    uint8_t *px = out->rgb + ((size_t)y * width + x) * 3;
                    /* map the output pixel into each component's own sample grid */
                    float u0 = ((float)x + 0.5f) * (float)comp[0].hs / (float)hmax - 0.5f;
                    float v0 = ((float)y + 0.5f) * (float)comp[0].vs / (float)vmax - 0.5f;
                    float u1 = ((float)x + 0.5f) * (float)comp[1].hs / (float)hmax - 0.5f;
                    float v1 = ((float)y + 0.5f) * (float)comp[1].vs / (float)vmax - 0.5f;
                    float u2 = ((float)x + 0.5f) * (float)comp[2].hs / (float)hmax - 0.5f;
                    float v2 = ((float)y + 0.5f) * (float)comp[2].vs / (float)vmax - 0.5f;
                    if (ncomp == 1) {
                        float v = sample_bilinear(&comp[0], u0, v0);
                        uint8_t g = (uint8_t)(v < 0 ? 0 : (v > 255 ? 255 : lroundf(v)));
                        px[0] = px[1] = px[2] = g;
                    } else {
                        float Y  = (comp[0].hs == hmax && comp[0].vs == vmax)
                                 ? (float)comp[0].plane[(size_t)((int)floorf(v0)) * comp[0].pw + (int)floorf(u0)]
                                 : sample_bilinear(&comp[0], u0, v0);
                        float Cb = sample_bilinear(&comp[1], u1, v1) - 128.0f;
                        float Cr = sample_bilinear(&comp[2], u2, v2) - 128.0f;
                        int r = (int)lroundf(Y + 1.402f * Cr);
                        int g = (int)lroundf(Y - 0.344136f * Cb - 0.714136f * Cr);
                        int bl = (int)lroundf(Y + 1.772f * Cb);
                        px[0] = (uint8_t)(r < 0 ? 0 : (r > 255 ? 255 : r));
                        px[1] = (uint8_t)(g < 0 ? 0 : (g > 255 ? 255 : g));
                        px[2] = (uint8_t)(bl < 0 ? 0 : (bl > 255 ? 255 : bl));
                    }
                }
            }
        }
    }

    for (int i = 0; i < ncomp; i++) free(comp[i].plane);
    if (rc) { free(out->rgb); out->rgb = NULL; out->w = out->h = 0; if (err) *err = rc; return rc; }
    return WUBU_JPEG_OK;
}
