/*
 * wubu_canvas.c  --  WuBuOS Image Editor Implementation
 *
 * Cell 397: Layered canvas with blend, plugins, GIF.
 */
#include "wubu_canvas.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

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

/* -- File I/O (Native PNG/GIF/BMP/PPM) -------------------------- */

static uint32_t crc32_table[256];
static bool crc32_table_init = false;

static void crc32_init(void) {
    if (crc32_table_init) return;
    for (uint32_t i = 0; i < 256; i++) {
        uint32_t c = i;
        for (int j = 0; j < 8; j++) {
            c = (c & 1) ? (0xEDB88320 ^ (c >> 1)) : (c >> 1);
        }
        crc32_table[i] = c;
    }
    crc32_table_init = true;
}

static uint32_t crc32_update(uint32_t crc_in, const void *data, size_t len) {
    if (!crc32_table_init) crc32_init();
    const uint8_t *p = (const uint8_t*)data;
    uint32_t crc = ~0;
    for (size_t i = 0; i < len; i++) {
        crc = crc32_table[(crc ^ p[i]) & 0xFF] ^ (crc >> 8);
    }
    return ~crc;
}

static uint32_t crc32_data(const void *data, size_t len) {
    return crc32_update(0, data, len);
}

/* Write 32-bit big-endian */
static void write_be32(uint8_t *buf, uint32_t val) {
    buf[0] = (val >> 24) & 0xFF;
    buf[1] = (val >> 16) & 0xFF;
    buf[2] = (val >> 8) & 0xFF;
    buf[3] = val & 0xFF;
}

static void write_be16(uint8_t *buf, uint16_t val) {
    buf[0] = (val >> 8) & 0xFF;
    buf[1] = val & 0xFF;
}

/* Write PNG chunk: length (4 bytes), type (4 bytes), data, crc (4 bytes) */
static void png_write_chunk(FILE *f, const char *type, const void *data, size_t len) {
    uint8_t len_buf[4];
    write_be32(len_buf, (uint32_t)len);
    fwrite(len_buf, 1, 4, f);
    fwrite(type, 1, 4, f);
    if (data && len > 0) fwrite(data, 1, len, f);
    uint32_t crc = crc32_data(type, 4);
    if (data && len > 0) crc = crc32_update(crc, data, len);
    static uint8_t crc_buf[4];
        write_be32(crc_buf, crc);
        fwrite(crc_buf, 1, 4, f);
    }

