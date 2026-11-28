/* wubu_jpeg_core.c -- bit reader, Huffman (Annex F), IDCT, chroma sampling, marker reading.
 * Shared by the baseline driver (wubu_jpeg.c) and the progressive decoder (wubu_jpeg_prog.c). */
#include "wubu_jpeg_internal.h"
#include <math.h>
#include <string.h>

const int zz[ZLEN] = {
     0, 1, 8,16, 9, 2, 3,10,17,24,32,25,18,11, 4, 5,
    12,19,26,33,40,48,41,34,27,20,13, 6, 7,14,21,28,
    35,42,49,56,57,50,43,36,29,22,15,23,30,37,44,51,
    58,59,52,45,38,31,39,46,53,60,61,54,47,55,62,63
};







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

void jpeg_bits_fill(Bits *b) {
    while (b->cnt <= 24) {
        int c = raw_byte(b);
        if (c < 0) { b->buf <<= 8; b->cnt += 8; continue; }
        b->buf = (b->buf << 8) | (uint32_t)c;
        b->cnt += 8;
    }
}

int jpeg_get_bits(Bits *b, int n) {
    if (n == 0) return 0;
    jpeg_bits_fill(b);
    int v = (int)((b->buf >> (b->cnt - n)) & ((1u << n) - 1));
    b->cnt -= n;
    return v;
}


/* ---------------------------------------------------------------- Huffman */

int jpeg_huff_build(Huff *h) {
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

int jpeg_huff_decode(Bits *b, const Huff *h) {
    int code = 0;
    for (int l = 1; l <= 16; l++) {
        code = (code << 1) | jpeg_get_bits(b, 1);
        if (h->bits[l] && code <= h->maxcode[l])
            return h->val[h->valptr[l] + code - h->mincode[l]];
    }
    return -1;
}

int jpeg_receive_extend(Bits *b, int n) {
    if (n == 0) return 0;
    int v = jpeg_get_bits(b, n);
    int vt = 1 << (n - 1);
    if (v < vt) v -= (1 << n) - 1;
    return v;
}


/* ---------------------------------------------------------------- IDCT */

static int   idct_ready;
static float idct_cos[8][8];

void jpeg_idct_init(void) {
    if (idct_ready) return;
    for (int x = 0; x < 8; x++)
        for (int u = 0; u < 8; u++)
            idct_cos[x][u] = (float)cos((2 * x + 1) * u * 3.14159265358979323846 / 16.0);
    idct_ready = 1;
}

/* Separable IDCT on a dequantised block of coefficients already in natural order. */
void jpeg_idct_block(const float in[ZLEN], uint8_t out[ZLEN]) {
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
float jpeg_sample_bilinear(const Comp *c, float u, float v) {
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


int jpeg_read_marker(Bits *b) {
    /* Skip jpeg_bits_fill bytes until 0xFF then the marker code. */
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

