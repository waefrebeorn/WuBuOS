/* wubu_jpeg.c -- JPEG driver: markers, baseline scans, dispatch to the progressive path.
 *
 * Structure follows ITU T.81 rather than a compressed-blog version: markers, then
 * canonical Huffman decoding (Annex F), then dequantise + dezigzag + separable IDCT per
 * 8x8 block, then nearest-neighbour chroma upsampling. Restart intervals are honoured
 * because a truncated or damaged photo usually fails there first.
 */
#include "wubu_jpeg.h"
#include "wubu_jpeg_internal.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ZLEN 64
#define MAX_COMPS 4


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
        case WUBU_JPEG_UNSUPPORTED: return "this JPEG variant is not supported (arithmetic, lossless, 12-bit or CMYK)";
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

int wubu_jpeg_decode(const uint8_t *buf, size_t len, WubuJpegImage *out, size_t max_pixels, int *err) {
    if (err) *err = WUBU_JPEG_OK;
    if (!buf || !out) { if (err) *err = WUBU_JPEG_ARGS; return WUBU_JPEG_ARGS; }
    out->rgb = NULL; out->w = out->h = 0;
    if (len < 4 || buf[0] != 0xFF || buf[1] != 0xD8) { if (err) *err = WUBU_JPEG_NOT_JPEG; return WUBU_JPEG_NOT_JPEG; }
    if (!max_pixels) max_pixels = 64u * 1000u * 1000u;

    jpeg_idct_init();
    Bits b; memset(&b, 0, sizeof b);
    b.p = buf + 2; b.end = buf + len;

    uint16_t qtab[4][ZLEN]; memset(qtab, 0, sizeof qtab);
    int qdef[4] = {0,0,0,0};
    Huff ht[2][4]; memset(ht, 0, sizeof ht);

    Comp comp[MAX_COMPS]; memset(comp, 0, sizeof comp);
    int ncomp = 0, width = 0, height = 0, hmax = 1, vmax = 1;
    int restart_interval = 0, have_sof = 0, progressive = 0, planes_ready = 0;
    int rc = WUBU_JPEG_OK;

    for (;;) {
        int m = jpeg_read_marker(&b);
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
                if (!jpeg_huff_build(t)) { rc = WUBU_JPEG_BAD_HUFFMAN; break; }
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
            /* SOF2 is progressive: the same coefficients split across scans. Everything else here
             * (arithmetic, lossless, differential, 12-bit, CMYK) stays refused by design. */
            if (m != 0xC0 && m != 0xC2) { rc = WUBU_JPEG_UNSUPPORTED; break; }
            progressive = (m == 0xC2);
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
        case 0xDA: {                                             /* SOS: decode one scan */
            if (!have_sof) { rc = WUBU_JPEG_NOT_JPEG; break; }
            if (slen < 1) { rc = WUBU_JPEG_SHORT; break; }
            int ns = seg[0];
            if (ns < 1 || ns > ncomp || slen < (size_t)(1 + ns * 2 + 3)) { rc = WUBU_JPEG_NOT_JPEG; break; }
            int sci[MAX_COMPS];
            for (int i = 0; i < ns; i++) {
                int cid = seg[1 + i*2], t = seg[2 + i*2];
                int found = -1;
                for (int k = 0; k < ncomp; k++) if (comp[k].id == cid) found = k;
                if (found < 0) { rc = WUBU_JPEG_NOT_JPEG; break; }
                comp[found].td = t >> 4;
                comp[found].ta = t & 15;
                sci[i] = found;
            }
            if (rc) break;
            const uint8_t *tail = seg + 1 + ns * 2;
            int ss = tail[0], se = tail[1], ah = tail[2] >> 4, al = tail[2] & 15;
            if (!progressive && (ss != 0 || se != 63 || ah != 0 || al != 0)) { rc = WUBU_JPEG_UNSUPPORTED; break; }

            if (!width || !height) { rc = WUBU_JPEG_SIZE; break; }
            if ((size_t)width * (size_t)height > max_pixels) { rc = WUBU_JPEG_SIZE; break; }
            int mx = (width + 8 * hmax - 1) / (8 * hmax);
            int my = (height + 8 * vmax - 1) / (8 * vmax);

            if (!planes_ready) {                    /* progressive has many scans: allocate ONCE */
                for (int i = 0; i < ncomp && !rc; i++) {
                    if (!qdef[comp[i].tq]) { rc = WUBU_JPEG_BAD_TABLE; break; }
                    comp[i].pw = mx * 8 * comp[i].hs;
                    comp[i].ph = my * 8 * comp[i].vs;
                    comp[i].plane = calloc((size_t)comp[i].pw * (size_t)comp[i].ph, 1);
                    if (!comp[i].plane) { rc = WUBU_JPEG_NOMEM; break; }
                    comp[i].dc_pred = 0;
                    if (progressive) {
                        /* Every block's coefficients for the whole frame: the price of SOF2. */
                        comp[i].stride = mx * comp[i].hs;
                        /* What a non-interleaved (AC) scan actually encodes. */
                        int cw = (width * comp[i].hs + hmax - 1) / hmax;
                        int ch = (height * comp[i].vs + vmax - 1) / vmax;
                        comp[i].bw = (cw + 7) / 8;
                        comp[i].bh = (ch + 7) / 8;
                        comp[i].coef = calloc((size_t)comp[i].stride * (size_t)(my * comp[i].vs) * ZLEN,
                                              sizeof(int16_t));
                        if (!comp[i].coef) { rc = WUBU_JPEG_NOMEM; break; }
                    }
                }
                planes_ready = 1;
            }
            if (rc) break;
            for (int i = 0; i < ns && !rc; i++) {
                int k = sci[i];
                if (progressive) {
                    /* A DC first scan needs the DC table; an AC scan needs the AC table of its
                     * single component. DC refinement needs no table at all. */
                    if (ss == 0 && ah == 0 && !ht[0][comp[k].td].valid) rc = WUBU_JPEG_BAD_HUFFMAN;
                    if (ss > 0 && !ht[1][comp[k].ta].valid) rc = WUBU_JPEG_BAD_HUFFMAN;
                } else {
                    if (!ht[0][comp[k].td].valid || !ht[1][comp[k].ta].valid) rc = WUBU_JPEG_BAD_HUFFMAN;
                }
            }
            if (rc) break;

            b.p = tail + 3;                                     /* entropy-coded data starts here */
            b.buf = 0; b.cnt = 0; b.marker = 0;

            if (progressive) {
                int eobrun = 0;
                rc = wubu_jpeg_prog_scan(&b, ht, comp, mx, my, restart_interval,
                                         ss, se, ah, al, ns, sci, &eobrun);
                break;                                          /* the driver keeps reading markers */
            }

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
                                int t = jpeg_huff_decode(&b, &ht[0][comp[ci].td]);
                                if (t < 0) { rc = WUBU_JPEG_BAD_CODE; break; }
                                int diff = jpeg_receive_extend(&b, t);
                                comp[ci].dc_pred += diff;
                                coef[0] = (float)(comp[ci].dc_pred * qtab[comp[ci].tq][0]);
                                for (int k = 1; k < ZLEN; ) {
                                    int rs = jpeg_huff_decode(&b, &ht[1][comp[ci].ta]);
                                    if (rs < 0) { rc = WUBU_JPEG_BAD_CODE; break; }
                                    int r = rs >> 4, s = rs & 15;
                                    if (s == 0) {
                                        if (r == 15) { k += 16; continue; }   /* ZRL */
                                        break;                                /* EOB */
                                    }
                                    k += r;
                                    if (k > 63) { rc = WUBU_JPEG_BAD_CODE; break; }
                                    coef[zz[k]] = (float)(jpeg_receive_extend(&b, s) * qtab[comp[ci].tq][zz[k]]);
                                    k++;
                                }
                                if (rc) break;
                                uint8_t px[ZLEN];
                                jpeg_idct_block(coef, px);
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
        if (m == 0xDA && !progressive) break;                    /* baseline: one scan is the image */
        /* After a progressive scan the cursor is already past the entropy data. Rewinding it to the
         * end of the SOS header would make the next marker read land inside entropy bytes. */
        if (m != 0xDA) b.p = seg_end;
    }

    if (!rc && !have_sof) rc = WUBU_JPEG_NOT_JPEG;
    if (!rc && progressive) wubu_jpeg_prog_finish(comp, ncomp, qtab);   /* dequantise + IDCT */

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
                        float v = jpeg_sample_bilinear(&comp[0], u0, v0);
                        uint8_t g = (uint8_t)(v < 0 ? 0 : (v > 255 ? 255 : lroundf(v)));
                        px[0] = px[1] = px[2] = g;
                    } else {
                        float Y  = (comp[0].hs == hmax && comp[0].vs == vmax)
                                 ? (float)comp[0].plane[(size_t)((int)floorf(v0)) * comp[0].pw + (int)floorf(u0)]
                                 : jpeg_sample_bilinear(&comp[0], u0, v0);
                        float Cb = jpeg_sample_bilinear(&comp[1], u1, v1) - 128.0f;
                        float Cr = jpeg_sample_bilinear(&comp[2], u2, v2) - 128.0f;
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

    for (int i = 0; i < ncomp; i++) { free(comp[i].plane); free(comp[i].coef); }
    if (rc) { free(out->rgb); out->rgb = NULL; out->w = out->h = 0; if (err) *err = rc; return rc; }
    return WUBU_JPEG_OK;
}
