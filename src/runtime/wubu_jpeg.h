/* wubu_jpeg.h -- baseline JPEG to RGB, ours.
 *
 * Own implementation (docs/GOAL-SLERM.json, SLERM-2024 stage A). The file manager can
 * preview PNG, BMP and GIF because those decoders exist; a photograph is the one common
 * case left, and it is the one people actually have in their Pictures folder.
 *
 * Scope is stated rather than implied. This is BASELINE SEQUENTIAL, 8-bit, Huffman-coded,
 * one or three components, with restart intervals and any of the legal chroma subsamplings.
 * Progressive, arithmetic-coded, 12-bit and CMYK JPEGs are refused BY NAME -- a decoder that
 * returns a plausible grey rectangle for a progressive file is worse than one that says it
 * cannot read it.
 */
#ifndef WUBU_JPEG_H
#define WUBU_JPEG_H

#include <stddef.h>
#include <stdint.h>

enum {
    WUBU_JPEG_OK          =  0,
    WUBU_JPEG_SHORT       = -1,   /* the buffer ends mid-structure */
    WUBU_JPEG_NOT_JPEG    = -2,   /* no SOI, or a marker stream that is not JPEG */
    WUBU_JPEG_UNSUPPORTED = -3,   /* progressive, arithmetic, 12-bit, 4-component */
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

/* Marker names for diagnostics and for the gate that refuses progressive files. */
const char *wubu_jpeg_sof_name(int sof_marker);

#endif
