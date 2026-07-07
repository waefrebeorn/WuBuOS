/*
 * wm.c  --  My Seed WmWindow Manager Implementation (LEGACY - being phased out)
 */
#include "wm.h"
#include "../kernel/vbe_legacy.h"
#include <string.h>
#include <stdio.h>

/* Legacy Win98 color constants (local to legacy WM) */
#define C_WIN_FACE      0x00C0C0C0
#define C_WIN_TITLE     0x00000080
#define C_WIN_TITLE_INA 0x00808080
#define C_WIN_BORDER_DK 0x00808080

static WmWindow g_windows[WM_MAX_WINDOWS];
static int    g_next_id = 1;
static int    g_screen_w = 0, g_screen_h = 0;
static int    g_focused_id = -1;

int wm_init(int screen_w, int screen_h) {
    g_screen_w = screen_w;
    g_screen_h = screen_h;
    memset(g_windows, 0, sizeof(g_windows));
    return 0;
}

void wm_shutdown(void) {
    for (int i = 0; i < WM_MAX_WINDOWS; i++)
        if (g_windows[i].flags != WIN_UNUSED)
            wm_destroy_window(&g_windows[i]);
}

WmWindow *wm_create_window(int x, int y, int w, int h, const char *title) {
    WmWindow *win = NULL;
    for (int i = 0; i < WM_MAX_WINDOWS; i++) {
        if (g_windows[i].flags == WIN_UNUSED) { win = &g_windows[i]; break; }
    }
    if (!win) return NULL;
    
    memset(win, 0, sizeof(WmWindow));
    win->id = g_next_id++;
    win->flags = WIN_VISIBLE;
    win->x = x; win->y = y; win->w = w; win->h = h;
    win->z_order = win->id;
    win->title_color = 0x00000080; /* Navy active */
    strncpy(win->title, title ? title : "WmWindow", sizeof(win->title)-1);
    
    wm_set_focus(win);
    return win;
}

void wm_destroy_window(WmWindow *win) {
    if (!win) return;
    if (win->on_close) win->on_close(win);
    win->flags = WIN_UNUSED;
}

void wm_set_focus(WmWindow *win) {
    if (!win) return;
    /* Unfocus old */
    for (int i = 0; i < WM_MAX_WINDOWS; i++) {
        g_windows[i].flags &= ~WIN_FOCUSED;
        if (g_windows[i].flags != WIN_UNUSED)
            g_windows[i].title_color = 0x00808080; /* Inactive gray */
    }
    win->flags |= WIN_FOCUSED;
    win->title_color = 0x00000080; /* Active navy */
    win->z_order = 10000 + win->id; /* Bring to front */
    g_focused_id = win->id;
}

WmWindow *wm_get_focused(void) {
    return wm_find_by_id(g_focused_id);
}

WmWindow *wm_find_by_id(int id) {
    for (int i = 0; i < WM_MAX_WINDOWS; i++)
        if (g_windows[i].id == id) return &g_windows[i];
    return NULL;
}

/* -- Rendering --------------------------------------------------- */

static void draw_window_chrome(WmWindow *win) {
    int bw = WM_BORDER_WIDTH;
    int th = WM_TITLE_HEIGHT;
    
    /* 3D raised border */
    vbe_3d_raised(win->x, win->y, win->w, win->h);
    
    /* WmWindow body */
    vbe_fill_rect(win->x+bw, win->y+bw, win->w-2*bw, win->h-2*bw, C_WIN_FACE);
    
    /* Title bar */
    vbe_fill_rect(win->x+bw, win->y+bw, win->w-2*bw, th, win->title_color);
    
    /* Close button (16x14 at top-right) */
    int cbx = win->x + win->w - bw - 18;
    int cby = win->y + bw + 3;
    vbe_fill_rect(cbx, cby, 16, 14, C_WIN_FACE);
    vbe_3d_raised(cbx, cby, 16, 14);
    
    /* Separator below title */
    vbe_hline(win->x+bw, win->x+win->w-bw-1, win->y+bw+th, C_WIN_BORDER_DK);
    
    /* Client area sunken border */
    vbe_3d_sunken(win->x+bw+1, win->y+bw+th+2,
                  win->w-2*bw-2, win->h-bw-th-3);
}

void wm_render(uint32_t *fb, int fb_w, int fb_h) {
    (void)fb; (void)fb_w; (void)fb_h;
    /* Sort by z_order and draw visible windows */
    for (int z = 0; z < 20000; z++) {
        for (int i = 0; i < WM_MAX_WINDOWS; i++) {
            WmWindow *w = &g_windows[i];
            if (w->flags == WIN_UNUSED || !(w->flags & WIN_VISIBLE)) continue;
            if (w->z_order != z) continue;
            draw_window_chrome(w);
            if (w->on_draw) w->on_draw(w, NULL, 0, 0);
        }
    }
}

void wm_invalidate(WmWindow *win) { (void)win; }
void wm_invalidate_all(void) { }

/* -- Input ------------------------------------------------------- */

void wm_handle_key(uint32_t key, uint32_t mods) {
    WmWindow *focused = wm_get_focused();
    if (focused && focused->on_key) focused->on_key(focused, key, mods);
}

void wm_handle_mouse(int x, int y, int btn, int kind) {
    /* Find topmost window under cursor */
    WmWindow *hit = NULL;
    int best_z = -1;
    for (int i = 0; i < WM_MAX_WINDOWS; i++) {
        WmWindow *w = &g_windows[i];
        if (w->flags == WIN_UNUSED || !(w->flags & WIN_VISIBLE)) continue;
        if (x >= w->x && x < w->x+w->w && y >= w->y && y < w->y+w->h) {
            if (w->z_order > best_z) { hit = w; best_z = w->z_order; }
        }
    }
    if (hit) {
        wm_set_focus(hit);
        if (hit->on_mouse) hit->on_mouse(hit, x-hit->x, y-hit->y, btn, kind);
    }
}

int wm_window_count(void) {
    int n = 0;
    for (int i = 0; i < WM_MAX_WINDOWS; i++)
        if (g_windows[i].flags != WIN_UNUSED) n++;
    return n;
}
