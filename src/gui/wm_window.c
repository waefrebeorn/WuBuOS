/*
 * wm_window.c -- WuBuOS DosGui WM: window lifecycle + virtual desktops
 *
 * Self-contained concern split out of wm.c (the WM facade):
 *   - Window lifecycle: spawn / close / raise / destroy / focus / lookup
 *   - Z-order + focus management
 *   - Virtual-desktop migration (Ctrl+Alt+Left/Right, Win+Shift+arrows)
 *   - Wallpaper state + reload (the icon registry moved to wm_icon_registry.c)
 *
 * Depends only on the shared WM state (wm_internal.h) and the settings
 * public API. No rendering, no input dispatch, no theme engine wiring.
 */

#include "wm_internal.h"
#include "wubu_settings.h"

#include <string.h>
#include <stdio.h>

/* -- Z-order + window lifecycle ----------------------------------- */

void raise_win(int i) {
    int j = 0;
    while (j < g_dwm.nz && g_dwm.zorder[j] != i) j++;
    if (j == g_dwm.nz) return;
    for (; j < g_dwm.nz - 1; j++)
        g_dwm.zorder[j] = g_dwm.zorder[j + 1];
    g_dwm.zorder[g_dwm.nz - 1] = i;
}

int spawn_window(int x, int y, int w, int h, const char *title) {
    DosGuiWindow *win = NULL;
    int i;
    for (i = 0; i < DOSGUI_MAX_WINDOWS; i++) {
        if (!g_dwm.windows[i].alive) { win = &g_dwm.windows[i]; break; }
    }
    if (!win) return -1;

    memset(win, 0, sizeof(*win));
    win->id = g_dwm.next_id++;
    win->flags = DOSGUI_WIN_NORMAL;
    win->x = x; win->y = y; win->w = w; win->h = h;
    win->desktop = g_dwm.current_desktop;  /* Assign to current virtual desktop */
    win->alive = true;
    strncpy(win->title, title ? title : "Window", sizeof(win->title) - 1);

    g_dwm.zorder[g_dwm.nz++] = i;
    g_dwm.focused_id = i;
    return i;
}

void close_win(int i) {
    if (i < 0 || i >= DOSGUI_MAX_WINDOWS) return;
    g_dwm.windows[i].alive = false;
    g_dwm.windows[i].flags = DOSGUI_WIN_UNUSED;
    int j = 0;
    while (j < g_dwm.nz && g_dwm.zorder[j] != i) j++;
    for (; j < g_dwm.nz - 1; j++)
        g_dwm.zorder[j] = g_dwm.zorder[j + 1];
    if (j < g_dwm.nz) g_dwm.nz--;
    if (g_dwm.drag_id == i) g_dwm.drag_id = -1;
    if (g_dwm.focused_id == i)
        g_dwm.focused_id = g_dwm.nz ? g_dwm.zorder[g_dwm.nz - 1] : -1;
}

int hit_test(int x, int y) {
    for (int j = g_dwm.nz - 1; j >= 0; j--) {
        DosGuiWindow *w = &g_dwm.windows[g_dwm.zorder[j]];
        if (w->alive && w->desktop == g_dwm.current_desktop &&
            x >= w->x && x < w->x + w->w &&
            y >= w->y && y < w->y + w->h)
            return g_dwm.zorder[j];
    }
    return -1;
}

void wm_get_mouse(int *x, int *y) {
    if (x) *x = g_dwm.mouse_x;
    if (y) *y = g_dwm.mouse_y;
}

DosGuiWindow *wm_create(int x, int y, int w, int h,
                               const char *title) {
    int i = spawn_window(x, y, w, h, title);
    if (i < 0) return NULL;
    return &g_dwm.windows[i];
}

void wm_destroy(DosGuiWindow *win) {
    if (!win) return;
    for (int i = 0; i < DOSGUI_MAX_WINDOWS; i++) {
        if (&g_dwm.windows[i] == win) { close_win(i); return; }
    }
}

void wm_set_focus(DosGuiWindow *win) {
    if (!win) return;
    for (int i = 0; i < DOSGUI_MAX_WINDOWS; i++) {
        if (&g_dwm.windows[i] == win) {
            raise_win(i);
            g_dwm.focused_id = i;
            return;
        }
    }
}

DosGuiWindow *wm_get_focused(void) {
    if (g_dwm.focused_id < 0) return NULL;
    return &g_dwm.windows[g_dwm.focused_id];
}

DosGuiWindow *wm_find_by_id(int id) {
    for (int i = 0; i < DOSGUI_MAX_WINDOWS; i++)
        if (g_dwm.windows[i].alive && g_dwm.windows[i].id == id)
            return &g_dwm.windows[i];
    return NULL;
}

