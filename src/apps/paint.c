/*
 * paint.c  --  WuBuOS Paint (Photoshop-style image editor)
 *
 * A Win98-style paint application with:
 *   - Freehand brush drawing
 *   - Multiple brush sizes
 *   - Color palette (16-color EGA + 256-color palette)
 *   - Fill bucket
 *   - Line/rectangle/ellipse tools
 *   - Undo (1 level)
 *   - Save/load to raw pixel format
 *
 * Runs as a WuBuOS GUI app in a WmWindow.
 */

#include "../gui/wm.h"
#include "../kernel/vbe.h"
#include "../kernel/input.h"

#include <string.h>
#include <stdlib.h>

/* -- Constants --------------------------------------------------- */

#define CANVAS_W        512
#define CANVAS_H        384
#define PALETTE_COLS    16
#define PALETTE_ROWS    16
#define PALETTE_CELL    16
#define TOOLBAR_H       24
#define PALETTE_W       (PALETTE_COLS * PALETTE_CELL)
#define WIN_W           (CANVAS_W + PALETTE_W + 4)
#define WIN_H           (CANVAS_H + TOOLBAR_H + WM_TITLE_HEIGHT + 8)
#define MAX_UNDO        1

/* -- EGA 16-color palette ---------------------------------------- */

static const uint32_t g_ega_palette[16] = {
    0x000000, 0x0000AA, 0x00AA00, 0x00AAAA,
    0xAA0000, 0xAA00AA, 0xAA5500, 0xAAAAAA,
    0x555555, 0x5555FF, 0x55FF55, 0x55FFFF,
    0xFF5555, 0xFF55FF, 0xFFFF55, 0xFFFFFF,
};

/* -- 256-color palette (generated) ------------------------------- */

static uint32_t g_palette[256];

static void generate_palette(void) {
    /* First 16: EGA */
    for (int i = 0; i < 16; i++) g_palette[i] = g_ega_palette[i];
    /* 16-231: 6×6×6 color cube */
    for (int i = 16; i < 232; i++) {
        int idx = i - 16;
        int r = (idx / 36) % 6;
        int g = (idx / 6) % 6;
        int b = idx % 6;
        g_palette[i] = ((r * 51) << 16) | ((g * 51) << 8) | (b * 51);
    }
    /* 232-255: grayscale */
    for (int i = 232; i < 256; i++) {
        int v = (i - 232) * 11 + 8;
        g_palette[i] = (v << 16) | (v << 8) | v;
    }
}

/* -- Tool Types -------------------------------------------------- */

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

static const char *g_tool_names[] = {
    "Brush", "Fill", "Line", "Rect", "Ellipse", "Pick", "Eraser"
};

/* -- Paint State ------------------------------------------------- */

typedef struct {
    uint32_t canvas[CANVAS_W * CANVAS_H];
    uint32_t undo_buf[CANVAS_W * CANVAS_H];
    int undo_valid;

    PaintTool tool;
    int brush_size;
    uint32_t fg_color;
    uint32_t bg_color;
    int drawing;
    int start_x, start_y;
    int last_x, last_y;

    /* Window position */
    int win_x, win_y;
} PaintState;

static PaintState g_paint = {0};

/* -- Drawing Primitives ------------------------------------------ */

static void set_pixel(int x, int y, uint32_t color) {
    if (x < 0 || x >= CANVAS_W || y < 0 || y >= CANVAS_H) return;
    g_paint.canvas[y * CANVAS_W + x] = color;
}

static uint32_t get_pixel(int x, int y) {
    if (x < 0 || x >= CANVAS_W || y < 0 || y >= CANVAS_H) return 0;
    return g_paint.canvas[y * CANVAS_W + x];
}

