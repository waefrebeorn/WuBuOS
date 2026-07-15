/*
 * wubu_canvas_transform.c -- WuBuOS canvas: viewport (zoom/pan) and
 *   geometric layer transforms (resize, crop, flip H/V, rotate 90).
 *
 * Self-contained: depends only on wubu_canvas.h for the WubuCanvas /
 * WubuLayer types and the public wubu_cv_* transform API. Pixel-mutating
 * transforms (resize/crop/flip/rotate) scale or shuffle layer buffers
 * in-place. Minimal includes.
 */

#include "wubu_canvas.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* -- View --------------------------------------------------------- */

void wubu_cv_zoom_in(WubuCanvas *cv) {
    if (cv) cv->zoom *= 1.6180339887; /* φ scale */
}
void wubu_cv_zoom_out(WubuCanvas *cv) {
    if (cv) cv->zoom /= 1.6180339887;
}
void wubu_cv_zoom_fit(WubuCanvas *cv) {
    if (cv) cv->zoom = 1.0;
}
void wubu_cv_pan(WubuCanvas *cv, int dx, int dy) {
    if (cv) { cv->pan_x += dx; cv->pan_y += dy; }
}

/* -- Canvas ops (implemented) -------------------------------------------- */

void wubu_cv_resize(WubuCanvas *cv, int new_w, int new_h) {
    if (!cv || new_w <= 0 || new_h <= 0) return;
    if (new_w == cv->w && new_h == cv->h) return;

    for (int i = 0; i < cv->n_layers; i++) {
        WubuLayer *l = &cv->layers[i];
        if (!l->pixels) continue;

        uint32_t *new_pixels = (uint32_t*)calloc((size_t)new_w * new_h, sizeof(uint32_t));
        if (!new_pixels) continue;

        /* Nearest-neighbor scaling */
        float x_ratio = (float)l->w / new_w;
        float y_ratio = (float)l->h / new_h;

        for (int y = 0; y < new_h; y++) {
            int src_y = (int)(y * y_ratio);
            if (src_y >= l->h) src_y = l->h - 1;
            for (int x = 0; x < new_w; x++) {
                int src_x = (int)(x * x_ratio);
                if (src_x >= l->w) src_x = l->w - 1;
                new_pixels[y * new_w + x] = l->pixels[src_y * l->w + src_x];
            }
        }

        free(l->pixels);
        l->pixels = new_pixels;
        l->w = new_w;
        l->h = new_h;
    }

    cv->w = new_w;
    cv->h = new_h;
}

void wubu_cv_crop(WubuCanvas *cv, int x, int y, int w, int h) {
    if (!cv || w <= 0 || h <= 0) return;
    if (x < 0 || y < 0 || x + w > cv->w || y + h > cv->h) return;

    for (int i = 0; i < cv->n_layers; i++) {
        WubuLayer *l = &cv->layers[i];
        if (!l->pixels) continue;

        uint32_t *new_pixels = (uint32_t*)calloc((size_t)w * h, sizeof(uint32_t));
        if (!new_pixels) continue;

        int src_x = x + l->x;
        int src_y = y + l->y;

        for (int dy = 0; dy < h; dy++) {
            int sy = src_y + dy;
            if (sy < 0 || sy >= l->h) continue;
            for (int dx = 0; dx < w; dx++) {
                int sx = src_x + dx;
                if (sx < 0 || sx >= l->w) continue;
                new_pixels[dy * w + dx] = l->pixels[sy * l->w + sx];
            }
        }

        free(l->pixels);
        l->pixels = new_pixels;
        l->w = w;
        l->h = h;
        l->x = 0;
        l->y = 0;
    }

    cv->w = w;
    cv->h = h;
}

void wubu_cv_flip_h(WubuCanvas *cv) {
    if (!cv) return;
    for (int i = 0; i < cv->n_layers; i++) {
        WubuLayer *l = &cv->layers[i];
        if (!l->pixels) continue;
        for (int y = 0; y < l->h; y++) {
            for (int x = 0; x < l->w / 2; x++) {
                int idx1 = y * l->w + x;
                int idx2 = y * l->w + (l->w - 1 - x);
                uint32_t tmp = l->pixels[idx1];
                l->pixels[idx1] = l->pixels[idx2];
                l->pixels[idx2] = tmp;
            }
        }
    }
}

void wubu_cv_flip_v(WubuCanvas *cv) {
    if (!cv) return;
    for (int i = 0; i < cv->n_layers; i++) {
        WubuLayer *l = &cv->layers[i];
        if (!l->pixels) continue;
        for (int y = 0; y < l->h / 2; y++) {
            for (int x = 0; x < l->w; x++) {
                int idx1 = y * l->w + x;
                int idx2 = (l->h - 1 - y) * l->w + x;
                uint32_t tmp = l->pixels[idx1];
                l->pixels[idx1] = l->pixels[idx2];
                l->pixels[idx2] = tmp;
            }
        }
    }
}

void wubu_cv_rotate_90(WubuCanvas *cv, bool cw) {
    if (!cv) return;
    for (int i = 0; i < cv->n_layers; i++) {
        WubuLayer *l = &cv->layers[i];
        if (!l->pixels) continue;

        uint32_t *new_pixels = (uint32_t*)calloc((size_t)l->h * l->w, sizeof(uint32_t));
        if (!new_pixels) continue;

        if (cw) {
            for (int y = 0; y < l->h; y++) {
                for (int x = 0; x < l->w; x++) {
                    new_pixels[x * l->h + (l->h - 1 - y)] = l->pixels[y * l->w + x];
                }
            }
        } else {
            for (int y = 0; y < l->h; y++) {
                for (int x = 0; x < l->w; x++) {
                    new_pixels[(l->w - 1 - x) * l->h + y] = l->pixels[y * l->w + x];
                }
            }
        }

        free(l->pixels);
        l->pixels = new_pixels;
        int tmp = l->w;
        l->w = l->h;
        l->h = tmp;
    }

    int tmp = cv->w;
    cv->w = cv->h;
    cv->h = tmp;
}
