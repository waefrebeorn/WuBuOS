/*
 * wubu_wallpaper.c -- WuBuOS Wallpaper Decoder + Placement
 *
 * Real implementation: decodes a BMP (24/32 bpp, uncompressed BITMAPINFOHEADER)
 * into an XRGB8888 buffer compatible with the VBE framebuffer. Implements the
 * five ReactOS PLACEMENT modes as destination-rect math.
 *
 * No stubs, no system(), no external image libs.
 */

#include "wubu_wallpaper.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

/* -- Bundled default wallpaper path -------------------------------- */

const char *wubu_wallpaper_default_path(void) {
    static const char *paths[] = {
        "screenshots/media/wubuos-default.bmp",
        "../screenshots/media/wubuos-default.bmp",
        NULL
    };
    for (int i = 0; paths[i]; i++) {
        if (access(paths[i], F_OK) == 0)
            return paths[i];
    }
    return NULL;
}

/* -- BMP on-disk structures (little-endian, packed) ---------------- */

#pragma pack(push, 1)
typedef struct {
    uint16_t bfType;        /* "BM" = 0x4D42 */
    uint32_t bfSize;
    uint16_t bfReserved1;
    uint16_t bfReserved2;
    uint32_t bfOffBits;
} BITMAPFILEHEADER;

typedef struct {
    uint32_t biSize;
    int32_t  biWidth;
    int32_t  biHeight;      /* positive => bottom-up */
    uint16_t biPlanes;
    uint16_t biBitCount;
    uint32_t biCompression; /* 0 = BI_RGB */
    uint32_t biSizeImage;
    int32_t  biXPelsPerMeter;
    int32_t  biYPelsPerMeter;
    uint32_t biClrUsed;
    uint32_t biClrImportant;
} BITMAPINFOHEADER;
#pragma pack(pop)

#define BMP_BI_RGB 0

/* -- Decode -------------------------------------------------------- */

static int decode_bmp(const char *path, WubuWallpaper *out) {
    FILE *f = fopen(path, "rb");
    if (!f) return 0;

    BITMAPFILEHEADER fh;
    if (fread(&fh, sizeof(fh), 1, f) != 1 || fh.bfType != 0x4D42) {
        fclose(f);
        return 0;
    }

    BITMAPINFOHEADER ih;
    if (fread(&ih, sizeof(ih), 1, f) != 1) {
        fclose(f);
        return 0;
    }
    if (ih.biCompression != BMP_BI_RGB) {        /* only uncompressed */
        fclose(f);
        return 0;
    }
    if (ih.biBitCount != 24 && ih.biBitCount != 32) {
        fclose(f);
        return 0;
    }

    int w = ih.biWidth;
    int h = ih.biHeight < 0 ? -ih.biHeight : ih.biHeight; /* abs height */
    if (w <= 0 || h <= 0 || w > 16384 || h > 16384) {
        fclose(f);
        return 0;
    }

    /* Seek to pixel data. */
    if (fseek(f, (long)fh.bfOffBits, SEEK_SET) != 0) {
        fclose(f);
        return 0;
    }

    int bpp = ih.biBitCount / 8;
    int row_bytes = w * bpp;
    int pad = (4 - (row_bytes & 3)) & 3; /* BMP rows padded to 4 bytes */

    uint32_t *pixels = (uint32_t *)malloc((size_t)w * h * sizeof(uint32_t));
    if (!pixels) {
        fclose(f);
        return 0;
    }

    uint8_t *rowbuf = (uint8_t *)malloc(row_bytes + pad);
    if (!rowbuf) {
        free(pixels);
        fclose(f);
        return 0;
    }

    bool top_down = (ih.biHeight < 0);
    for (int y = 0; y < h; y++) {
        if (fread(rowbuf, 1, row_bytes + pad, f) != (size_t)(row_bytes + pad)) {
            free(pixels); free(rowbuf); fclose(f);
            return 0;
        }
        /* Source row: bottom-up unless top_down. */
        int dst_y = top_down ? y : (h - 1 - y);
        for (int x = 0; x < w; x++) {
            uint8_t *p = rowbuf + x * bpp;
            uint8_t r = p[2], g = p[1], b = p[0];
            /* XRGB8888: 0x00BBGGRR (matches VBE backbuffer). */
            pixels[dst_y * w + x] = (uint32_t)((b << 16) | (g << 8) | r);
        }
    }

    free(rowbuf);
    fclose(f);

    out->pixels = pixels;
    out->w = w;
    out->h = h;
    return 1;
}

int wubu_wallpaper_load(const char *path, WubuWallpaper *out) {
    if (out) { out->pixels = NULL; out->w = 0; out->h = 0; }
    if (!path || !*path || !out) return 0;

    /* Native BMP decoder (no deps). Extend with PNG/JPEG later. */
    size_t len = strlen(path);
    if (len >= 4 && (strcasecmp(path + len - 4, ".bmp") == 0)) {
        return decode_bmp(path, out);
    }
    /* Unknown / unsupported format: caller keeps gradient fallback. */
    return 0;
}

void wubu_wallpaper_free(WubuWallpaper *wp) {
    if (wp && wp->pixels) {
        free(wp->pixels);
        wp->pixels = NULL;
        wp->w = wp->h = 0;
    }
}

/* -- Placement rect math (ReactOS background.c semantics) ---------- */

void wubu_wallpaper_rect(WubuWallpaperMode mode,
                         int img_w, int img_h,
                         int fb_w, int fb_h, int taskbar_h,
                         int *out_x, int *out_y, int *out_w, int *out_h) {
    int avail_h = fb_h - taskbar_h;
    if (avail_h < 1) avail_h = fb_h;

    int x = 0, y = 0, w = img_w, h = img_h;

    switch (mode) {
    case WUBU_WP_CENTER:
        w = img_w; h = img_h;
        x = (fb_w - w) / 2;
        y = (avail_h - h) / 2;
        if (x < 0) x = 0;
        if (y < 0) y = 0;
        break;

    case WUBU_WP_TILE:
        /* One tile; caller repeats across the surface. */
        w = img_w; h = img_h;
        x = 0; y = 0;
        break;

    case WUBU_WP_STRETCH:
        w = fb_w; h = avail_h;
        x = 0; y = 0;
        break;

    case WUBU_WP_FIT: {
        /* Scale to fit entirely inside, preserve aspect (letterbox). */
        double ar = (double)img_w / (double)img_h;
        w = fb_w;
        h = (int)((double)w / ar);
        if (h > avail_h) { h = avail_h; w = (int)((double)h * ar); }
        x = (fb_w - w) / 2;
        y = (avail_h - h) / 2;
        break;
    }

    case WUBU_WP_FILL: {
        /* Scale to cover, preserve aspect (crop overflow). */
        double ar = (double)img_w / (double)img_h;
        w = fb_w;
        h = (int)((double)w / ar);
        if (h < avail_h) { h = avail_h; w = (int)((double)h * ar); }
        x = (fb_w - w) / 2;
        y = (avail_h - h) / 2;
        break;
    }
    }

    if (out_x) *out_x = x;
    if (out_y) *out_y = y;
    if (out_w) *out_w = w;
    if (out_h) *out_h = h;
}
