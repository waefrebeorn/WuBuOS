/* wubu_canvas_io_ppm.c -- Canvas PPM format save/load (self-contained).
 *
 * wubu_cv_save_ppm (P6 binary) + wubu_cv_load_ppm (P3/P6). Pure stdio/malloc;
 * uses wubu_cv_composite/resize (wubu_canvas.h). No external libs, no shared
 * canvas_io statics. Minimal includes.
 */

#include "wubu_canvas.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int wubu_cv_save_ppm(WubuCanvas *cv, const char *path) {
    if (!cv || !path) return -1;

    uint32_t *flat = (uint32_t*)malloc(cv->w * cv->h * sizeof(uint32_t));
    if (!flat) return -1;
    wubu_cv_composite(cv, flat, cv->w, cv->h);

    FILE *f = fopen(path, "wb");
    if (!f) { free(flat); return -1; }

    fprintf(f, "P6\n%d %d\n255\n", cv->w, cv->h);
    for (int y = 0; y < cv->h; y++) {
        for (int x = 0; x < cv->w; x++) {
            uint32_t px = flat[y * cv->w + x];
            uint8_t rgb[3];
            rgb[0] = (px >> 16) & 0xFF;            /* R */
            rgb[1] = (px >> 8) & 0xFF;             /* G */
            rgb[2] = px & 0xFF;                    /* B */
            fwrite(rgb, 1, 3, f);
        }
    }

    free(flat);
    fclose(f);
    return 0;
}

int wubu_cv_load_ppm(WubuCanvas *cv, const char *path) {
    if (!cv || !path) return -1;

    FILE *f = fopen(path, "r");
    if (!f) return -1;

    char magic[3];
    int w, h, maxval;
    if (fscanf(f, "%2s %d %d %d", magic, &w, &h, &maxval) != 4 || magic[0] != 'P') {
        fclose(f);
        return -1;
    }
    int ch = fgetc(f); /* consume newline */
    (void)ch;

    if (w > 4096 || h > 4096) { fclose(f); return -1; }
    wubu_cv_resize(cv, w, h);
    WubuLayer *l = &cv->layers[cv->active_layer];

    if (magic[1] == '6') {
        /* Binary PPM (P6) */
        for (int y = 0; y < h; y++) {
            for (int x = 0; x < w; x++) {
                unsigned char rgb[3];
                if (fread(rgb, 1, 3, f) != 3) { fclose(f); return -1; }
                l->pixels[y * w + x] = 0xFF000000 | ((uint32_t)rgb[0] << 16) | ((uint32_t)rgb[1] << 8) | (uint32_t)rgb[2];
            }
        }
    } else if (magic[1] == '3') {
        /* ASCII PPM (P3) */
        for (int y = 0; y < h; y++) {
            for (int x = 0; x < w; x++) {
                int r, g, b;
                if (fscanf(f, "%d %d %d", &r, &g, &b) != 3) { fclose(f); return -1; }
                l->pixels[y * w + x] = 0xFF000000 | ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
            }
        }
    }

    fclose(f);
    return 0;
}
