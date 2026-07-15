/*
 * wubu_canvas_filter.c -- WuBuOS canvas: per-layer pixel filters
 * (blur, sharpen, edge, invert, threshold, grayscale).
 *
 * Self-contained: depends only on wubu_canvas.h for the WubuCanvas /
 * WubuLayer types and the public wubu_cv_filter_* API. Each filter allocates a
 * scratch buffer, computes into it, then memcpy's back -- no shared state.
 * Minimal includes.
 */

#include "wubu_canvas.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

void wubu_cv_filter_blur(WubuCanvas *cv, int radius) {
    if (!cv || cv->active_layer < 0 || radius < 1) return;
    WubuLayer *l = &cv->layers[cv->active_layer];
    if (!l->pixels) return;
    int w = l->w, h = l->h;
    uint32_t *tmp = (uint32_t *)calloc((size_t)(w * h), sizeof(uint32_t));
    if (!tmp) return;
    int r = radius > 8 ? 8 : radius;
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            int cr = 0, cg = 0, cb = 0, ca = 0, cnt = 0;
            for (int dy = -r; dy <= r; dy++) {
                for (int dx = -r; dx <= r; dx++) {
                    int nx = x + dx, ny = y + dy;
                    if (nx < 0 || nx >= w || ny < 0 || ny >= h) continue;
                    uint32_t p = l->pixels[ny * w + nx];
                    ca += (p >> 24) & 0xFF; cr += (p >> 16) & 0xFF;
                    cg += (p >> 8) & 0xFF; cb += p & 0xFF;
                    cnt++;
                }
            }
            tmp[y * w + x] = ((uint32_t)(ca / cnt) << 24) | ((uint32_t)(cr / cnt) << 16) |
                             ((uint32_t)(cg / cnt) << 8) | (uint32_t)(cb / cnt);
        }
    }
    memcpy(l->pixels, tmp, (size_t)(w * h) * sizeof(uint32_t));
    free(tmp);
}

void wubu_cv_filter_sharpen(WubuCanvas *cv, int amount) {
    (void)amount;
    if (!cv || cv->active_layer < 0) return;
    WubuLayer *l = &cv->layers[cv->active_layer];
    if (!l->pixels) return;
    int w = l->w, h = l->h;
    uint32_t *tmp = (uint32_t *)calloc((size_t)(w * h), sizeof(uint32_t));
    if (!tmp) return;
    /* 3x3 sharpen kernel: 0 -1 0 / -1 5 -1 / 0 -1 0 */
    for (int y = 1; y < h - 1; y++) {
        for (int x = 1; x < w - 1; x++) {
            int cr = 0, cg = 0, cb = 0, ca = 0;
            for (int dy = -1; dy <= 1; dy++) {
                for (int dx = -1; dx <= 1; dx++) {
                    int weight = (dx == 0 && dy == 0) ? 5 : ((dx == 0 || dy == 0) ? -1 : 0);
                    uint32_t p = l->pixels[(y + dy) * w + (x + dx)];
                    ca += weight * ((p >> 24) & 0xFF);
                    cr += weight * ((p >> 16) & 0xFF);
                    cg += weight * ((p >> 8) & 0xFF);
                    cb += weight * (p & 0xFF);
                }
            }
            if (ca < 0) ca = 0; if (ca > 255) ca = 255;
            if (cr < 0) cr = 0; if (cr > 255) cr = 255;
            if (cg < 0) cg = 0; if (cg > 255) cg = 255;
            if (cb < 0) cb = 0; if (cb > 255) cb = 255;
            tmp[y * w + x] = ((uint32_t)ca << 24) | ((uint32_t)cr << 16) | ((uint32_t)cg << 8) | (uint32_t)cb;
        }
    }
    memcpy(l->pixels, tmp, (size_t)(w * h) * sizeof(uint32_t));
    free(tmp);
}

void wubu_cv_filter_edge(WubuCanvas *cv) {
    wubu_cv_filter_sharpen(cv, 1);
}

void wubu_cv_filter_invert(WubuCanvas *cv) {
    if (!cv || cv->active_layer < 0) return;
    WubuLayer *l = &cv->layers[cv->active_layer];
    if (!l->pixels) return;
    int total = l->w * l->h;
    for (int i = 0; i < total; i++) {
        uint32_t p = l->pixels[i];
        uint32_t a = p & 0xFF000000;
        uint32_t r = 255 - ((p >> 16) & 0xFF);
        uint32_t g = 255 - ((p >> 8) & 0xFF);
        uint32_t b = 255 - (p & 0xFF);
        l->pixels[i] = a | (r << 16) | (g << 8) | b;
    }
}

void wubu_cv_filter_threshold(WubuCanvas *cv, int t) {
    if (!cv || cv->active_layer < 0) return;
    WubuLayer *l = &cv->layers[cv->active_layer];
    if (!l->pixels) return;
    int total = l->w * l->h;
    for (int i = 0; i < total; i++) {
        uint32_t p = l->pixels[i];
        int a = (p >> 24) & 0xFF;
        int r = (p >> 16) & 0xFF;
        int g = (p >> 8) & 0xFF;
        int b = p & 0xFF;
        int gray = (r + g + b) / 3;
        int v = gray > t ? 255 : 0;
        l->pixels[i] = ((uint32_t)a << 24) | ((uint32_t)v << 16) | ((uint32_t)v << 8) | (uint32_t)v;
    }
}

void wubu_cv_filter_grayscale(WubuCanvas *cv) {
    if (!cv || cv->active_layer < 0) return;
    WubuLayer *l = &cv->layers[cv->active_layer];
    if (!l->pixels) return;
    int total = l->w * l->h;
    for (int i = 0; i < total; i++) {
        uint32_t p = l->pixels[i];
        int a = (p >> 24) & 0xFF;
        int r = (p >> 16) & 0xFF;
        int g = (p >> 8) & 0xFF;
        int b = p & 0xFF;
        int gray = (r * 30 + g * 59 + b * 11) / 100;
        l->pixels[i] = ((uint32_t)a << 24) | ((uint32_t)gray << 16) | ((uint32_t)gray << 8) | (uint32_t)gray;
    }
}
