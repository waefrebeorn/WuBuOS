/*
 * wubu_wm.c  --  WuBuOS Window Manager (Core)
 *
 * Core WM operations: init, window lifecycle, GAAD snap.
 * Sub-modules: wubu_wm_desktop.c, wubu_wm_input.c, wubu_wm_render.c
 */
#include "wubu_wm_internal.h"
#include "../kernel/vbe.h"
#include "../gui/wubu_theme.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

/* -- Global WM State (non-static for sub-modules) ------------------ */

WubuWM g_wm;

/* -- Init / Shutdown ---------------------------------------------- */

int wubu_wm_init(int screen_w, int screen_h) {
    memset(&g_wm, 0, sizeof(g_wm));
    g_wm.screen_w = screen_w;
    g_wm.screen_h = screen_h;
    g_wm.next_id = 1;
    g_wm.focused_id = -1;
    g_wm.drag_kind = DRAG_NONE;

    /* Virtual desktops: default 4 */
    g_wm.desktops.current = 0;
    g_wm.desktops.count = 4;
    for (int i = 0; i < WUBU_WM_MAX_DESKTOPS; i++) {
        snprintf(g_wm.desktops.names[i], sizeof(g_wm.desktops.names[i]),
                 "Desktop %d", i + 1);
    }

    /* GAAD decomposition */
    wubu_gaad_decompose_feng_shui(screen_w, screen_h, 4,
                                   &g_wm.gaad, &g_wm.feng_shui);

    /* Theme engine */
    wubu_theme_init();

    return 0;
}

void wubu_wm_shutdown(void) {
    for (int i = 0; i < WUBU_WM_MAX_WINDOWS; i++)
        if (g_wm.windows[i].flags != WUBU_WIN_UNUSED)
            wubu_wm_close(&g_wm.windows[i]);
    wubu_theme_shutdown();
}

/* -- Window Create / Destroy -------------------------------------- */

WubuWin *wubu_wm_create(int x, int y, int w, int h, const char *title) {
    WubuWin *win = NULL;
    for (int i = 0; i < WUBU_WM_MAX_WINDOWS; i++) {
        if (g_wm.windows[i].flags == WUBU_WIN_UNUSED) {
            win = &g_wm.windows[i];
            break;
        }
    }
    if (!win) return NULL;

    memset(win, 0, sizeof(WubuWin));
    win->id = g_wm.next_id++;
    win->flags = WUBU_WIN_NORMAL;
    win->desktop = g_wm.desktops.current;
    win->x = x; win->y = y; win->w = w; win->h = h;
    win->min_w = 100; win->min_h = 60;
    win->z_order = win->id;
    win->snap_region = -1;
    win->was_snapped = false;
    strncpy(win->title, title ? title : "Window", sizeof(win->title) - 1);

    wubu_wm_set_focus(win);
    return win;
}

void wubu_wm_destroy(WubuWin *win) {
    if (!win) return;
    if (win->on_close) win->on_close(win);
    win->flags = WUBU_WIN_UNUSED;
}

void wubu_wm_set_focus(WubuWin *win) {
    if (!win) return;
    for (int i = 0; i < WUBU_WM_MAX_WINDOWS; i++)
        g_wm.windows[i].flags &= ~WUBU_WIN_FOCUSED;
    win->flags |= WUBU_WIN_FOCUSED;
    win->z_order = 10000 + win->id;
    g_wm.focused_id = win->id;
}

WubuWin *wubu_wm_get_focused(void) {
    return wubu_wm_find(g_wm.focused_id);
}

WubuWin *wubu_wm_find(int id) {
    for (int i = 0; i < WUBU_WM_MAX_WINDOWS; i++)
        if (g_wm.windows[i].id == id && g_wm.windows[i].flags != WUBU_WIN_UNUSED)
            return &g_wm.windows[i];
    return NULL;
}

int wubu_wm_count(void) {
    int n = 0;
    for (int i = 0; i < WUBU_WM_MAX_WINDOWS; i++)
        if (g_wm.windows[i].flags != WUBU_WIN_UNUSED) n++;
    return n;
}

/* -- Window Operations -------------------------------------------- */

void wubu_wm_minimize(WubuWin *win) {
    if (!win) return;
    win->save_x = win->x; win->save_y = win->y;
    win->save_w = win->w; win->save_h = win->h;
    win->flags &= ~WUBU_WIN_NORMAL;
    win->flags &= ~WUBU_WIN_FOCUSED;
    win->flags |= WUBU_WIN_MINIMIZED;
    /* Focus next window */
    for (int i = 0; i < WUBU_WM_MAX_WINDOWS; i++) {
        WubuWin *w = &g_wm.windows[i];
        if (w->flags != WUBU_WIN_UNUSED && !(w->flags & WUBU_WIN_MINIMIZED)
            && w->desktop == g_wm.desktops.current) {
            wubu_wm_set_focus(w);
            break;
        }
    }
}

void wubu_wm_maximize(WubuWin *win) {
    if (!win) return;
    if (win->flags & WUBU_WIN_MAXIMIZED) {
        wubu_wm_restore(win);
        return;
    }
    win->save_x = win->x; win->save_y = win->y;
    win->save_w = win->w; win->save_h = win->h;
    win->x = 0; win->y = 0;
    win->w = g_wm.screen_w;
    win->h = g_wm.screen_h - 28;
    win->flags |= WUBU_WIN_MAXIMIZED;
    win->was_snapped = false;
}

void wubu_wm_restore(WubuWin *win) {
    if (!win) return;
    if (win->save_w > 0 && win->save_h > 0) {
        win->x = win->save_x; win->y = win->save_y;
        win->w = win->save_w; win->h = win->save_h;
    }
    win->flags &= ~WUBU_WIN_MAXIMIZED;
    win->flags &= ~WUBU_WIN_MINIMIZED;
    win->flags |= WUBU_WIN_NORMAL;
}

void wubu_wm_close(WubuWin *win) {
    wubu_wm_destroy(win);
}

void wubu_wm_move(WubuWin *win, int x, int y) {
    if (!win) return;
    win->x = x; win->y = y;
    win->was_snapped = false;
    win->snap_region = -1;
}

void wubu_wm_resize(WubuWin *win, int w, int h) {
    if (!win) return;
    if (w < win->min_w) w = win->min_w;
    if (h < win->min_h) h = win->min_h;
    win->w = w; win->h = h;
    if (win->on_resize) win->on_resize(win, w, h);
}

void wubu_wm_gaad_snap(WubuWin *win) {
    if (!win) return;

    int sx, sy, sw, sh;
    if (wubu_gaad_feng_shui_snap(&g_wm.feng_shui,
                                  win->x, win->y, win->w, win->h,
                                  WUBU_WM_SNAP_DIST,
                                  &sx, &sy, &sw, &sh)) {
        win->x = sx; win->y = sy;
        win->w = sw; win->h = sh;
        win->was_snapped = true;
        win->snap_region = -2;
        return;
    }

    int idx = wubu_gaad_find_snap(&g_wm.gaad,
                                   win->x, win->y, win->w, win->h,
                                   WUBU_WM_SNAP_DIST);
    if (idx >= 0) {
        int rx, ry, rw, rh;
        wubu_gaad_snap_pos(&g_wm.gaad, idx, &rx, &ry, &rw, &rh);
        win->x = rx; win->y = ry;
        win->was_snapped = true;
        win->snap_region = idx;
        return;
    }

    win->was_snapped = false;
    win->snap_region = -1;
}

/* -- State Accessor ------------------------------------------------ */

WubuWM *wubu_wm_state(void) {
    return &g_wm;
}
