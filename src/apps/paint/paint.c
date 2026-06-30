/*
 * paint.c  --  MS Paint Style Editor (minimal stub)
 */

#include "paint.h"
#include "../gui/dosgui_wm.h"
#include "../kernel/vbe.h"
#include "../gui/wubu_theme.h"
#include <stdlib.h>

#define PAINT_MAX_SHAPES 1000
#define PAINT_MAX_UNDO 50

typedef struct {
    int x, y, w, h;
    uint32_t color;
    bool filled;
} PaintShape;

struct PaintState {
    PaintShape shapes[PAINT_MAX_SHAPES];
    int shape_count;
    int current_tool;
    uint32_t fg_color, bg_color;
    int brush_size;
    int undo_shapes[PAINT_MAX_UNDO];
    int undo_count;
    int undo_ptr;
};

PaintState* paint_create(void) {
    PaintState *p = calloc(1, sizeof(PaintState));
    if (p) {
        p->fg_color = 0x00000000;
        p->bg_color = 0x00FFFFFF;
        p->brush_size = 3;
    }
    return p;
}

void paint_destroy(PaintState *p) {
    free(p);
}

void paint_draw(DosGuiWindow *win, uint32_t *fb, int fb_w, int fb_h, PaintState *p) {
    (void)win; (void)fb; (void)fb_w; (void)fb_h; (void)p;
}

DosGuiWindow* paint_launch(void) {
    return dosgui_wm_create(80, 60, 700, 500, "Paint");
}

void paint_add_shape(PaintState *p, int x, int y, int w, int h, uint32_t color, bool filled) {
    if (p->shape_count >= PAINT_MAX_SHAPES) return;
    p->shapes[p->shape_count++] = (PaintShape){x, y, w, h, color, filled};
}

void paint_save_undo(PaintState *p) {
    if (p->undo_count < PAINT_MAX_UNDO) {
        p->undo_shapes[p->undo_ptr] = p->shape_count;
        p->undo_ptr = (p->undo_ptr + 1) % PAINT_MAX_UNDO;
        p->undo_count++;
    } else {
        p->undo_shapes[p->undo_ptr] = p->shape_count;
        p->undo_ptr = (p->undo_ptr + 1) % PAINT_MAX_UNDO;
    }
}

void paint_undo(PaintState *p) {
    if (p->undo_count > 0) {
        p->undo_ptr = (p->undo_ptr - 1 + PAINT_MAX_UNDO) % PAINT_MAX_UNDO;
        p->shape_count = p->undo_shapes[p->undo_ptr];
        p->undo_count--;
    }
}

void paint_set_tool(PaintState *p, PaintTool tool) { p->current_tool = tool; }
void paint_set_fg_color(PaintState *p, uint32_t c) { p->fg_color = c; }
void paint_set_bg_color(PaintState *p, uint32_t c) { p->bg_color = c; }
void paint_set_brush_size(PaintState *p, int s) { p->brush_size = s; }
void paint_toggle_grid(PaintState *p) { (void)p; }