/* wubu_jpeg.h -- JPEG to RGB, ours: baseline sequential AND progressive (SOF2).
 *
 * Own implementation (docs/GOAL-SLERM.json, SLERM-2024 stage A). The file manager can
 * preview PNG, BMP and GIF because those decoders exist; a photograph is the one common
 * case left, and it is the one people actually have in their Pictures folder.
 *
 * Scope is stated rather than implied: 8-bit Huffman-coded JPEG, one or three components,
 * restart intervals, any legal chroma subsampling, in BOTH sequential (SOF0) and progressive
 * (SOF2) form. Progressive is not a variant we tolerate -- it is the form most photographs on
 * the web are saved in, so refusing it meant the preview pane could not open the pictures
 * people actually have. Arithmetic-coded, lossless, 12-bit and CMYK files are refused BY
 * NAME: a decoder that returns a plausible grey rectangle is worse than one that says which
 * variant it is and why it will not open.
 */
#ifndef WUBU_JPEG_H
#define WUBU_JPEG_H

#include <stddef.h>
#include <stdint.h>

enum {
    WUBU_JPEG_OK          =  0,
    WUBU_JPEG_SHORT       = -1,   /* the buffer ends mid-structure */
    WUBU_JPEG_NOT_JPEG    = -2,   /* no SOI, or a marker stream that is not JPEG */
    WUBU_JPEG_UNSUPPORTED = -3,   /* arithmetic, lossless, 12-bit, 4-component */
    WUBU_JPEG_BAD_HUFFMAN = -4,   /* an incomplete or over-subscribed Huffman table */
    WUBU_JPEG_BAD_CODE    = -5,   /* a bit sequence no Huffman code covers */
    WUBU_JPEG_BAD_TABLE   = -6,   /* a quantisation table index with no table */
    WUBU_JPEG_ARGS        = -7,
    WUBU_JPEG_NOMEM       = -8,
    WUBU_JPEG_SIZE        = -9    /* zero, or larger than the caller's limit */
};

typedef struct {
    uint8_t *rgb;      /* w*h*3, caller frees with wubu_jpeg_free */
    int      w, h;
} WubuJpegImage;

/* `max_pixels` caps the allocation (0 = 64 megapixels). */
int wubu_jpeg_decode(const uint8_t *buf, size_t len, WubuJpegImage *out, size_t max_pixels, int *err);
void wubu_jpeg_free(WubuJpegImage *img);
const char *wubu_jpeg_reason(int err);

/* Marker names for diagnostics (a refused file says WHICH variant it was). */
const char *wubu_jpeg_sof_name(int sof_marker);

#endif
