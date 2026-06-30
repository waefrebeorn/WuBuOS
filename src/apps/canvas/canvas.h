/*
 * canvas.h  --  WuBu Canvas Wrapper
 * Opaque struct, C11, minimal includes, self-contained
 */

#ifndef WUBU_CANVAS_H
#define WUBU_CANVAS_H

#include <stdint.h>

typedef struct DosGuiWindow DosGuiWindow;

typedef struct CanvasState CanvasState;

CanvasState* canvas_create(void);
void canvas_destroy(CanvasState *cv);

void canvas_draw(DosGuiWindow *win, uint32_t *fb, int fb_w, int fb_h, CanvasState *cv);
DosGuiWindow* canvas_launch(void);

#endif