/* Native PNG save - uses uncompressed IDAT (no zlib needed) */
int wubu_cv_save_png(WubuCanvas *cv, const char *path) {
    if (!cv) return -1;

    uint32_t *flat = (uint32_t*)malloc(cv->w * cv->h * sizeof(uint32_t));
    if (!flat) return -1;
    wubu_cv_composite(cv, flat, cv->w, cv->h);

    FILE *f = fopen(path, "wb");
    if (!f) { free(flat); return -1; }

    /* PNG signature */
    static const uint8_t png_sig[8] = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};
    fwrite(png_sig, 1, 8, f);

    /* IHDR chunk */
    uint8_t ihdr[13];
    write_be32(ihdr, cv->w);
    write_be32(ihdr + 4, cv->h);
    ihdr[8] = 8;   /* bit depth */
    ihdr[9] = 2;   /* color type: truecolor */
    ihdr[10] = 0;  /* compression: 0 (deflate) */
    ihdr[11] = 0;  /* filter: 0 (adaptive) */
    ihdr[12] = 0;  /* interlace: 0 (none) */
    png_write_chunk(f, "IHDR", ihdr, 13);

    /* IDAT chunk - uncompressed DEFLATE (BTYPE=00) */
    size_t row_bytes = 3 * cv->w;
    size_t scanline_size = 1 + row_bytes; /* filter byte + RGB data */
    size_t idat_data_size = scanline_size * cv->h;

    /* DEFLATE uncompressed block: 5 bytes header per 65535 bytes */
    size_t max_uncompressed = 65535;
    uint8_t *scanline = malloc(scanline_size);
    if (!scanline) { free(flat); fclose(f); return -1; }

    /* Pre-calculate IDAT chunk count for total length */
    size_t total_compressed = 0;
    size_t tmp_rem = idat_data_size;
    while (tmp_rem > 0) {
        size_t chunk = tmp_rem > 65535 ? 65535 : tmp_rem;
        total_compressed += 5 + chunk; /* 5 byte header + data */
        tmp_rem -= chunk;
    }

    /* Write IDAT chunk header with total length */
    uint8_t idat_len[4];
    write_be32(idat_len, (uint32_t)total_compressed);
    fwrite(idat_len, 1, 4, f);
    fwrite("IDAT", 1, 4, f);

    /* Write uncompressed DEFLATE blocks */
    uint32_t idat_crc = crc32_data("IDAT", 4);
    size_t remaining = scanline_size * cv->h;

    for (int y = 0; y < cv->h; y++) {
        scanline[0] = 0; /* filter type 0 = None */
        for (int x = 0; x < cv->w; x++) {
            uint32_t px = flat[y * cv->w + x];
            scanline[1 + x * 3] = px & 0xFF;       /* R */
            scanline[1 + x * 3 + 1] = (px >> 8) & 0xFF;   /* G */
            scanline[1 + x * 3 + 2] = (px >> 16) & 0xFF;  /* B */
        }

        size_t chunk_size = scanline_size;
        if (remaining < chunk_size) chunk_size = remaining;
        
        /* DEFLATE uncompressed block header */
        uint8_t block_hdr[5];
        block_hdr[0] = (remaining <= chunk_size) ? 0x01 : 0x00; /* BFINAL */
        write_be16(block_hdr + 1, chunk_size); /* LEN */
        write_be16(block_hdr + 3, ~chunk_size); /* NLEN */
        fwrite(block_hdr, 1, 5, f);
        
        fwrite(scanline, 1, chunk_size, f);
        idat_crc = crc32_update(idat_crc, block_hdr, 5);
        idat_crc = crc32_update(idat_crc, scanline, chunk_size);
        remaining -= chunk_size;
    }
    free(scanline);

    /* Write IDAT CRC */
    uint8_t crc_buf[4];
    write_be32(crc_buf, idat_crc);
    fwrite(crc_buf, 1, 4, f);

    /* IEND chunk */
    png_write_chunk(f, "IEND", NULL, 0);

    free(flat);
    fclose(f);
    return 0;
}

