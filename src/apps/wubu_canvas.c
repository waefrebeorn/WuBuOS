/*
 * wubu_canvas.c — WuBuOS Image Editor Implementation
 *
 * Cell 397: Layered canvas with blend, plugins, GIF.
 */
#include "wubu_canvas.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdio.h>

/* ── Blend Compositing ──────────────────────────────────────────── */

static inline uint8_t blend_channel(uint8_t dst, uint8_t src,
                                     uint8_t opacity, WubuBlendMode mode) {
    int a = src, b = dst;
    int result;
    switch (mode) {
        case BLEND_MULTIPLY:   result = a * b / 255; break;
        case BLEND_SCREEN:     result = 255 - (255 - a) * (255 - b) / 255; break;
        case BLEND_DIFFERENCE: result = abs(a - b); break;
        case BLEND_ADDITION:   result = a + b; if (result > 255) result = 255; break;
        case BLEND_SUBTRACT:   result = a - b; if (result < 0) result = 0; break;
        case BLEND_DARKEN:    result = a < b ? a : b; break;
        case BLEND_LIGHTEN:   result = a > b ? a : b; break;
        case BLEND_OVERLAY:
            result = b < 128 ? (2 * a * b / 255) : (255 - 2 * (255 - a) * (255 - b) / 255);
            break;
        case BLEND_COLOR_DODGE:
            result = (a == 255) ? 255 : (b * 255 / (255 - a));
            if (result > 255) result = 255; break;
        case BLEND_COLOR_BURN:
            result = (a == 0) ? 0 : (255 - (255 - b) * 255 / a);
            if (result < 0) result = 0; break;
        case BLEND_HARD_LIGHT:
            result = a < 128 ? (2 * a * b / 255) : (255 - 2 * (255 - a) * (255 - b) / 255);
            break;
        case BLEND_SOFT_LIGHT:
            result = b + (2 * a - 255) * b * (255 - b) / (255 * 255);
            if (result < 0) result = 0; if (result > 255) result = 255;
            break;
        default: result = a; break;  /* NORMAL */
    }
    /* Apply opacity */
    return (uint8_t)(opacity * result / 255 + (255 - opacity) * b / 255);
}

uint32_t wubu_blend(uint32_t dst, uint32_t src, uint8_t opacity,
                     WubuBlendMode mode) {
    uint8_t dr = dst & 0xFF, dg = (dst >> 8) & 0xFF, db = (dst >> 16) & 0xFF;
    uint8_t sr = src & 0xFF, sg = (src >> 8) & 0xFF, sb = (src >> 16) & 0xFF;
    uint8_t r = blend_channel(dr, sr, opacity, mode);
    uint8_t g = blend_channel(dg, sg, opacity, mode);
    uint8_t b = blend_channel(db, sb, opacity, mode);
    return r | (g << 8) | (b << 16);
}

/* ── Canvas Create/Destroy ───────────────────────────────────────── */

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

/* ── Layer Operations ───────────────────────────────────────────── */

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

void wubu_cv_layer_merge_down(WubuCanvas *cv, int idx) { (void)cv; (void)idx; /* TODO */ }
void wubu_cv_layer_flatten(WubuCanvas *cv) { (void)cv; /* TODO */ }

/* ── Composite ──────────────────────────────────────────────────── */

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

/* ── Drawing (stubs — fill the core ones) ───────────────────────── */

void wubu_cv_brush(WubuCanvas *cv, int x, int y) {
    if (!cv || cv->active_layer < 0) return;
    WubuLayer *l = &cv->layers[cv->active_layer];
    if (l->locked || !l->pixels) return;
    int r = cv->tool.brush_size / 2;
    for (int dy = -r; dy <= r; dy++)
        for (int dx = -r; dx <= r; dx++) {
            if (dx*dx + dy*dy > r*r) continue;
            int px = x + dx, py = y + dy;
            if (px >= 0 && px < l->w && py >= 0 && py < l->h)
                l->pixels[py * l->w + px] = cv->tool.fg_color;
        }
}

void wubu_cv_eraser(WubuCanvas *cv, int x, int y) {
    if (!cv || cv->active_layer < 0) return;
    WubuLayer *l = &cv->layers[cv->active_layer];
    if (l->locked || !l->pixels) return;
    int r = cv->tool.brush_size / 2;
    for (int dy = -r; dy <= r; dy++)
        for (int dx = -r; dx <= r; dx++) {
            if (dx*dx + dy*dy > r*r) continue;
            int px = x + dx, py = y + dy;
            if (px >= 0 && px < l->w && py >= 0 && py < l->h)
                l->pixels[py * l->w + px] = 0;  /* Transparent */
        }
}

