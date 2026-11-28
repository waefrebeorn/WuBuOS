/* wubu_jpeg_internal.h -- what the baseline driver, the progressive scan decoder and the
 * shared core agree on. NOT a public header: nothing outside src/runtime/wubu_jpeg* includes it.
 *
 * The split exists because progressive JPEG cannot reuse a block-at-a-time decoder: its
 * coefficients are scattered across up to ~10 scans, so every block must live until the last
 * scan. Rather than write a second parser, the pieces both need (bit reader, Huffman, IDCT,
 * colour) live here and in wubu_jpeg_core.c, and are used by both paths.
 */
#ifndef WUBU_JPEG_INTERNAL_H
#define WUBU_JPEG_INTERNAL_H

#include <stdint.h>
#include <stddef.h>

#define ZLEN 64
#define MAX_COMPS 4

typedef struct {
    int      bits[17];
    int      mincode[17], maxcode[17], valptr[17];
    uint8_t  val[256];
    int      nval;
    int      valid;
} Huff;

typedef struct {
    const uint8_t *p, *end;
    uint32_t buf;
    int      cnt;
    int      marker;
    int      eof;
} Bits;

typedef struct {
    int  id, hs, vs, tq, td, ta;
    int  dc_pred;
    int  pw, ph;            /* plane size in samples, padded to whole blocks */
    uint8_t *plane;
    /* Progressive only: every block's coefficients, natural order, kept until the last scan.
     * NULL for baseline, which dequantises and transforms block by block. */
    int16_t *coef;
    int      stride;        /* blocks across in the PADDED MCU grid (storage stride) */
    int      bw, bh;        /* blocks across/down actually ENCODED: ceil(comp_size/8).
                             * A non-interleaved scan carries exactly this many per row, which is
                             * fewer than stride when the component width is not a multiple of the
                             * MCU. Walking stride instead desynchronises the entropy stream. */
} Comp;

extern const int zz[ZLEN];

void jpeg_bits_fill(Bits *b);                /* buffer at least 25 bits */
int  jpeg_get_bits(Bits *b, int n);
int  jpeg_huff_build(Huff *h);
int  jpeg_huff_decode(Bits *b, const Huff *h);
int  jpeg_receive_extend(Bits *b, int n);
void jpeg_idct_init(void);
void jpeg_idct_block(const float in[ZLEN], uint8_t out[ZLEN]);
float jpeg_sample_bilinear(const Comp *c, float u, float v);
int  jpeg_read_marker(Bits *b);

/* Progressive scans (wubu_jpeg_prog.c). One call per SOS; `eobrun` carries EOB state between
 * the AC blocks of a scan and must be reset at every restart marker. */
int  wubu_jpeg_prog_scan(Bits *b, Huff ht[2][4], Comp *comp, int mx, int my, int restart_interval,
                         int ss, int se, int ah, int al, int ns, const int *sci, int *eobrun);
void wubu_jpeg_prog_finish(Comp *comp, int ncomp, const uint16_t qtab[4][ZLEN]);

#endif