/* -- Live-window list -------------------------------------------------- */
/* Index over the ALIVE windows, in creation order. Task Manager enumerates
 * these: a single-process OS has no fabricated process table, so the window
 * set IS the process set. All return NULL/0/false for an out-of-range index. */
static int window_at(int i) {
    if (i < 0) return -1;
    int seen = 0;
    for (int k = 0; k < DOSGUI_MAX_WINDOWS; k++) {
        if (!g_dwm.windows[k].alive) continue;
        if (seen++ == i) return k;
    }
    return -1;
}

const char *wm_window_title_at(int i) {
    int k = window_at(i);
    return k < 0 ? NULL : g_dwm.windows[k].title;
}

int wm_window_id_at(int i) {
    int k = window_at(i);
    return k < 0 ? 0 : g_dwm.windows[k].id;
}

bool wm_window_minimized_at(int i) {
    int k = window_at(i);
    return k >= 0 && (g_dwm.windows[k].flags & DOSGUI_WIN_MINIMIZED) != 0;
}

int wm_window_count(void) {
    return g_dwm.nz;
}

/* -- Virtual Desktop Migration ------------------------------------- */

void wm_move_window_to_desktop(DosGuiWindow *win, int desktop) {
    if (!win) return;
    if (desktop < 0 || desktop >= g_dwm.desktop_count) return;
    win->desktop = desktop;
    /* If moved away from current desktop, unfocus it */
    if (win->desktop != g_dwm.current_desktop && g_dwm.focused_id >= 0) {
        DosGuiWindow *focused = &g_dwm.windows[g_dwm.focused_id];
        if (focused == win) {
            g_dwm.focused_id = -1;
        }
    }
}

int wm_get_current_desktop(void) {
    return g_dwm.current_desktop;
}

void wm_add_desktop(void) {
    if (g_dwm.desktop_count >= DOSGUI_MAX_DESKTOPS) return;
    g_dwm.desktop_count++;
}

void wm_set_current_desktop(int desktop) {
    if (desktop < 0 || desktop >= g_dwm.desktop_count) return;
    g_dwm.current_desktop = desktop;
    /* Unfocus window if it's not on the new desktop */
    if (g_dwm.focused_id >= 0) {
        DosGuiWindow *w = &g_dwm.windows[g_dwm.focused_id];
        if (w->alive && w->desktop != g_dwm.current_desktop) {
            g_dwm.focused_id = -1;
        }
    }
}

/* Move focused window to adjacent desktop (Win+Shift+Left/Right) */
/* Cycle focus through live windows in z-order. Wheel over the taskbar in Windows walks
 * the window list; we had no way to move focus without the mouse at all. */
void wm_wheel_cycle(int delta) {
    if (g_dwm.nz <= 1) return;
    int cur = 0;
    for (int j = 0; j < g_dwm.nz; j++)
        if (g_dwm.zorder[j] == g_dwm.focused_id) { cur = j; break; }
    int next = (cur + (delta >= 0 ? 1 : -1) + g_dwm.nz) % g_dwm.nz;
    int id = g_dwm.zorder[next];
    if (id >= 0 && id < DOSGUI_MAX_WINDOWS && g_dwm.windows[id].alive)
        wm_set_focus(&g_dwm.windows[id]);
}

void wm_move_focused_window(int delta) {
    if (g_dwm.focused_id < 0) return;
    DosGuiWindow *w = &g_dwm.windows[g_dwm.focused_id];
    if (!w->alive) return;
    int new_desktop = w->desktop + delta;
    if (new_desktop < 0) new_desktop = 0;
    if (new_desktop >= g_dwm.desktop_count) new_desktop = g_dwm.desktop_count - 1;
    if (new_desktop != w->desktop) {
        wm_move_window_to_desktop(w, new_desktop);
    }
}

DosGuiWindow *wm_spawn(int x, int y, int w, int h,
                               const char *title,
                               void (*on_draw)(DosGuiWindow *, uint32_t *, int, int)) {
    int i = spawn_window(x, y, w, h, title);
    if (i < 0) return NULL;
    g_dwm.windows[i].on_draw = on_draw;
    return &g_dwm.windows[i];
}

/* -- Icon persistence (Stream 2) & wallpaper reload (Stream 4) ---- */

bool wm_is_initialized(void) { return g_dwm.screen_w > 0; }

int wm_wallpaper_mode(void) { return g_dwm.wallpaper_mode; }
int wm_wallpaper_w(void)    { return g_dwm.wallpaper_w; }
int wm_wallpaper_h(void)    { return g_dwm.wallpaper_h; }

/* Re-decode the configured wallpaper from settings (Control Panel apply). */
void wm_reload_wallpaper(void) {
    if (g_dwm.wallpaper) { free(g_dwm.wallpaper); g_dwm.wallpaper = NULL; }
    load_default_wallpaper();
}