void wubu_cv_fill(WubuCanvas *cv, int x, int y) { (void)cv; (void)x; (void)y; /* TODO: flood fill */ }
void wubu_cv_line(WubuCanvas *cv, int x0, int y0, int x1, int y1) { (void)cv; (void)x0; (void)y0; (void)x1; (void)y1; }
void wubu_cv_rect(WubuCanvas *cv, int x, int y, int w, int h, bool filled) { (void)cv; (void)x; (void)y; (void)w; (void)h; (void)filled; }
void wubu_cv_ellipse(WubuCanvas *cv, int cx, int cy, int rx, int ry, bool filled) { (void)cv; (void)cx; (void)cy; (void)rx; (void)ry; (void)filled; }
void wubu_cv_gradient(WubuCanvas *cv, int x0, int y0, int x1, int y1) { (void)cv; (void)x0; (void)y0; (void)x1; (void)y1; }

uint32_t wubu_cv_pick(WubuCanvas *cv, int x, int y) {
    if (!cv || cv->active_layer < 0) return 0;
    WubuLayer *l = &cv->layers[cv->active_layer];
    if (!l->pixels || x < 0 || x >= l->w || y < 0 || y >= l->h) return 0;
    return l->pixels[y * l->w + x];
}

/* ── Selection ──────────────────────────────────────────────────── */

void wubu_cv_select_rect(WubuCanvas *cv, int x, int y, int w, int h) {
    if (!cv) return;
    cv->selection.kind = SEL_RECT;
    cv->selection.x = x; cv->selection.y = y;
    cv->selection.w = w; cv->selection.h = h;
    cv->selection.active = true;
}
void wubu_cv_select_ellipse(WubuCanvas *cv, int cx, int cy, int rx, int ry) {
    if (!cv) return;
    cv->selection.kind = SEL_ELLIPSE;
    cv->selection.x = cx - rx; cv->selection.y = cy - ry;
    cv->selection.w = rx * 2; cv->selection.h = ry * 2;
    cv->selection.active = true;
}
void wubu_cv_select_none(WubuCanvas *cv) { if (cv) cv->selection.active = false; }
void wubu_cv_select_all(WubuCanvas *cv) { if (cv) wubu_cv_select_rect(cv, 0, 0, cv->w, cv->h); }
void wubu_cv_select_invert(WubuCanvas *cv) { (void)cv; /* TODO */ }

/* ── Filters (stubs) ────────────────────────────────────────────── */

void wubu_cv_filter_blur(WubuCanvas *cv, int radius)      { (void)cv; (void)radius; }
void wubu_cv_filter_sharpen(WubuCanvas *cv, int amount)   { (void)cv; (void)amount; }
void wubu_cv_filter_edge(WubuCanvas *cv)                   { (void)cv; }
void wubu_cv_filter_invert(WubuCanvas *cv)                 { (void)cv; }
void wubu_cv_filter_threshold(WubuCanvas *cv, int t)       { (void)cv; (void)t; }
void wubu_cv_filter_grayscale(WubuCanvas *cv)              { (void)cv; }

/* ── Plugin API ─────────────────────────────────────────────────── */

int wubu_cv_plugin_register(WubuCanvas *cv, const WubuPlugin *plugin) {
    if (!cv || !plugin || cv->n_plugins >= WUBU_CV_MAX_PLUGINS) return -1;
    cv->plugins[cv->n_plugins] = *plugin;
    cv->plugins[cv->n_plugins].active = true;
    cv->n_plugins++;
    return cv->n_plugins - 1;
}

int wubu_cv_plugin_run(WubuCanvas *cv, int plugin_idx) {
    if (!cv || plugin_idx < 0 || plugin_idx >= cv->n_plugins) return -1;
    WubuPlugin *p = &cv->plugins[plugin_idx];
    if (!p->active) return -1;
    if (p->process_image)
        return p->process_image(cv, p->user_data);
    return 0;
}

void wubu_cv_plugin_unregister(WubuCanvas *cv, int plugin_idx) {
    if (!cv || plugin_idx < 0 || plugin_idx >= cv->n_plugins) return;
    WubuPlugin *p = &cv->plugins[plugin_idx];
    if (p->destroy) p->destroy(p->user_data);
    for (int i = plugin_idx; i < cv->n_plugins - 1; i++)
        cv->plugins[i] = cv->plugins[i + 1];
    cv->n_plugins--;
}

