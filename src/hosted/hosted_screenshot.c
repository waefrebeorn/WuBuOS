/* See hosted_screenshot.h. */
#include "hosted_screenshot.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../gui/wubu_screenshot.h"   /* png_encode_rgba: the encoder the tree already had */

static int has_png_extension(const char *path)
{
    if (!path) return 0;
    size_t n = strlen(path);
    if (n < 4) return 0;
    const char *e = path + n - 4;
    return e[0] == '.' &&
           (e[1] == 'p' || e[1] == 'P') &&
           (e[2] == 'n' || e[2] == 'N') &&
           (e[3] == 'g' || e[3] == 'G');
}

int hosted_screenshot_write(const char *path, const uint32_t *fb, int w, int h)
{
    if (!path || !fb || w <= 0 || h <= 0) return 0;

    if (has_png_extension(path)) {
        uint8_t *png = NULL;
        size_t len = png_encode_rgba(fb, w, h, &png);
        if (len == 0 || !png) return 0;
        FILE *f = fopen(path, "wb");
        if (!f) {
            free(png);
            return 0;
        }
        size_t wrote = fwrite(png, 1, len, f);
        fclose(f);
        free(png);
        return wrote == len;
    }

    FILE *f = fopen(path, "wb");
    if (!f) return 0;
    fprintf(f, "P6\n%d %d\n255\n", w, h);
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            uint32_t c = fb[(size_t)y * (size_t)w + (size_t)x];
            fputc((int)((c >> 16) & 0xFF), f);
            fputc((int)((c >> 8) & 0xFF), f);
            fputc((int)(c & 0xFF), f);
        }
    }
    fclose(f);
    return 1;
}
