/* wubu_gif.h -- GIF87a/89a decoding (single frame, with transparency).
 *
 * Own implementation (docs/GOAL-SLERM.json, SLERM-2021 stage A). GIF is what an
 * emoji, a screenshot tool, a sticker and half the web's animations arrive as; the
 * file manager currently falls back to a generic icon for all of them.
 *
 * Scope is stated rather than implied: the FIRST frame is decoded, in full, with the
 * palette, interlacing and transparency the format defines. Animation timing and
 * frame compositing are not implemented and are not claimed.
 *
 * Output is always RGBA, because transparency is part of the format's meaning and a
 * decoder that drops it invents pixels the file does not contain.
 */
#ifndef WUBU_GIF_H
#define WUBU_GIF_H

#include <stddef.h>
#include <stdint.h>

enum {
    WUBU_GIF_OK        =  0,
    WUBU_GIF_ARGS      = -1,
    WUBU_GIF_FORMAT    = -2,   /* not a GIF */
    WUBU_GIF_TRUNCATED = -3,   /* data ends inside a block */
    WUBU_GIF_CORRUPT   = -4,   /* bad code, bad palette index, bad sub-block chain */
    WUBU_GIF_OVERFLOW  = -5    /* output buffer smaller than width*height*4 */
};

typedef struct {
    int width;
    int height;
    int channels;        /* always 4 (RGBA) */
    int interlaced;
    int transparent;     /* 1 when a transparent index was declared */
} WubuGif;

int wubu_gif_info(const void *data, size_t len, WubuGif *info, int *err);

/* Decode the first frame into RGBA. Pixels outside the frame's own rectangle are
 * fully transparent. */
int wubu_gif_decode(const void *data, size_t len, uint8_t *out, size_t outcap,
                    WubuGif *info, int *err);

const char *wubu_gif_reason(int err);

#endif