/* ── File I/O (BMP native, PNG/GIF via ffmpeg) ──────────────────── */

int wubu_cv_save_bmp(WubuCanvas *cv, const char *path) {
    if (!cv) return -1;
    uint32_t *flat = (uint32_t*)malloc(cv->w * cv->h * sizeof(uint32_t));
    if (!flat) return -1;
    wubu_cv_composite(cv, flat, cv->w, cv->h);

    FILE *f = fopen(path, "wb");
    if (!f) { free(flat); return -1; }

    int row_size = (cv->w * 3 + 3) & ~3;
    int img_size = row_size * cv->h;
    /* BMP header */
    uint8_t hdr[54] = {0};
    hdr[0] = 'B'; hdr[1] = 'M';
    uint32_t fsize = 54 + img_size;
    memcpy(&hdr[2], &fsize, 4);
    uint32_t off = 54; memcpy(&hdr[10], &off, 4);
    uint32_t sz = 40; memcpy(&hdr[14], &sz, 4);
    int32_t w32 = cv->w, h32 = cv->h;
    memcpy(&hdr[18], &w32, 4); memcpy(&hdr[22], &h32, 4);
    hdr[26] = 1; hdr[28] = 24;
    memcpy(&hdr[34], &img_size, 4);
    fwrite(hdr, 1, 54, f);

    /* Write rows bottom-up */
    uint8_t pad[3] = {0};
    int padding = row_size - cv->w * 3;
    for (int y = cv->h - 1; y >= 0; y--) {
        for (int x = 0; x < cv->w; x++) {
            uint32_t px = flat[y * cv->w + x];
            uint8_t rgb[3] = { px & 0xFF, (px >> 8) & 0xFF, (px >> 16) & 0xFF };
            fwrite(rgb, 1, 3, f);
        }
        if (padding > 0) fwrite(pad, 1, padding, f);
    }
    fclose(f); free(flat);
    return 0;
}

int wubu_cv_save_ppm(WubuCanvas *cv, const char *path) {
    if (!cv) return -1;
    uint32_t *flat = (uint32_t*)malloc(cv->w * cv->h * sizeof(uint32_t));
    if (!flat) return -1;
    wubu_cv_composite(cv, flat, cv->w, cv->h);
    FILE *f = fopen(path, "wb");
    if (!f) { free(flat); return -1; }
    fprintf(f, "P6\n%d %d\n255\n", cv->w, cv->h);
    for (int i = 0; i < cv->w * cv->h; i++) {
        uint8_t rgb[3] = { flat[i] & 0xFF, (flat[i] >> 8) & 0xFF, (flat[i] >> 16) & 0xFF };
        fwrite(rgb, 1, 3, f);
    }
    fclose(f); free(flat);
    return 0;
}

int wubu_cv_save_png(WubuCanvas *cv, const char *path) {
    if (!cv) return -1;
    /* Fallback: save as PPM then ffmpeg convert */
    char ppm[512]; snprintf(ppm, sizeof(ppm), "%s.ppm", path);
    if (wubu_cv_save_ppm(cv, ppm) != 0) return -1;
    char cmd[1024]; snprintf(cmd, sizeof(cmd), "ffmpeg -y -i %s %s 2>/dev/null && rm %s", ppm, path, ppm);
    int ret = system(cmd);
    return WIFEXITED(ret) ? WEXITSTATUS(ret) : -1;
}

int wubu_cv_save_gif(WubuCanvas *cv, const char *path) { (void)cv; (void)path; return -1; /* TODO */ }
int wubu_cv_load(WubuCanvas *cv, const char *path) { (void)cv; (void)path; return -1; /* TODO */ }

/* ── Undo/Redo ──────────────────────────────────────────────────── */

void wubu_cv_undo(WubuCanvas *cv) { (void)cv; }
void wubu_cv_redo(WubuCanvas *cv) { (void)cv; }

/* ── View ───────────────────────────────────────────────────────── */

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

/* ── Canvas ops (stubs) ──────────────────────────────────────────── */

void wubu_cv_resize(WubuCanvas *cv, int new_w, int new_h) { (void)cv; (void)new_w; (void)new_h; }
void wubu_cv_crop(WubuCanvas *cv, int x, int y, int w, int h) { (void)cv; (void)x; (void)y; (void)w; (void)h; }
void wubu_cv_flip_h(WubuCanvas *cv) { (void)cv; }
void wubu_cv_flip_v(WubuCanvas *cv) { (void)cv; }
void wubu_cv_rotate_90(WubuCanvas *cv, bool cw) { (void)cv; (void)cw; }
