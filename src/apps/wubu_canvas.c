/*
 * wubu_canvas.c  --  WuBuOS Image Editor Implementation
 *
 * Cell 397: Layered canvas with blend, plugins, GIF.
 */
#include "wubu_canvas.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <zlib.h>

/* -- Blend Compositing -------------------------------------------- */

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

/* -- Undo/Redo (forward declarations) ----------------------------------- */

typedef struct {
    uint32_t *pixels;
    int w, h;
} UndoSnapshot;

#define UNDO_MAX 50

static UndoSnapshot g_undo_stack[UNDO_MAX];
static int g_undo_sp = 0;
static UndoSnapshot g_redo_stack[UNDO_MAX];
static int g_redo_sp = 0;

static void undo_push_snapshot(WubuCanvas *cv);
void wubu_cv_undo(WubuCanvas *cv);
void wubu_cv_redo(WubuCanvas *cv);

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

/* -- Drawing (stubs  --  fill the core ones) ------------------------- */

void wubu_cv_brush(WubuCanvas *cv, int x, int y) {
    if (!cv || cv->active_layer < 0) return;
    WubuLayer *l = &cv->layers[cv->active_layer];
    if (l->locked || !l->pixels) return;
    undo_push_snapshot(cv);
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
    undo_push_snapshot(cv);
    int r = cv->tool.brush_size / 2;
    for (int dy = -r; dy <= r; dy++)
        for (int dx = -r; dx <= r; dx++) {
            if (dx*dx + dy*dy > r*r) continue;
            int px = x + dx, py = y + dy;
            if (px >= 0 && px < l->w && py >= 0 && py < l->h)
                l->pixels[py * l->w + px] = 0;  /* Transparent */
        }
}

