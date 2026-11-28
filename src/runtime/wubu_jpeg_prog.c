/* wubu_jpeg_prog.c -- progressive (SOF2) scans.
 *
 * Progressive JPEG is not baseline with extra markers: the SAME 64 coefficients are split
 * across up to ~10 scans along two axes (spectral selection Ss..Se, successive approximation
 * Ah/Al). A block is not finished until the last scan touching it is read, so every block's
 * coefficients must be kept for the whole decode -- which is why this cannot reuse the
 * block-at-a-time path in wubu_jpeg.c, and why it stores quantised coefficients per block and
 * only dequantises + transforms at the end.
 *
 * The four entropy modes are transcribed from libjpeg's jdphuff.c (decode_mcu_DC_first /
 * DC_refine / AC_first / AC_refine), including two details that are easy to get wrong:
 *   - EOBRUN is decoder STATE, not bitstream: it spans MCUs and resets at restart markers.
 *   - In a refinement scan a correction bit is appended to every already-nonzero coefficient,
 *     and a newly-nonzero coefficient flips sign on a 0 bit -- the value is +-(1 << Al).
 */
#include "wubu_jpeg.h"
#include "wubu_jpeg_internal.h"
#include <string.h>

/* One scan's parameters plus its component membership, in scan order. */
typedef struct {
    int ss, se, ah, al;
    int ns;
    int ci[MAX_COMPS];
} Scan;

static int16_t *blk(Comp *c, int bx, int by) { return c->coef + ((size_t)by * c->stride + bx) * ZLEN; }

/* DC first scan: interleaved over MCUs, one DC per block, predictor per component. */
static int dc_first(Bits *b, Huff ht[2][4], Comp *comp, const Scan *sc, int restart_interval,
                    int *since_restart, int mx, int my) {
    for (int yy = 0; yy < my; yy++)
        for (int xx = 0; xx < mx; xx++) {
            if (restart_interval && *since_restart == restart_interval) {
                b->cnt = 0; b->buf = 0;
                while (b->p < b->end) {
                    if (b->p[0] == 0xFF && b->p + 1 < b->end && b->p[1] >= 0xD0 && b->p[1] <= 0xD7) { b->p += 2; break; }
                    b->p++;
                }
                for (int i = 0; i < MAX_COMPS; i++) comp[i].dc_pred = 0;
                *since_restart = 0;
            }
            for (int si = 0; si < sc->ns; si++) {
                Comp *c = &comp[sc->ci[si]];
                for (int by = 0; by < c->vs; by++)
                    for (int bx = 0; bx < c->hs; bx++) {
                        int t = jpeg_huff_decode(b, &ht[0][c->td]);
                        if (t < 0) return WUBU_JPEG_BAD_CODE;
                        int diff = jpeg_receive_extend(b, t);
                        c->dc_pred += diff;
                        int16_t *o = blk(c, xx * c->hs + bx, yy * c->vs + by);
                        o[0] = (int16_t)(c->dc_pred << sc->al);
                    }
            }
            (*since_restart)++;
        }
    return WUBU_JPEG_OK;
}

/* DC refinement: exactly one bit per block, ORed into the DC coefficient. */
static int dc_refine(Bits *b, Comp *comp, const Scan *sc, int restart_interval,
                     int *since_restart, int mx, int my) {
    int p1 = 1 << sc->al;
    for (int yy = 0; yy < my; yy++)
        for (int xx = 0; xx < mx; xx++) {
            if (restart_interval && *since_restart == restart_interval) {
                b->cnt = 0; b->buf = 0;
                while (b->p < b->end) {
                    if (b->p[0] == 0xFF && b->p + 1 < b->end && b->p[1] >= 0xD0 && b->p[1] <= 0xD7) { b->p += 2; break; }
                    b->p++;
                }
                *since_restart = 0;
            }
            for (int si = 0; si < sc->ns; si++) {
                Comp *c = &comp[sc->ci[si]];
                for (int by = 0; by < c->vs; by++)
                    for (int bx = 0; bx < c->hs; bx++) {
                        int16_t *o = blk(c, xx * c->hs + bx, yy * c->vs + by);
                        if (jpeg_get_bits(b, 1)) o[0] |= (int16_t)p1;
                    }
            }
            (*since_restart)++;
        }
    return WUBU_JPEG_OK;
}

/* Walk the blocks of a single-component (AC) scan in the component's own raster order. */
#define AC_WALK(BODY)                                                                     \
    for (int by = 0; by < c->bh; by++)                                                    \
        for (int bx = 0; bx < c->bw; bx++) {                                              \
            if (restart_interval && *since_restart == restart_interval) {                 \
                b->cnt = 0; b->buf = 0;                                                   \
                while (b->p < b->end) {                                                   \
                    if (b->p[0] == 0xFF && b->p + 1 < b->end &&                        \
                        b->p[1] >= 0xD0 && b->p[1] <= 0xD7) { b->p += 2; break; }          \
                    b->p++;                                                               \
                }                                                                         \
                eobrun = 0;                                                               \
                (*since_restart) = 0;                                                     \
            }                                                                             \
            int16_t *o = blk(c, bx, by);                                                  \
            BODY                                                                          \
            (*since_restart)++;                                                           \
        }