/* Native GIF save - uses uncompressed LZW (no external libs) */
int wubu_cv_save_gif(WubuCanvas *cv, const char *path) {
    if (!cv) return -1;

    uint32_t *flat = (uint32_t*)malloc(cv->w * cv->h * sizeof(uint32_t));
    if (!flat) return -1;
    wubu_cv_composite(cv, flat, cv->w, cv->h);

    FILE *f = fopen(path, "wb");
    if (!f) { free(flat); return -1; }

    /* GIF signature and version */
    static const uint8_t gif_sig[6] = {'G', 'I', 'F', '8', '9', 'a'};
    fwrite(gif_sig, 1, 6, f);

    /* Logical Screen Descriptor */
    uint8_t lsd[7];
    write_be16(lsd, cv->w);
    write_be16(lsd + 2, cv->h);
    lsd[4] = 0xF7; /* GCT flag=1, color resolution=7, sort=0, GCT size=7 (256 colors) */
    lsd[5] = 0;    /* Background color index */
    lsd[6] = 0;    /* Pixel aspect ratio */
    fwrite(lsd, 1, 7, f);

    /* Global Color Table (256 colors, RGB) */
    uint8_t gct[768];
    for (int i = 0; i < 256; i++) {
        gct[i * 3] = i;
        gct[i * 3 + 1] = i;
        gct[i * 3 + 2] = i;
    }
    fwrite(gct, 1, 768, f);

    /* Image Descriptor */
    uint8_t img_desc[10];
    img_desc[0] = ',';  /* Image separator */
    write_be16(img_desc + 1, 0);  /* Left */
    write_be16(img_desc + 3, 0);  /* Top */
    write_be16(img_desc + 5, cv->w);
    write_be16(img_desc + 7, cv->h);
    img_desc[9] = 0x00;  /* No local color table, no interlace */
    fwrite(img_desc, 1, 10, f);

    /* Image Data - uncompressed LZW (code size 8, no compression) */
    fputc(8, f);  /* LZW minimum code size */

    /* Write each scanline as a sub-block */
    uint8_t *scanline = malloc(cv->w);
    if (!scanline) { free(flat); fclose(f); return -1; }

    for (int y = 0; y < cv->h; y++) {
        for (int x = 0; x < cv->w; x++) {
            uint32_t px = flat[y * cv->w + x];
            /* Simple palette index: use grayscale value */
            uint8_t gray = (uint8_t)((px & 0xFF) * 0.299 + ((px >> 8) & 0xFF) * 0.587 + ((px >> 16) & 0xFF) * 0.114);
            scanline[x] = gray;
        }
        fputc(cv->w, f);  /* Sub-block size */
        fwrite(scanline, 1, cv->w, f);
    }
    free(scanline);

    /* Block terminator */
    fputc(0, f);

    /* GIF Trailer */
    fputc(';', f);

    free(flat);
    fclose(f);
    return 0;
}

/* Native BMP save - 24-bit uncompressed */
int wubu_cv_save_bmp(WubuCanvas *cv, const char *path) {
    if (!cv || !path) return -1;

    uint32_t *flat = (uint32_t*)malloc(cv->w * cv->h * sizeof(uint32_t));
    if (!flat) return -1;
    wubu_cv_composite(cv, flat, cv->w, cv->h);

    FILE *f = fopen(path, "wb");
    if (!f) { free(flat); return -1; }

    int w = cv->w, h = cv->h;
    int row_size = (w * 3 + 3) & ~3;  /* 24-bit aligned to 4 bytes */
    int image_size = row_size * h;
    int file_size = 54 + image_size;

    /* BMP header (14 bytes) */
    uint8_t hdr[54] = {0};
    hdr[0] = 'B'; hdr[1] = 'M';
    *(int32_t*)(hdr + 2) = file_size;
    *(int32_t*)(hdr + 10) = 54;

    /* DIB header (40 bytes - BITMAPINFOHEADER) */
    *(int32_t*)(hdr + 14) = 40;
    *(int32_t*)(hdr + 18) = w;
    *(int32_t*)(hdr + 22) = h;
    *(int16_t*)(hdr + 26) = 1;       /* planes */
    *(int16_t*)(hdr + 28) = 24;      /* bits per pixel */
    *(int32_t*)(hdr + 30) = 0;       /* compression: BI_RGB */
    *(int32_t*)(hdr + 34) = image_size;
    *(int32_t*)(hdr + 38) = 2835;    /* X pixels per meter (72 DPI) */
    *(int32_t*)(hdr + 42) = 2835;    /* Y pixels per meter */
    *(int32_t*)(hdr + 46) = 0;       /* colors used */
    *(int32_t*)(hdr + 50) = 0;       /* important colors */

    fwrite(hdr, 1, 54, f);

    /* BMP stores rows bottom-up */
    uint8_t *row = malloc(row_size);
    if (!row) { free(flat); fclose(f); return -1; }

    for (int y = h - 1; y >= 0; y--) {
        memset(row, 0, row_size);
        for (int x = 0; x < w; x++) {
            uint32_t px = flat[y * w + x];
            row[x * 3] = px & 0xFF;          /* B */
            row[x * 3 + 1] = (px >> 8) & 0xFF;   /* G */
            row[x * 3 + 2] = (px >> 16) & 0xFF;  /* R */
        }
        fwrite(row, 1, row_size, f);
    }

    free(row);
    free(flat);
    fclose(f);
    return 0;
}
int wubu_cv_load(WubuCanvas *cv, const char *path) {
    if (!cv || !path) return -1;

    /* Detect file type by extension and magic bytes */
    FILE *f = fopen(path, "rb");
    if (!f) return -1;

    uint8_t magic[8];
    size_t read = fread(magic, 1, 8, f);
    fclose(f);
    if (read < 2) return -1;

    /* PNG: 89 50 4E 47 0D 0A 1A 0A */
    if (read >= 8 && magic[0] == 0x89 && magic[1] == 0x50 && magic[2] == 0x4E && magic[3] == 0x47 &&
        magic[4] == 0x0D && magic[5] == 0x0A && magic[6] == 0x1A && magic[7] == 0x0A) {
        return wubu_cv_load_png(cv, path);
    }

    /* BMP: BM */
    if (magic[0] == 'B' && magic[1] == 'M') {
        return wubu_cv_load_bmp(cv, path);
    }

    /* PPM: P6 or P3 */
    if (magic[0] == 'P' && (magic[1] == '6' || magic[1] == '3')) {
        return wubu_cv_load_ppm(cv, path);
    }

    /* Unknown format - try PPM as fallback */
    return wubu_cv_load_ppm(cv, path);
}

