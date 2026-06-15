/*
 * wm.h  --  My Seed Window Manager (simplified NanoShell)
 */
#ifndef MYSEED_WM_H
#define MYSEED_WM_H
#include <stdint.h>
#include <stddef.h>
#include "wubu_theme.h"

#define WM_MAX_WINDOWS 64
#define WM_TITLE_HEIGHT 20
#define WM_BORDER_WIDTH 3

typedef enum {
    WIN_UNUSED  = 0,
    WIN_VISIBLE = 1,
    WIN_MINIMIZED = 2,
    WIN_MAXIMIZED = 4,
    WIN_FOCUSED = 8,
    WIN_NOCLOSE = 16,
} WinFlags;

typedef struct WmWindow WmWindow;
struct WmWindow {
    int       id;
    WinFlags  flags;
    int       x, y, w, h;        /* Position and size */
    int       min_x, min_y, min_w, min_h; /* Saved before maximize */
    int       z_order;            /* Higher = on top */
    char      title[64];
    uint32_t  title_color;        /* Active/inactive */
    
    /* Callbacks */
    void     (*on_draw)(WmWindow *win, void *fb, int fb_w, int fb_h);
    void     (*on_key)(WmWindow *win, uint32_t key, uint32_t mods);
    void     (*on_mouse)(WmWindow *win, int x, int y, int btn, int kind);
    void     (*on_close)(WmWindow *win);
    void     (*on_resize)(WmWindow *win, int w, int h);
    
    void     *user_data;
    int       user_data_size;
};

/* -- WM Lifecycle ------------------------------------------------ */

int  wm_init(int screen_w, int screen_h);
void wm_shutdown(void);

/* -- Window Management ------------------------------------------- */

WmWindow *wm_create_window(int x, int y, int w, int h, const char *title);
void      wm_destroy_window(WmWindow *win);
void      wm_set_focus(WmWindow *win);
WmWindow *wm_get_focused(void);
WmWindow *wm_find_by_id(int id);

/* -- Rendering --------------------------------------------------- */

/* Render all windows to framebuffer */
void wm_render(uint32_t *fb, int fb_w, int fb_h);

/* Mark a window as needing redraw */
void wm_invalidate(WmWindow *win);

/* Invalidate entire screen */
void wm_invalidate_all(void);

/* -- Input Routing ----------------------------------------------- */

void wm_handle_key(uint32_t key, uint32_t mods);
void wm_handle_mouse(int x, int y, int btn, int kind);

/* -- Query ------------------------------------------------------- */

int wm_window_count(void);

#endif
