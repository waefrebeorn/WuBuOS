/*
 * canvas.c  --  WuBu Canvas Wrapper - minimal stub
 */

#include "canvas.h"
#include "../gui/dosgui_wm.h"
#include "../kernel/vbe.h"
#include "../gui/wubu_theme.h"
#include <stdlib.h>

struct CanvasState { int dummy; };

CanvasState* canvas_create(void) { return calloc(1, sizeof(CanvasState)); }
void canvas_destroy(CanvasState *cv) { free(cv); }

void canvas_draw(DosGuiWindow *win, uint32_t *fb, int fb_w, int fb_h, CanvasState *cv) {
    (void)win; (void)fb; (void)fb_w; (void)fb_h; (void)cv;
}

DosGuiWindow* canvas_launch(void) {
    return dosgui_wm_create(80, 60, 700, 500, "WuBu Canvas");
}