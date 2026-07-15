/*
 * wubu_canvas_layers.c -- WuBuOS canvas: create/destroy + layer ops +
 *                         compositing.
 *
 * Self-contained: depends only on wubu_canvas.h (WubuCanvas / WubuLayer
 * types, wubu_blend for compositing, public wubu_cv_* layer API). Minimal
 * includes. Originally part of the wubu_canvas.c monolith; the pixel-level
 * drawing tools live in wubu_canvas_draw.c, filters in wubu_canvas_filter.c.
 */

#include "wubu_canvas.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* -- Canvas Create/Destroy ----------------------------------------- */

WubuCanvas *wubu_cv_create(int w, int h) {
    WubuCanvas *cv = (WubuCanvas*)calloc(1, sizeof(WubuCanvas));
    if (!cv) return NULL;
    cv->w = w; cv->h = h;
    cv->n_layers = 0; cv->active_layer = -1;
    cv->zoom = 1.0;
    cv->tool.fg_color = 0x00FFFFFF;
    cv->tool.bg_color = 0x00000000;
    cv->tool.brush_size = 8;
    cv->tool.brush_hardness = 100;
    cv->tool.anti_alias = true;
    /* Add initial background layer */
    wubu_cv_layer_add(cv, "Background");
    return cv;
}

void wubu_cv_destroy(WubuCanvas *cv) {
    if (!cv) return;
    for (int i = 0; i < cv->n_layers; i++)
        if (cv->layers[i].pixels) free(cv->layers[i].pixels);
    for (int i = 0; i < cv->n_plugins; i++)
        if (cv->plugins[i].destroy)
            cv->plugins[i].destroy(cv->plugins[i].user_data);
    free(cv);
}

/* -- Layer Operations --------------------------------------------- */

int wubu_cv_layer_add(WubuCanvas *cv, const char *name) {
    if (!cv || cv->n_layers >= WUBU_CV_MAX_LAYERS) return -1;
    WubuLayer *l = &cv->layers[cv->n_layers];
    memset(l, 0, sizeof(WubuLayer));
    strncpy(l->name, name ? name : "Layer", sizeof(l->name) - 1);
    l->w = cv->w; l->h = cv->h;
    l->x = 0; l->y = 0;
    l->opacity = 255;
    l->blend = BLEND_NORMAL;
    l->visible = true;
    l->pixels = (uint32_t*)calloc(cv->w * cv->h, sizeof(uint32_t));
    if (!l->pixels) return -1;
    /* Fill first layer white, rest transparent */
    if (cv->n_layers == 0)
        memset(l->pixels, 0xFF, cv->w * cv->h * sizeof(uint32_t));
    cv->active_layer = cv->n_layers;
    cv->n_layers++;
    return cv->active_layer;
}

int wubu_cv_layer_add_from_data(WubuCanvas *cv, const char *name,
                                 const uint32_t *pixels, int w, int h) {
    int idx = wubu_cv_layer_add(cv, name);
    if (idx < 0) return -1;
    WubuLayer *l = &cv->layers[idx];
    l->w = w; l->h = h;
    memcpy(l->pixels, pixels, w * h * sizeof(uint32_t));
    return idx;
}

void wubu_cv_layer_remove(WubuCanvas *cv, int idx) {
    if (!cv || idx < 0 || idx >= cv->n_layers) return;
    if (cv->layers[idx].pixels) free(cv->layers[idx].pixels);
    for (int i = idx; i < cv->n_layers - 1; i++)
        cv->layers[i] = cv->layers[i + 1];
    cv->n_layers--;
    if (cv->active_layer >= cv->n_layers)
        cv->active_layer = cv->n_layers - 1;
}

void wubu_cv_layer_set_opacity(WubuCanvas *cv, int idx, uint8_t opacity) {
    if (cv && idx >= 0 && idx < cv->n_layers) cv->layers[idx].opacity = opacity;
}
void wubu_cv_layer_set_blend(WubuCanvas *cv, int idx, WubuBlendMode blend) {
    if (cv && idx >= 0 && idx < cv->n_layers) cv->layers[idx].blend = blend;
}
void wubu_cv_layer_set_visible(WubuCanvas *cv, int idx, bool visible) {
    if (cv && idx >= 0 && idx < cv->n_layers) cv->layers[idx].visible = visible;
}
void wubu_cv_layer_set_locked(WubuCanvas *cv, int idx, bool locked) {
    if (cv && idx >= 0 && idx < cv->n_layers) cv->layers[idx].locked = locked;
}
WubuLayer *wubu_cv_layer_get(WubuCanvas *cv, int idx) {
    if (!cv || idx < 0 || idx >= cv->n_layers) return NULL;
    return &cv->layers[idx];
}

