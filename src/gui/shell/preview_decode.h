/* preview_decode.h -- one function the preview pane needs: a file on disk, pixels out.
 *
 * Own code only (docs/GOAL-SLERM.json): the decode formats are wubu_pixmap (PNG/BMP),
 * wubu_gif (first frame) and wubu_jpeg (baseline). WebP has no decoder here and is refused
 * by name rather than shown as a broken image.
 *
 * Kept out of explorer_preview.c because that file's job is choosing WHAT to preview; this
 * file's job is turning bytes into pixels, and the second one can be gated without a screen.
 */
#ifndef WUBU_PREVIEW_DECODE_H
#define WUBU_PREVIEW_DECODE_H

#include <stddef.h>
#include <stdint.h>

enum {
    WUBU_PREVIEW_OK        =  0,
    WUBU_PREVIEW_OPEN      = -1,   /* cannot read the file */
    WUBU_PREVIEW_FORMAT    = -2,   /* extension has no decoder (webp, tiff, ...) */
    WUBU_PREVIEW_DECODE    = -3,   /* the decoder refused the bytes */
    WUBU_PREVIEW_TOO_BIG   = -4,   /* above the pixel cap */
    WUBU_PREVIEW_NOMEM     = -5
};

/* Decode bytes into `*out_px`, 0x00RRGGBB, top row first. Caller frees with free().
 * `name` only supplies the extension, so this works for a namespace file whose bytes came
 * through styxfs and never existed at a host path. `max_pixels` 0 = 24 megapixels. */
int wubu_preview_decode_bytes(const char *name, const unsigned char *data, size_t len,
                              uint32_t **out_px, int *w, int *h, size_t max_pixels);

/* Convenience for host files (fixtures, gates): reads `path` then calls the bytes form. */
int wubu_preview_decode_file(const char *path, uint32_t **out_px, int *w, int *h, size_t max_pixels);

const char *wubu_preview_reason(int code);

#endif