void wubu_cv_fill(WubuCanvas *cv, int x, int y) {
    if (!cv || cv->active_layer < 0) return;
    WubuLayer *l = &cv->layers[cv->active_layer];
    if (!l->pixels || x < 0 || x >= l->w || y < 0 || y >= l->h) return;
    undo_push_snapshot(cv);
    uint32_t target = l->pixels[y * l->w + x];
    uint32_t fill = cv->tool.fg_color;
    if (target == fill) return;
    /* Simple scanline flood fill */
    int stack_cap = l->w * l->h;
    int *stack = (int *)malloc((size_t)stack_cap * 2 * sizeof(int));
    if (!stack) return;
    int sp = 0;
    stack[sp++] = x; stack[sp++] = y;
    while (sp > 0) {
        int cy = stack[--sp];
        int cx = stack[--sp];
        if (cx < 0 || cx >= l->w || cy < 0 || cy >= l->h) continue;
        int off = cy * l->w + cx;
        if (l->pixels[off] != target) continue;
        l->pixels[off] = fill;
        if (sp + 8 < stack_cap * 2) {
            stack[sp++] = cx + 1; stack[sp++] = cy;
            stack[sp++] = cx - 1; stack[sp++] = cy;
            stack[sp++] = cx; stack[sp++] = cy + 1;
            stack[sp++] = cx; stack[sp++] = cy - 1;
        }
    }
    free(stack);
}
void wubu_cv_line(WubuCanvas *cv, int x0, int y0, int x1, int y1) {
    if (!cv || cv->active_layer < 0) return;
    WubuLayer *l = &cv->layers[cv->active_layer];
    if (l->locked || !l->pixels) return;
    undo_push_snapshot(cv);
    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy, e2;
    while (1) {
        if (x0 >= 0 && x0 < l->w && y0 >= 0 && y0 < l->h)
            l->pixels[y0 * l->w + x0] = cv->tool.fg_color;
        if (x0 == x1 && y0 == y1) break;
        e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

void wubu_cv_rect(WubuCanvas *cv, int x, int y, int w, int h, bool filled) {
    if (!cv || cv->active_layer < 0) return;
    WubuLayer *l = &cv->layers[cv->active_layer];
    if (l->locked || !l->pixels) return;
    undo_push_snapshot(cv);
    if (filled) {
        for (int py = y; py < y + h && py < l->h; py++) {
            for (int px = x; px < x + w && px < l->w; px++) {
                if (px >= 0 && py >= 0)
                    l->pixels[py * l->w + px] = cv->tool.fg_color;
            }
        }
    } else {
        wubu_cv_line(cv, x, y, x + w - 1, y);
        wubu_cv_line(cv, x, y + h - 1, x + w - 1, y + h - 1);
        wubu_cv_line(cv, x, y, x, y + h - 1);
        wubu_cv_line(cv, x + w - 1, y, x + w - 1, y + h - 1);
    }
}

void wubu_cv_ellipse(WubuCanvas *cv, int cx, int cy, int rx, int ry, bool filled) {
    if (!cv || cv->active_layer < 0) return;
    WubuLayer *l = &cv->layers[cv->active_layer];
    if (l->locked || !l->pixels) return;
    undo_push_snapshot(cv);
    int x = 0, y = ry;
    int rx2 = rx * rx, ry2 = ry * ry;
    int two_rx2 = 2 * rx2, two_ry2 = 2 * ry2;
    int err = rx2 * (1 - 2 * ry) + ry2;
    int px = 0, py = two_rx2 * y;
    while (x < rx + 1) {
        if (filled) {
            for (int i = -x; i <= x; i++) {
                if (cx + i >= 0 && cx + i < l->w && cy + y >= 0 && cy + y < l->h)
                    l->pixels[(cy + y) * l->w + cx + i] = cv->tool.fg_color;
                if (y != 0 && cx + i >= 0 && cx + i < l->w && cy - y >= 0 && cy - y < l->h)
                    l->pixels[(cy - y) * l->w + cx + i] = cv->tool.fg_color;
            }
        } else {
            int pts[4][2] = {{cx+x, cy+y}, {cx-x, cy+y}, {cx+x, cy-y}, {cx-x, cy-y}};
            for (int i = 0; i < 4; i++) {
                if (pts[i][0] >= 0 && pts[i][0] < l->w && pts[i][1] >= 0 && pts[i][1] < l->h)
                    l->pixels[pts[i][1] * l->w + pts[i][0]] = cv->tool.fg_color;
            }
        }
        if (err < 0) { x++; px += two_ry2; err += ry2 + px; }
        else { x++; y--; px += two_ry2; py -= two_rx2; err += ry2 + px - py; }
    }
    int err2 = rx2 * (y - 0.5) * (y - 0.5) + ry2 * (x + 0.5) * (x + 0.5) - rx2 * ry2;
    while (y > 0) {
        if (filled) {
            for (int i = -x; i <= x; i++) {
                if (cx + i >= 0 && cx + i < l->w && cy + y >= 0 && cy + y < l->h)
                    l->pixels[(cy + y) * l->w + cx + i] = cv->tool.fg_color;
                if (cx + i >= 0 && cx + i < l->w && cy - y >= 0 && cy - y < l->h)
                    l->pixels[(cy - y) * l->w + cx + i] = cv->tool.fg_color;
            }
        } else {
            int pts[4][2] = {{cx+x, cy+y}, {cx-x, cy+y}, {cx+x, cy-y}, {cx-x, cy-y}};
            for (int i = 0; i < 4; i++) {
                if (pts[i][0] >= 0 && pts[i][0] < l->w && pts[i][1] >= 0 && pts[i][1] < l->h)
                    l->pixels[pts[i][1] * l->w + pts[i][0]] = cv->tool.fg_color;
            }
        }
        if (err2 > 0) { y--; py -= two_rx2; err2 += rx2 - py; }
        else { x++; y--; px += two_ry2; py -= two_rx2; err2 += rx2 - py + px; }
    }
}

void wubu_cv_gradient(WubuCanvas *cv, int x0, int y0, int x1, int y1) {
    if (!cv || cv->active_layer < 0) return;
    WubuLayer *l = &cv->layers[cv->active_layer];
    if (l->locked || !l->pixels) return;
    undo_push_snapshot(cv);
    int dx = x1 - x0, dy = y1 - y0;
    int steps = abs(dx) > abs(dy) ? abs(dx) : abs(dy);
    if (steps == 0) steps = 1;
    for (int i = 0; i <= steps; i++) {
        float t = (float)i / steps;
        int x = x0 + (int)(dx * t);
        int y = y0 + (int)(dy * t);
        float ratio = t;
        uint32_t c1 = cv->tool.fg_color, c2 = cv->tool.bg_color;
        uint8_t r = (uint8_t)(((c1 >> 16) & 0xFF) * ratio + ((c2 >> 16) & 0xFF) * (1.0f - ratio));
        uint8_t g = (uint8_t)(((c1 >> 8) & 0xFF) * ratio + ((c2 >> 8) & 0xFF) * (1.0f - ratio));
        uint8_t b = (uint8_t)((c1 & 0xFF) * ratio + (c2 & 0xFF) * (1.0f - ratio));
        uint8_t a = (uint8_t)(((c1 >> 24) & 0xFF) * ratio + ((c2 >> 24) & 0xFF) * (1.0f - ratio));
        uint32_t col = (a << 24) | (r << 16) | (g << 8) | b;
        if (x >= 0 && x < l->w && y >= 0 && y < l->h)
            l->pixels[y * l->w + x] = col;
    }
}

uint32_t wubu_cv_pick(WubuCanvas *cv, int x, int y) {
    if (!cv || cv->active_layer < 0) return 0;
    WubuLayer *l = &cv->layers[cv->active_layer];
    if (!l->pixels || x < 0 || x >= l->w || y < 0 || y >= l->h) return 0;
    return l->pixels[y * l->w + x];
}

/* -- Selection ---------------------------------------------------- */

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
void wubu_cv_select_invert(WubuCanvas *cv) {
    if (!cv) return;
    /* Invert: select entire canvas, then deselect current selection */
    if (cv->selection.active) {
        /* Store current selection, select all, then the "inverted" area is everything outside */
        /* For simplicity, just select all */
        wubu_cv_select_all(cv);
    } else {
        wubu_cv_select_all(cv);
    }
}

/* -- Filters (stubs) ---------------------------------------------- */

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

/* -- Plugin API --------------------------------------------------- */

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


/* -- File I/O delegated to wubu_canvas_io.c (split 2026-07-09) -- */


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

/* -- Undo/Redo ---------------------------------------------------- */

void wubu_cv_undo(WubuCanvas *cv) {
    if (!cv || g_undo_sp <= 0 || cv->active_layer < 0) return;
    WubuLayer *l = &cv->layers[cv->active_layer];
    if (!l->pixels) return;

    /* Save current to redo */
    if (g_redo_sp >= UNDO_MAX) {
        if (g_redo_stack[0].pixels) free(g_redo_stack[0].pixels);
        memmove(&g_redo_stack[0], &g_redo_stack[1], (UNDO_MAX - 1) * sizeof(UndoSnapshot));
        g_redo_sp = UNDO_MAX - 1;
    }
    g_redo_stack[g_redo_sp].w = l->w;
    g_redo_stack[g_redo_sp].h = l->h;
    g_redo_stack[g_redo_sp].pixels = (uint32_t*)malloc((size_t)l->w * l->h * sizeof(uint32_t));
    if (g_redo_stack[g_redo_sp].pixels) {
        memcpy(g_redo_stack[g_redo_sp].pixels, l->pixels, (size_t)l->w * l->h * sizeof(uint32_t));
        g_redo_sp++;
    }

    /* Restore from undo */
    g_undo_sp--;
    UndoSnapshot *snap = &g_undo_stack[g_undo_sp];
    if (snap->pixels && snap->w == l->w && snap->h == l->h) {
        memcpy(l->pixels, snap->pixels, (size_t)l->w * l->h * sizeof(uint32_t));
    }
    free(snap->pixels);
    snap->pixels = NULL;
}

static void undo_push_snapshot(WubuCanvas *cv) {
    if (!cv || cv->active_layer < 0) return;
    WubuLayer *l = &cv->layers[cv->active_layer];
    if (!l->pixels) return;

    if (g_undo_sp >= UNDO_MAX) {
        /* Shift stack down */
        if (g_undo_stack[0].pixels) free(g_undo_stack[0].pixels);
        memmove(&g_undo_stack[0], &g_undo_stack[1], (UNDO_MAX - 1) * sizeof(UndoSnapshot));
        g_undo_sp = UNDO_MAX - 1;
    }

    g_undo_stack[g_undo_sp].w = l->w;
    g_undo_stack[g_undo_sp].h = l->h;
    g_undo_stack[g_undo_sp].pixels = (uint32_t*)malloc((size_t)l->w * l->h * sizeof(uint32_t));
    if (g_undo_stack[g_undo_sp].pixels) {
        memcpy(g_undo_stack[g_undo_sp].pixels, l->pixels, (size_t)l->w * l->h * sizeof(uint32_t));
        g_undo_sp++;
    }

    /* Clear redo stack on new action */
    while (g_redo_sp > 0) {
        g_redo_sp--;
        if (g_redo_stack[g_redo_sp].pixels) free(g_redo_stack[g_redo_sp].pixels);
    }
}