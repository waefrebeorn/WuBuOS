/*
 * paint.h  --  WuBuOS Paint (Photoshop-style image editor)
 * Type surface exposed for tests / inspection (C11, minimal includes)
 */
#ifndef MYSEED_PAINT_H
#define MYSEED_PAINT_H

#include <stdint.h>
#include <stdbool.h>

typedef struct DosGuiWindow DosGuiWindow;

/* Tool types. The implementation (paint.c) uses TOOL_* names; we expose
 * both TOOL_* (internal) and PAINT_TOOL_* (public/test) spellings. */
typedef enum {
    TOOL_BRUSH = 0,
    TOOL_FILL,
    TOOL_LINE,
    TOOL_RECT,
    TOOL_ELLIPSE,
    TOOL_PICKER,
    TOOL_ERASER,
    TOOL_COUNT
} PaintTool;

#define PAINT_TOOL_BRUSH   TOOL_BRUSH
#define PAINT_TOOL_FILL    TOOL_FILL
#define PAINT_TOOL_LINE    TOOL_LINE
#define PAINT_TOOL_RECT    TOOL_RECT
#define PAINT_TOOL_ELLIPSE TOOL_ELLIPSE
#define PAINT_TOOL_PICKER  TOOL_PICKER
#define PAINT_TOOL_ERASER  TOOL_ERASER

#define PAINT_MAX_SHAPES 256

/* A recorded vector shape */
typedef struct {
    int x, y, w, h;
    uint32_t color;
    bool fill;
} PaintShape;

/* Paint state (fields exposed for tests / inspection) */
typedef struct {
    uint32_t canvas[512 * 384];
    uint32_t undo_buf[512 * 384];
    int undo_valid;

    PaintTool current_tool;
    int brush_size;
    uint32_t fg_color;
    uint32_t bg_color;
    int drawing;
    int start_x, start_y;
    int last_x, last_y;

    /* Window position */
    int win_x, win_y;

    /* Vector shape list (test-inspected) */
    PaintShape shapes[PAINT_MAX_SHAPES];
    int shape_count;
    int undo_shape_count;
} PaintState;

/* Instance API (so tests can create/destroy per-instance state) */
PaintState* paint_create(void);
void paint_destroy(PaintState *p);

void paint_set_tool(PaintState *p, PaintTool tool);
void paint_add_shape(PaintState *p, int x, int y, int w, int h, uint32_t color, bool fill);
void paint_save_undo(PaintState *p);
void paint_undo(PaintState *p);

void paint_init(void);
void paint_open(void);
void paint_update(void);
void paint_shutdown(void);

DosGuiWindow* paint_launch(void);

#endif
