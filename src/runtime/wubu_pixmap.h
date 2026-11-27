/* wubu_pixmap.h -- decode an image file into pixels we own.
 *
 * Own implementation (docs/GOAL-SLERM.json, SLERM-2002). Naming note: `wubu_image`
 * already means a CONTAINER image (layers, manifests, pulls) in this tree, so a
 * decoded raster is a pixmap here. Two different concepts must never share a name.
 *
 * Stage 1 covers the formats a wallpaper, an icon and a screenshot actually arrive
 * in: PNG (8-bit, colour types 0/2/3/6, filters 0-4, non-interlaced) and BMP
 * (24/32-bit BI_RGB). JPEG, GIF and TGA are refused by name rather than half-done:
 * a decoder that returns garbage for a format it claims is worse than one that says
 * "unsupported".
 *
 * No allocation: the caller owns the pixel buffer and the IDAT scratch area.
 */
#ifndef WUBU_PIXMAP_H
#define WUBU_PIXMAP_H

#include <stddef.h>

enum {
    WUBU_PIXMAP_OK          =  0,
    WUBU_PIXMAP_ARGS        = -1,
    WUBU_PIXMAP_FORMAT      = -2,  /* not a format we decode */
    WUBU_PIXMAP_TRUNCATED   = -3,  /* file ends before its own structures do */
    WUBU_PIXMAP_UNSUPPORTED = -4,  /* a real format, in a shape we do not do yet */
    WUBU_PIXMAP_CORRUPT     = -5,  /* bad CRC, bad header field, bad filter */
    WUBU_PIXMAP_SCRATCH     = -6,  /* caller scratch buffer too small for the IDATs */
    WUBU_PIXMAP_OVERFLOW    = -7   /* pixel buffer too small for width*height*channels */
};

typedef struct {
    int width;
    int height;
    int channels;            /* 1 grey, 3 RGB, 4 RGBA */
} WubuPixmap;

/* Metadata only; cheap, no decoding. Returns 0 or -1 with *err set. */
int wubu_pixmap_info(const void *data, size_t len, WubuPixmap *px, int *err);

/* Decode into `out` (cap >= width*height*channels, plus one filter byte per row for
 * PNG). `scratch` holds the concatenated PNG IDAT payloads; unused for BMP. */
int wubu_pixmap_decode(const void *data, size_t len, void *scratch, size_t scratch_cap,
                       unsigned char *out, size_t outcap, WubuPixmap *px, int *err);

const char *wubu_pixmap_reason(int err);

#endif
