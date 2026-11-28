/* preview_decode.c -- see preview_decode.h. */
#include "preview_decode.h"
#include <strings.h>
#include "../../runtime/wubu_pixmap.h"
#include "../../runtime/wubu_gif.h"
#include "../../runtime/wubu_jpeg.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const char *wubu_preview_reason(int code) {
    switch (code) {
        case WUBU_PREVIEW_OK:      return "ok";
        case WUBU_PREVIEW_OPEN:    return "cannot read the file";
        case WUBU_PREVIEW_FORMAT:  return "no decoder for this format";
        case WUBU_PREVIEW_DECODE:  return "the decoder refused the bytes";
        case WUBU_PREVIEW_TOO_BIG: return "larger than the preview cap";
        case WUBU_PREVIEW_NOMEM:   return "out of memory";
        default:                   return "invalid";
    }
}

static const char *ext_of(const char *path) {
    const char *dot = strrchr(path, '.');
    if (!dot || !dot[1]) return "";
    return dot + 1;
}

static int eq_ci(const char *a, const char *b) {
    for (; *a && *b; a++, b++) {
        char x = *a, y = *b;
        if (x >= 'A' && x <= 'Z') x += ('a' - 'A');
        if (y >= 'A' && y <= 'Z') y += ('a' - 'A');
        if (x != y) return 0;
    }
    return *a == *b;
}

static unsigned char *read_all(const char *path, size_t *len) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return NULL; }
    long n = ftell(f);
    rewind(f);
    if (n <= 0) { fclose(f); return NULL; }
    unsigned char *b = malloc((size_t)n);
    if (!b) { fclose(f); return NULL; }
    if (fread(b, 1, (size_t)n, f) != (size_t)n) { free(b); fclose(f); return NULL; }
    fclose(f);
    *len = (size_t)n;
    return b;
}

/* RGB(A) planes -> packed 0x00RRGGBB. Alpha is dropped: the preview pane draws on an opaque
 * background, and a half-transparent image is better shown than hidden. */
static uint32_t *pack(const unsigned char *src, int w, int h, int ch, size_t max_pixels) {
    if (w <= 0 || h <= 0) return NULL;
    if ((size_t)w * (size_t)h > max_pixels) return NULL;
    uint32_t *out = malloc((size_t)w * (size_t)h * sizeof *out);
    if (!out) return NULL;
    for (size_t i = 0, n = (size_t)w * (size_t)h; i < n; i++) {
        unsigned char r, g, b;
        if (ch == 1) { r = g = b = src[i]; }
        else if (ch == 3) { r = src[i*3]; g = src[i*3+1]; b = src[i*3+2]; }
        else { r = src[i*4]; g = src[i*4+1]; b = src[i*4+2]; }
        out[i] = ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
    }
    return out;
}

