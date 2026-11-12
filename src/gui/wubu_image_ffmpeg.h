/*
 * wubu_image_ffmpeg.h -- image codecs backed by ffmpeg.
 *
 * WuBuOS has no image decoder: the tree hand-rolls a PNG *encoder* for
 * screenshots, but nothing can READ an image back. That gap means a user
 * cannot set a photograph as wallpaper, an agent cannot inspect a sprite
 * sheet, and every asset has to be baked into C at build time.
 *
 * ffmpeg is the right backbone for this rather than a second hand-written
 * decoder: it already carries hundreds of format backends (PNG, JPEG, GIF,
 * WebP, BMP, TIFF, AVIF, ...) and it is a mature, audited codebase. WuBuOS
 * imports that backbone instead of reimplementing it.
 *
 * This module is a thin, honest wrapper: it runs ffmpeg as a subprocess with
 * rawvideo/RGBA on each side. It does not pretend to be a codec and it does
 * not link libav* -- the OS stays small and the codec surface is whatever the
 * host's ffmpeg supports.
 *
 * If ffmpeg is unavailable the callers fall back to the built-in PNG writer
 * (encode only), so a minimal install still works.
 *
 * C11, opaque API, no god headers.
 */

#ifndef WUBU_IMAGE_FFMPEG_H
#define WUBU_IMAGE_FFMPEG_H

#include <stdint.h>

/* Decode any format ffmpeg understands into a freshly malloc'd RGBA buffer.
 * Returns 0 on success; *out_rgba is caller-owned (free it). */
int wubu_image_load_ffmpeg(const char *path,
                           uint32_t **out_rgba, int *out_w, int *out_h);

/* Encode an RGBA buffer to `path`. `fmt` is a ffmpeg format hint
 * ("png", "jpeg", "gif", "webp", "bmp"); NULL lets ffmpeg pick from the
 * extension. Returns 0 on success. */
int wubu_image_save_ffmpeg(const char *path,
                           const uint32_t *rgba, int w, int h,
                           const char *fmt);

/* Is the ffmpeg backbone available on this host? */
int wubu_image_ffmpeg_available(void);

/* The backend's own version string, for diagnostics ("ffmpeg n7.0", ...). */
const char *wubu_image_ffmpeg_version(void);

#endif /* WUBU_IMAGE_FFMPEG_H */