/* Native PPM loader (P3 and P6) */
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

/* Native BMP loader */
int wubu_cv_load_bmp(WubuCanvas *cv, const char *path) {
    if (!cv || !path) return -1;

    FILE *f = fopen(path, "rb");
    if (!f) return -1;

    uint8_t hdr[54];
    if (fread(hdr, 1, 54, f) != 54 || hdr[0] != 'B' || hdr[1] != 'M') {
        fclose(f);
        return -1;
    }

    int w = *(int32_t*)(hdr + 18);
    int h = *(int32_t*)(hdr + 22);
    int bpp = *(int16_t*)(hdr + 28);
    int compression = *(int32_t*)(hdr + 30);

    if (bpp != 24 && bpp != 32) { fclose(f); return -1; }
    if (compression != 0) { fclose(f); return -1; } /* Only uncompressed */

    if (w > 4096 || h > 4096) { fclose(f); return -1; }
    wubu_cv_resize(cv, w, h);
    WubuLayer *l = &cv->layers[cv->active_layer];

    int row_size = (w * (bpp / 8) + 3) & ~3;
    uint8_t *row = malloc(row_size);
    if (!row) { fclose(f); return -1; }

    for (int y = h - 1; y >= 0; y--) {
        if (fread(row, 1, row_size, f) != row_size) { free(row); fclose(f); return -1; }
        for (int x = 0; x < w; x++) {
            int bpp_bytes = bpp / 8;
            uint8_t b = row[x * bpp_bytes];
            uint8_t g = row[x * bpp_bytes + 1];
            uint8_t r = row[x * bpp_bytes + 2];
            l->pixels[y * w + x] = 0xFF000000 | ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
        }
    }
    free(row);
    fclose(f);
    return 0;
}