static int ac_first(Bits *b, Huff ht[2][4], Comp *comp, const Scan *sc, int restart_interval,
                    int *since_restart, int *eobrun_in) {
    Comp *c = &comp[sc->ci[0]];
    int eobrun = *eobrun_in;
    int rc = WUBU_JPEG_OK;
    AC_WALK({
        if (eobrun > 0) { eobrun--; }
        else {
            for (int k = sc->ss; k <= sc->se; k++) {
                int s = jpeg_huff_decode(b, &ht[1][c->ta]);
                if (s < 0) {
                    rc = WUBU_JPEG_BAD_CODE;
                    break;
                }
                int r = s >> 4;
                s &= 15;
                if (s) {
                    k += r;
                    if (k > sc->se) { rc = WUBU_JPEG_BAD_CODE; break; }
                    int v = jpeg_receive_extend(b, s);
                    o[zz[k]] = (int16_t)(v << sc->al);
                } else if (r == 15) {
                    k += 15;                       /* ZRL: sixteen zeroes */
                } else {
                    eobrun = 1 << r;
                    if (r) eobrun += jpeg_get_bits(b, r);
                    eobrun--;
                    break;
                }
            }
        }
    });
    *eobrun_in = eobrun;
    return rc;
}

static int ac_refine(Bits *b, Huff ht[2][4], Comp *comp, const Scan *sc, int restart_interval,
                     int *since_restart, int *eobrun_in) {
    Comp *c = &comp[sc->ci[0]];
    int eobrun = *eobrun_in;
    int p1 = 1 << sc->al, m1 = -(1 << sc->al);
    int rc = WUBU_JPEG_OK;
    AC_WALK({
        int k = sc->ss;
        if (eobrun == 0) {
            for (; k <= sc->se; k++) {
                int s = jpeg_huff_decode(b, &ht[1][c->ta]);
                if (s < 0) { rc = WUBU_JPEG_BAD_CODE; break; }
                int r = s >> 4;
                s &= 15;
                if (s) {
                    if (s != 1) { rc = WUBU_JPEG_BAD_CODE; break; }   /* new coefs are size 1 */
                    s = jpeg_get_bits(b, 1) ? p1 : m1;               /* sign of the new value */
                } else if (r != 15) {
                    eobrun = 1 << r;
                    if (r) eobrun += jpeg_get_bits(b, r);
                    break;                                            /* rest of block is EOB */
                }
                /* Advance over nonzero coefficients (appending correction bits) and over r
                 * still-zero ones, then emit the new coefficient at k. */
                do {
                    int16_t *t = &o[zz[k]];
                    if (*t != 0) {
                        if (jpeg_get_bits(b, 1) && (*t & p1) == 0)
                            *t = (int16_t)(*t >= 0 ? *t + p1 : *t + m1);
                    } else if (--r < 0) {
                        break;
                    }
                    k++;
                } while (k <= sc->se);
                if (s) {
                    if (k > sc->se) { rc = WUBU_JPEG_BAD_CODE; break; }
                    o[zz[k]] = (int16_t)s;
                }
            }
        }
        if (eobrun > 0) {
            for (; k <= sc->se; k++) {
                int16_t *t = &o[zz[k]];
                if (*t != 0 && jpeg_get_bits(b, 1) && (*t & p1) == 0)
                    *t = (int16_t)(*t >= 0 ? *t + p1 : *t + m1);
            }
            eobrun--;
        }
    });
    *eobrun_in = eobrun;
    return rc;
}

int wubu_jpeg_prog_scan(Bits *b, Huff ht[2][4], Comp *comp, int mx, int my, int restart_interval,
                        int ss, int se, int ah, int al, int ns, const int *sci, int *eobrun) {
    int since_restart = 0;                /* per scan: restart intervals are scan-local */
    Scan sc;
    sc.ss = ss; sc.se = se; sc.ah = ah; sc.al = al; sc.ns = ns;
    for (int i = 0; i < ns; i++) sc.ci[i] = sci[i];
    if (al < 0 || al > 13) return WUBU_JPEG_UNSUPPORTED;
    if (ss == 0) {
        if (se != 0) return WUBU_JPEG_UNSUPPORTED;
        return ah == 0 ? dc_first(b, ht, comp, &sc, restart_interval, &since_restart, mx, my)
                       : dc_refine(b, comp, &sc, restart_interval, &since_restart, mx, my);
    }
    if (ss > se || se >= ZLEN || ns != 1) return WUBU_JPEG_UNSUPPORTED;
    return ah == 0 ? ac_first(b, ht, comp, &sc, restart_interval, &since_restart, eobrun)
                   : ac_refine(b, ht, comp, &sc, restart_interval, &since_restart, eobrun);
}

/* After the last scan: dequantise each block in natural order and transform it into the plane. */
void wubu_jpeg_prog_finish(Comp *comp, int ncomp, const uint16_t qtab[4][ZLEN]) {
    for (int ci = 0; ci < ncomp; ci++) {
        Comp *c = &comp[ci];
        if (!c->coef) continue;
        for (int by = 0; by < c->bh; by++)
            for (int bx = 0; bx < c->bw; bx++) {
                const int16_t *src = blk(c, bx, by);
                float coef[ZLEN];
                for (int i = 0; i < ZLEN; i++) coef[i] = (float)src[i] * (float)qtab[c->tq][i];
                uint8_t px[ZLEN];
                jpeg_idct_block(coef, px);
                for (int r = 0; r < 8; r++)
                    memcpy(c->plane + (size_t)(by * 8 + r) * c->pw + bx * 8, px + r * 8, 8);
            }
    }
}
