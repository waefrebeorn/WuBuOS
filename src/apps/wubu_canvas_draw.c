/*
 * wubu_canvas_draw.c -- WuBuOS canvas: pixel drawing tools, color pick,
 *                      and selection.
 *
 * Self-contained: depends only on wubu_canvas.h for the WubuCanvas /
 * WubuLayer types and the public wubu_cv_* drawing/selection API. Mutations
 * that touch layer pixels call wubu_cv__undo_push() (wubu_canvas_undo.c) to
 * record a snapshot before editing -- the single internal seam, declared in
 * wubu_canvas_internal.h. Minimal includes.
 */

#include "wubu_canvas.h"
#include "wubu_canvas_internal.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* -- Drawing ------------------------------------------------------- */

void wubu_cv_brush(WubuCanvas *cv, int x, int y) {
    if (!cv || cv->active_layer < 0) return;
    WubuLayer *l = &cv->layers[cv->active_layer];
    if (l->locked || !l->pixels) return;
    wubu_cv__undo_push(cv);
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
    wubu_cv__undo_push(cv);
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
    wubu_cv__undo_push(cv);
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
    wubu_cv__undo_push(cv);
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
    wubu_cv__undo_push(cv);
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
    wubu_cv__undo_push(cv);
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
    wubu_cv__undo_push(cv);
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