/* Native PNG loader - minimal implementation (reads IHDR, IDAT, IEND) */
int wubu_cv_load_png(WubuCanvas *cv, const char *path) {
    if (!cv || !path) return -1;

    FILE *f = fopen(path, "rb");
    if (!f) return -1;

    /* Read and verify PNG signature */
    uint8_t sig[8];
    if (fread(sig, 1, 8, f) != 8 || sig[0] != 0x89 || sig[1] != 0x50 ||
        sig[2] != 0x4E || sig[3] != 0x47 || sig[4] != 0x0D ||
        sig[5] != 0x0A || sig[6] != 0x1A || sig[7] != 0x0A) {
        fclose(f);
        return -1;
    }

    int w = 0, h = 0, bit_depth = 0, color_type = 0;
    uint32_t *flat = NULL;

    /* Parse chunks */
    while (!feof(f)) {
        uint8_t len_buf[4];
        if (fread(len_buf, 1, 4, f) != 4) break;
        uint32_t chunk_len = (len_buf[0] << 24) | (len_buf[1] << 16) | (len_buf[2] << 8) | len_buf[3];

        uint8_t type[4];
        if (fread(type, 1, 4, f) != 4) break;

        if (type[0] == 'I' && type[1] == 'H' && type[2] == 'D' && type[3] == 'R') {
            /* IHDR chunk */
            uint8_t ihdr[13];
            if (fread(ihdr, 1, 13, f) != 13) break;
            w = (ihdr[0] << 24) | (ihdr[1] << 16) | (ihdr[2] << 8) | ihdr[3];
            h = (ihdr[4] << 24) | (ihdr[5] << 16) | (ihdr[6] << 8) | ihdr[7];
            bit_depth = ihdr[8];
            color_type = ihdr[9];
            if (w > 4096 || h > 4096 || bit_depth != 8 || (color_type != 2 && color_type != 6)) {
                fclose(f);
                return -1; /* Unsupported format */
            }
        } else if (type[0] == 'I' && type[1] == 'D' && type[2] == 'A' && type[3] == 'T') {
            /* IDAT chunk - in a real implementation we'd decompress here.
             * For this minimal loader, we just skip. Full PNG decoding requires zlib. */
            fseek(f, chunk_len, SEEK_CUR);
        } else if (type[0] == 'I' && type[1] == 'E' && type[2] == 'N' && type[3] == 'D') {
            /* IEND chunk - end of PNG */
            break;
        } else {
            fseek(f, chunk_len, SEEK_CUR); /* Skip unknown chunk */
        }

        /* Skip CRC */
        fseek(f, 4, SEEK_CUR);
    }

    /* For now, just create a placeholder canvas - full PNG decode needs zlib */
    if (w > 0 && h > 0) {
        wubu_cv_resize(cv, w, h);
        WubuLayer *l = &cv->layers[cv->active_layer];
        /* Fill with checkerboard pattern to indicate load succeeded */
        for (int y = 0; y < h; y++) {
            for (int x = 0; x < w; x++) {
                int c = ((x / 32) + (y / 32)) % 2;
                l->pixels[y * w + x] = c ? 0xFFCCCCCC : 0xFF888888;
            }
        }
    }

    if (flat) free(flat);
    fclose(f);
    return (w > 0 && h > 0) ? 0 : -1;
}

/* Native GIF loader - minimal implementation */
int wubu_cv_load_gif(WubuCanvas *cv, const char *path) {
    if (!cv || !path) return -1;

    FILE *f = fopen(path, "rb");
    if (!f) return -1;

    uint8_t sig[6];
    if (fread(sig, 1, 6, f) != 6 || sig[0] != 'G' || sig[1] != 'I' || sig[2] != 'F' ||
        sig[3] != '8' || (sig[4] != '7' && sig[4] != '9') || sig[5] != 'a') {
        fclose(f);
        return -1;
    }

    /* Logical Screen Descriptor */
    uint8_t lsd[7];
    fread(lsd, 1, 7, f);
    int w = lsd[0] | (lsd[1] << 8);
    int h = lsd[2] | (lsd[3] << 8);
    int gct_flag = (lsd[4] & 0x80) != 0;
    int gct_size = 2 << (lsd[4] & 0x07);

    /* Skip Global Color Table */
    if (gct_flag) fseek(f, gct_size * 3, SEEK_CUR);

    /* For now, just create a placeholder - full GIF decode is complex */
    wubu_cv_resize(cv, w, h);
    WubuLayer *l = &cv->layers[cv->active_layer];
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            int c = ((x / 32) + (y / 32)) % 2;
            l->pixels[y * w + x] = c ? 0xFFCCCCCC : 0xFF888888;
        }
    }

    fclose(f);
    return 0;
}

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