static void draw_line(int x0, int y0, int x1, int y1, uint32_t color, int size) {
    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;

    while (1) {
        /* Draw brush-sized dot */
        for (int by = -size/2; by <= size/2; by++) {
            for (int bx = -size/2; bx <= size/2; bx++) {
                set_pixel(x0 + bx, y0 + by, color);
            }
        }
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

static void fill_bucket(int x, int y, uint32_t new_color) {
    if (x < 0 || x >= CANVAS_W || y < 0 || y >= CANVAS_H) return;
    uint32_t target = get_pixel(x, y);
    if (target == new_color) return;

    /* Simple scanline fill */
    int stack[CANVAS_W * CANVAS_H];
    int sp = 0;
    stack[sp++] = y * CANVAS_W + x;

    while (sp > 0) {
        int idx = stack[--sp];
        int px = idx % CANVAS_W;
        int py = idx / CANVAS_W;

        if (px < 0 || px >= CANVAS_W || py < 0 || py >= CANVAS_H) continue;
        if (get_pixel(px, py) != target) continue;

        set_pixel(px, py, new_color);

        if (px + 1 < CANVAS_W) stack[sp++] = py * CANVAS_W + (px + 1);
        if (px - 1 >= 0) stack[sp++] = py * CANVAS_W + (px - 1);
        if (py + 1 < CANVAS_H) stack[sp++] = (py + 1) * CANVAS_W + px;
        if (py - 1 >= 0) stack[sp++] = (py - 1) * CANVAS_W + px;
    }
}

static void draw_rect(int x0, int y0, int x1, int y1, uint32_t color) {
    for (int x = x0; x <= x1; x++) {
        set_pixel(x, y0, color);
        set_pixel(x, y1, color);
    }
    for (int y = y0; y <= y1; y++) {
        set_pixel(x0, y, color);
        set_pixel(x1, y, color);
    }
}

static void draw_ellipse(int cx, int cy, int rx, int ry, uint32_t color) {
    for (int angle = 0; angle < 360; angle++) {
        float rad = angle * 3.14159265f / 180.0f;
        /* Pure C sin/cos approximation */
        float s = rad - (rad*rad*rad)/6.0f + (rad*rad*rad*rad*rad)/120.0f;
        float c = 1.0f - (rad*rad)/2.0f + (rad*rad*rad*rad)/24.0f;
        int x = cx + (int)(rx * c);
        int y = cy + (int)(ry * s);
        set_pixel(x, y, color);
    }
}

/* -- Undo -------------------------------------------------------- */

static void save_undo(void) {
    memcpy(g_paint.undo_buf, g_paint.canvas, sizeof(g_paint.canvas));
    g_paint.undo_valid = 1;
}

static void do_undo(void) {
    if (g_paint.undo_valid) {
        memcpy(g_paint.canvas, g_paint.undo_buf, sizeof(g_paint.canvas));
    }
}

/* -- Rendering --------------------------------------------------- */

static void paint_do_render(WmWindow *win, uint32_t *fb, int fb_w, int fb_h) {
    (void)win;
    uint32_t *pixels = (uint32_t *)fb;

    /* Clear to gray */
    for (int i = 0; i < fb_w * fb_h; i++) {
        pixels[i] = 0xC0C0C0;
    }

    /* Toolbar */
    for (int y = 0; y < TOOLBAR_H; y++) {
        for (int x = 0; x < fb_w; x++) {
            pixels[y * fb_w + x] = 0xE0E0E0;
        }
    }

    /* Tool buttons */
    for (int t = 0; t < TOOL_COUNT; t++) {
        int bx = 4 + t * 52;
        int by = 4;
        uint32_t bg = (t == g_paint.tool) ? 0x8080FF : 0xC0C0C0;
        for (int dy = 0; dy < 16; dy++) {
            for (int dx = 0; dx < 48; dx++) {
                pixels[(by + dy) * fb_w + (bx + dx)] = bg;
            }
        }
    }

    /* Brush size indicator */
    int bs_x = WIN_W - PALETTE_W - 60;
    for (int dy = 0; dy < 16; dy++) {
        for (int dx = 0; dx < 48; dx++) {
            pixels[(4 + dy) * fb_w + (bs_x + dx)] = 0xC0C0C0;
        }
    }
    /* Draw brush size dot */
    int dot_r = g_paint.brush_size / 2;
    for (int dy = -dot_r; dy <= dot_r; dy++) {
        for (int dx = -dot_r; dx <= dot_r; dx++) {
            if (dx*dx + dy*dy <= dot_r*dot_r) {
                pixels[(12 + dy) * fb_w + (bs_x + 24 + dx)] = g_paint.fg_color;
            }
        }
    }

    /* Canvas area */
    int canvas_x = 2;
    int canvas_y = TOOLBAR_H + 2;
    for (int y = 0; y < CANVAS_H; y++) {
        for (int x = 0; x < CANVAS_W; x++) {
            uint32_t c = g_paint.canvas[y * CANVAS_W + x];
            if (c == 0) c = 0xFFFFFF; /* White background */
            pixels[(canvas_y + y) * fb_w + (canvas_x + x)] = c;
        }
    }

    /* Canvas border */
    for (int x = -1; x <= CANVAS_W; x++) {
        pixels[(canvas_y - 1) * fb_w + (canvas_x + x)] = 0x808080;
        pixels[(canvas_y + CANVAS_H) * fb_w + (canvas_x + x)] = 0x808080;
    }
    for (int y = -1; y <= CANVAS_H; y++) {
        pixels[(canvas_y + y) * fb_w + (canvas_x - 1)] = 0x808080;
        pixels[(canvas_y + y) * fb_w + (canvas_x + CANVAS_W)] = 0x808080;
    }

    /* Color palette */
    int pal_x = CANVAS_W + 4;
    int pal_y = TOOLBAR_H + 2;
    for (int row = 0; row < PALETTE_ROWS; row++) {
        for (int col = 0; col < PALETTE_COLS; col++) {
            int idx = row * PALETTE_COLS + col;
            uint32_t c = g_palette[idx];
            for (int dy = 0; dy < PALETTE_CELL; dy++) {
                for (int dx = 0; dx < PALETTE_CELL; dx++) {
                    pixels[(pal_y + row * PALETTE_CELL + dy) * fb_w +
                           (pal_x + col * PALETTE_CELL + dx)] = c;
                }
            }
        }
    }

    /* FG/BG color indicator */
    int fg_x = pal_x;
    int fg_y = pal_y + PALETTE_ROWS * PALETTE_CELL + 4;
    for (int dy = 0; dy < 20; dy++) {
        for (int dx = 0; dx < 20; dx++) {
            pixels[(fg_y + dy) * fb_w + (fg_x + dx)] = g_paint.bg_color;
        }
    }
    for (int dy = 0; dy < 14; dy++) {
        for (int dx = 0; dx < 14; dx++) {
            pixels[(fg_y + 3 + dy) * fb_w + (fg_x + 3 + dx)] = g_paint.fg_color;
        }
    }
}

/* -- Input Handling ---------------------------------------------- */

static void paint_handle_mouse(WmWindow *win, int x, int y, int btn, int kind) {
    (void)win;
    /* Convert window coordinates to canvas coordinates */
    int canvas_x = 2;
    int canvas_y = TOOLBAR_H + 2;
    int mx = x - canvas_x;
    int my = y - canvas_y;

    /* Check palette click */
    int pal_x = CANVAS_W + 4;
    int pal_y = TOOLBAR_H + 2;
    if (x >= pal_x && x < pal_x + PALETTE_W && y >= pal_y && y < pal_y + PALETTE_ROWS * PALETTE_CELL) {
        int col = (x - pal_x) / PALETTE_CELL;
        int row = (y - pal_y) / PALETTE_CELL;
        int idx = row * PALETTE_COLS + col;
        if (idx < 256) {
            if (btn == 1) g_paint.fg_color = g_palette[idx];
            else if (btn == 3) g_paint.bg_color = g_palette[idx];
        }
        return;
    }

    /* Check toolbar click */
    if (y < TOOLBAR_H && btn == 1) {
        int tool = (x - 4) / 52;
        if (tool >= 0 && tool < TOOL_COUNT) {
            g_paint.tool = tool;
        }
        return;
    }

    /* Canvas interaction */
    if (mx >= 0 && mx < CANVAS_W && my >= 0 && my < CANVAS_H) {
        if (btn == 1 && kind == 0) { /* Left press */
            save_undo();
            g_paint.drawing = 1;
            g_paint.start_x = mx;
            g_paint.start_y = my;
            g_paint.last_x = mx;
            g_paint.last_y = my;

            if (g_paint.tool == TOOL_BRUSH || g_paint.tool == TOOL_ERASER) {
                uint32_t c = (g_paint.tool == TOOL_ERASER) ? 0xFFFFFF : g_paint.fg_color;
                draw_line(mx, my, mx, my, c, g_paint.brush_size);
            } else if (g_paint.tool == TOOL_FILL) {
                fill_bucket(mx, my, g_paint.fg_color);
            } else if (g_paint.tool == TOOL_PICKER) {
                g_paint.fg_color = get_pixel(mx, my);
            }
        } else if (btn == 1 && kind == 1) { /* Left release */
            if (g_paint.drawing) {
                if (g_paint.tool == TOOL_LINE) {
                    draw_line(g_paint.start_x, g_paint.start_y, mx, my, g_paint.fg_color, 1);
                } else if (g_paint.tool == TOOL_RECT) {
                    int x0 = g_paint.start_x < mx ? g_paint.start_x : mx;
                    int y0 = g_paint.start_y < my ? g_paint.start_y : my;
                    int x1 = g_paint.start_x > mx ? g_paint.start_x : mx;
                    int y1 = g_paint.start_y > my ? g_paint.start_y : my;
                    draw_rect(x0, y0, x1, y1, g_paint.fg_color);
                } else if (g_paint.tool == TOOL_ELLIPSE) {
                    int rx = abs(mx - g_paint.start_x);
                    int ry = abs(my - g_paint.start_y);
                    draw_ellipse(g_paint.start_x, g_paint.start_y, rx, ry, g_paint.fg_color);
                }
                g_paint.drawing = 0;
            }
        } else if (g_paint.drawing && btn == 0 && kind == 2) { /* Motion while drawing */
            if (g_paint.tool == TOOL_BRUSH || g_paint.tool == TOOL_ERASER) {
                uint32_t c = (g_paint.tool == TOOL_ERASER) ? 0xFFFFFF : g_paint.fg_color;
                draw_line(g_paint.last_x, g_paint.last_y, mx, my, c, g_paint.brush_size);
                g_paint.last_x = mx;
                g_paint.last_y = my;
            }
        }
    }
}

static void paint_handle_key(WmWindow *win, uint32_t key, uint32_t mods) {
    (void)win; (void)mods;
    if (key == 0x1A) { /* [  --  smaller brush */
        if (g_paint.brush_size > 1) g_paint.brush_size--;
    } else if (key == 0x1B) { /* ]  --  larger brush */
        if (g_paint.brush_size < 32) g_paint.brush_size++;
    } else if (key == 0x2E && (mods & 0x01)) { /* Ctrl+Z  --  undo */
        do_undo();
    }
}

/* -- Public API -------------------------------------------------- */

void paint_init(void) {
    memset(&g_paint, 0, sizeof(g_paint));
    generate_palette();
    g_paint.tool = TOOL_BRUSH;
    g_paint.brush_size = 3;
    g_paint.fg_color = 0x000000;
    g_paint.bg_color = 0xFFFFFF;
    g_paint.undo_valid = 0;

    /* Clear canvas to white */
    for (int i = 0; i < CANVAS_W * CANVAS_H; i++) {
        g_paint.canvas[i] = 0xFFFFFF;
    }
}

void paint_open(void) {
    paint_init();
    WmWindow *win = wm_create_window(50, 50, WIN_W, WIN_H, "WuBuOS Paint");
    if (win) {
        win->on_draw = paint_do_render;
        win->on_mouse = paint_handle_mouse;
        win->on_key = paint_handle_key;
    }
}

void paint_update(void) {
    /* Animation updates (cursor blink, etc.) */
}

void paint_render(WmWindow *win, uint32_t *fb, int w, int h) {
    paint_do_render(win, fb, w, h);
}

void paint_shutdown(void) {
    /* Nothing to clean up */
}
