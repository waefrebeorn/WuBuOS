/*
 * gui_dbuf.h  --  WuBuOS Double-Buffered GUI Renderer
 *
 * Cell 101: Resolves P2-1 (GUI sluggishness).
 *
 * Provides flicker-free rendering by drawing everything to an
 * off-screen buffer, then flipping to the visible buffer in a
 * single vblank-synchronized operation.
 *
 * Design:
 *   - Two VBE buffers: front (visible) and back (drawing target)
 *   - All drawing primitives target the back buffer
 *   - gui_dbuf_flip() copies changed regions (dirty rects) to front
 *   - In hosted mode, we simulate with memcpy
 *   - In the real kernel, this would use等待 for vblank
 *
 * Also provides:
 *   - Dirty rectangle tracking (only copy changed regions)
 *   - Solid fill / blit / text rendering to back buffer
 *   - Win98-style 3D border drawing (raised/sunken)
 *
 * All C11, no external deps.
 */

#ifndef WUBU_GUI_DBUF_H
#define WUBU_GUI_DBUF_H

#include <stdint.h>
#include <stddef.h>

#define GUI_DBUF_DIRTY_MAX 32  /* Max dirty rectangles per frame */

/* -- Dirty Rectangle ---------------------------------------- */

typedef struct {
    int x, y, w, h;
} gui_rect_t;

/* -- Double Buffer State ------------------------------------ */

typedef struct {
    /* VBE back buffer (drawing target) */
    uint32_t    *back;
    int          width;
    int          height;
    int          bpp;

    /* Dirty rectangles for this frame */
    gui_rect_t   dirty[GUI_DBUF_DIRTY_MAX];
    int          dirty_count;

    /* Stats */
    uint64_t     frames;
    uint64_t     pixels_copied;
    uint64_t     flips;
} gui_dbuf_t;

/* -- Lifecycle ----------------------------------------------- */

/* Initialize double buffer. width/height from VBE mode. */
int  gui_dbuf_init(gui_dbuf_t *db, int width, int height);

/* Shutdown  --  free back buffer. */
void gui_dbuf_shutdown(gui_dbuf_t *db);

/* -- Drawing Primitives (all target back buffer) ------------- */

void gui_dbuf_clear(gui_dbuf_t *db, uint32_t color);
void gui_dbuf_pixel(gui_dbuf_t *db, int x, int y, uint32_t color);
void gui_dbuf_hline(gui_dbuf_t *db, int x1, int x2, int y, uint32_t color);
void gui_dbuf_vline(gui_dbuf_t *db, int x, int y1, int y2, uint32_t color);
void gui_dbuf_rect(gui_dbuf_t *db, int x, int y, int w, int h, uint32_t color);
void gui_dbuf_fill_rect(gui_dbuf_t *db, int x, int y, int w, int h, uint32_t color);

/* -- Win98-Style 3D Borders ---------------------------------- */

/* Draw a raised 3D border (button up, window frame) */
void gui_dbuf_border_raised(gui_dbuf_t *db, int x, int y, int w, int h);

/* Draw a sunken 3D border (button down, input field) */
void gui_dbuf_border_sunken(gui_dbuf_t *db, int x, int y, int w, int h);

/* Draw a Win98 button (raised or sunken based on 'pressed') */
void gui_dbuf_button(gui_dbuf_t *db, int x, int y, int w, int h,
                     const char *label, int pressed);

/* Draw a Win98 window frame with title bar */
void gui_dbuf_window(gui_dbuf_t *db, int x, int y, int w, int h,
                     const char *title, int active);

/* -- Dirty Rectangle Tracking -------------------------------- */

/* Mark a region as dirty (needs flip) */
void gui_dbuf_mark_dirty(gui_dbuf_t *db, int x, int y, int w, int h);

/* Clear dirty rects (call after flip) */
void gui_dbuf_clear_dirty(gui_dbuf_t *db);

/* -- Flip ---------------------------------------------------- */

/* Flip back buffer to front (VBE).
 * Copies only dirty rectangles for efficiency.
 * Returns number of pixels copied. */
uint32_t gui_dbuf_flip(gui_dbuf_t *db);

/* -- Query --------------------------------------------------- */

int          gui_dbuf_width(const gui_dbuf_t *db);
int          gui_dbuf_height(const gui_dbuf_t *db);
uint64_t     gui_dbuf_frames(const gui_dbuf_t *db);
int          gui_dbuf_dirty_count(const gui_dbuf_t *db);
/* Draw a character using the 8x8 font */
void gui_dbuf_draw_char(gui_dbuf_t *db, int x, int y, char c, uint32_t color);

uint32_t     gui_dbuf_get_pixel(const gui_dbuf_t *db, int x, int y);

#endif /* WUBU_GUI_DBUF_H */
