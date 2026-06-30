/*
 * paint.h  --  MS Paint Style Editor (brush, line, rect, ellipse, fill, text, eyedropper, eraser)
 * Opaque struct, C11, minimal includes, self-contained
 */

#ifndef WUBU_PAINT_H
#define WUBU_PAINT_H

#include <stdint.h>
#include <stdbool.h>

typedef struct DosGuiWindow DosGuiWindow;

/* Tools */
typedef enum {
    PAINT_TOOL_BRUSH = 0,
    PAINT_TOOL_LINE,
    PAINT_TOOL_RECT,
    PAINT_TOOL_ELLIPSE,
    PAINT_TOOL_FILL,
    PAINT_TOOL_TEXT,
    PAINT_TOOL_EYEDROPPER,
    PAINT_TOOL_ERASER,
    PAINT_TOOL_COUNT
} PaintTool;

/* Opaque state */
typedef struct PaintState PaintState;

/* API */
PaintState* paint_create(void);
void paint_destroy(PaintState *paint);

void paint_draw(DosGuiWindow *win, uint32_t *fb, int fb_w, int fb_h, PaintState *paint);
DosGuiWindow* paint_launch(void);

/* Shape management */
void paint_add_shape(PaintState *paint, int x, int y, int w, int h, uint32_t color, bool filled);
void paint_save_undo(PaintState *paint);
void paint_undo(PaintState *paint);

/* Tool/color */
void paint_set_tool(PaintState *paint, PaintTool tool);
void paint_set_fg_color(PaintState *paint, uint32_t color);
void paint_set_bg_color(PaintState *paint, uint32_t color);
void paint_set_brush_size(PaintState *paint, int size);
void paint_toggle_grid(PaintState *paint);

#endif