void wubu_cv_layer_dup(WubuCanvas *cv, int idx) {
    if (!cv || idx < 0 || idx >= cv->n_layers) return;
    WubuLayer *src = &cv->layers[idx];
    char name[64]; snprintf(name, sizeof(name), "%s copy", src->name);
    int new_idx = wubu_cv_layer_add(cv, name);
    if (new_idx < 0) return;
    WubuLayer *dst = &cv->layers[new_idx];
    dst->opacity = src->opacity;
    dst->blend = src->blend;
    memcpy(dst->pixels, src->pixels, cv->w * cv->h * sizeof(uint32_t));
}

void wubu_cv_layer_move(WubuCanvas *cv, int from, int to) {
    if (!cv || from == to) return;
    if (from < 0 || from >= cv->n_layers || to < 0 || to >= cv->n_layers) return;
    WubuLayer tmp = cv->layers[from];
    if (from < to) memmove(&cv->layers[from], &cv->layers[from+1], (to-from)*sizeof(WubuLayer));
    else memmove(&cv->layers[to+1], &cv->layers[to], (from-to)*sizeof(WubuLayer));
    cv->layers[to] = tmp;
}

void wubu_cv_layer_merge_down(WubuCanvas *cv, int idx) {
    if (!cv || idx <= 0 || idx >= cv->n_layers) return;
    WubuLayer *top = &cv->layers[idx];
    WubuLayer *bot = &cv->layers[idx - 1];
    if (!top->pixels || !bot->pixels) return;
    int w = top->w < bot->w ? top->w : bot->w;
    int h = top->h < bot->h ? top->h : bot->h;
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            uint32_t tp = top->pixels[y * top->w + x];
            uint32_t bp = bot->pixels[y * bot->w + x];
            int ta = (tp >> 24) & 0xFF;
            int ba = (bp >> 24) & 0xFF;
            int oa = ta + ba * (255 - ta) / 255;
            if (oa == 0) { bot->pixels[y * bot->w + x] = 0; continue; }
            int tr = (tp >> 16) & 0xFF, tg = (tp >> 8) & 0xFF, tb = tp & 0xFF;
            int br = (bp >> 16) & 0xFF, bg = (bp >> 8) & 0xFF, bb = bp & 0xFF;
            int rr = (tr * ta + br * ba * (255 - ta) / 255) / oa;
            int rg = (tg * ta + bg * ba * (255 - ta) / 255) / oa;
            int rb = (tb * ta + bb * ba * (255 - ta) / 255) / oa;
            bot->pixels[y * bot->w + x] = ((uint32_t)oa << 24) | ((uint32_t)rr << 16) | ((uint32_t)rg << 8) | (uint32_t)rb;
        }
    }
    /* Remove top layer */
    wubu_cv_layer_remove(cv, idx);
}
void wubu_cv_layer_flatten(WubuCanvas *cv) {
    if (!cv || cv->n_layers <= 1) return;
    /* Merge all layers down to layer 0 */
    while (cv->n_layers > 1) {
        wubu_cv_layer_merge_down(cv, 1);
    }
}

/* -- Composite ---------------------------------------------------- */

void wubu_cv_composite(WubuCanvas *cv, uint32_t *out, int out_w, int out_h) {
    if (!cv || !out) return;
    /* Copy bottom layer, then blend each visible layer on top */
    bool started = false;
    for (int i = 0; i < cv->n_layers; i++) {
        WubuLayer *l = &cv->layers[i];
        if (!l->visible || !l->pixels) continue;
        if (!started) {
            /* First visible layer → copy directly */
            for (int y = 0; y < out_h && y < l->h; y++)
                for (int x = 0; x < out_w && x < l->w; x++)
                    out[y * out_w + x] = l->pixels[y * l->w + x];
            started = true;
        } else {
            /* Blend on top */
            for (int y = 0; y < out_h && y < l->h; y++)
                for (int x = 0; x < out_w && x < l->w; x++) {
                    int idx = y * out_w + x;
                    out[idx] = wubu_blend(out[idx], l->pixels[y * l->w + x],
                                          l->opacity, l->blend);
                }
        }
    }
}