static int decode_bytes(const char *name, const unsigned char *file, size_t len,
                        uint32_t **out_px, int *w, int *h, size_t max_pixels) {
    const char *ext = ext_of(name);
    int png = eq_ci(ext, "png"), bmp = eq_ci(ext, "bmp"), gif = eq_ci(ext, "gif");
    int jpg = eq_ci(ext, "jpg") || eq_ci(ext, "jpeg");
    if (!png && !bmp && !gif && !jpg) return WUBU_PREVIEW_FORMAT;

    int rc = WUBU_PREVIEW_DECODE;
    if (jpg) {
        WubuJpegImage img; int err = 0;
        if (wubu_jpeg_decode(file, len, &img, max_pixels, &err) == WUBU_JPEG_OK) {
            *out_px = pack(img.rgb, img.w, img.h, 3, max_pixels);
            *w = img.w; *h = img.h;
            wubu_jpeg_free(&img);
            rc = *out_px ? WUBU_PREVIEW_OK : WUBU_PREVIEW_NOMEM;
        }
    } else if (gif) {
        WubuGif info; int err = 0;
        if (wubu_gif_info(file, len, &info, &err) == 0 &&
            (size_t)info.width * (size_t)info.height <= max_pixels) {
            size_t cap = (size_t)info.width * (size_t)info.height * 4;
            unsigned char *rgba = malloc(cap);
            if (!rgba) rc = WUBU_PREVIEW_NOMEM;
            else {
                /* Frame 0 through the COMPOSITOR, not the single-frame path: an animated GIF's
                 * first frame is composited onto the background, and that is what the animation
                 * shows. wubu_gif_frame is also the path that proves the animation decoder is
                 * reachable from the shipped binary. */
                if (wubu_gif_frame(file, len, 0, rgba, cap, &err) == 0) {
                    *out_px = pack(rgba, info.width, info.height, 4, max_pixels);
                    *w = info.width; *h = info.height;
                    rc = *out_px ? WUBU_PREVIEW_OK : WUBU_PREVIEW_NOMEM;
                }
                free(rgba);
            }
        } else {
            rc = WUBU_PREVIEW_TOO_BIG;
        }
    } else {
        WubuPixmap px; int err = 0;
        if (wubu_pixmap_info(file, len, &px, &err) == 0) {
            if ((size_t)px.width * (size_t)px.height > max_pixels) rc = WUBU_PREVIEW_TOO_BIG;
            else {
                /* PNG needs the concatenated IDAT as scratch (never larger than the file);
                 * every row also costs one filter byte in the output plane. */
                size_t outcap = (size_t)px.width * (size_t)px.height * (size_t)px.channels + (size_t)px.height;
                unsigned char *scratch = malloc(len), *plane = malloc(outcap);
                if (!scratch || !plane) rc = WUBU_PREVIEW_NOMEM;
                else if (wubu_pixmap_decode(file, len, scratch, len, plane, outcap, &px, &err) == 0) {
                    *out_px = pack(plane, px.width, px.height, px.channels, max_pixels);
                    *w = px.width; *h = px.height;
                    rc = *out_px ? WUBU_PREVIEW_OK : WUBU_PREVIEW_NOMEM;
                }
                free(scratch); free(plane);
            }
        }
    }
    if (rc != WUBU_PREVIEW_OK) { free(*out_px); *out_px = NULL; *w = *h = 0; }
    return rc;
}

int wubu_preview_decode_bytes(const char *name, const unsigned char *data, size_t len,
                              uint32_t **out_px, int *w, int *h, size_t max_pixels) {
    if (!name || !data || !len || !out_px || !w || !h) return WUBU_PREVIEW_OPEN;
    *out_px = NULL; *w = 0; *h = 0;
    if (!max_pixels) max_pixels = 24u * 1000u * 1000u;
    return decode_bytes(name, data, len, out_px, w, h, max_pixels);
}

int wubu_preview_decode_file(const char *path, uint32_t **out_px, int *w, int *h, size_t max_pixels) {
    if (!path || !out_px || !w || !h) return WUBU_PREVIEW_OPEN;
    *out_px = NULL; *w = 0; *h = 0;
    if (!max_pixels) max_pixels = 24u * 1000u * 1000u;

    /* Ask about the FORMAT before touching the disk: "there is no webp decoder" is true
     * whether or not the file happens to exist, and answering "cannot read the file" would
     * send a user looking for the wrong problem. */
    const char *ext = ext_of(path);
    if (!eq_ci(ext, "png") && !eq_ci(ext, "bmp") && !eq_ci(ext, "gif") &&
        !eq_ci(ext, "jpg") && !eq_ci(ext, "jpeg"))
        return WUBU_PREVIEW_FORMAT;

    size_t len = 0;
    unsigned char *file = read_all(path, &len);
    if (!file) return WUBU_PREVIEW_OPEN;
    int rc = decode_bytes(path, file, len, out_px, w, h, max_pixels);
    free(file);
    return rc;
}

