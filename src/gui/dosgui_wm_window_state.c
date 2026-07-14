/*
 * dosgui_wm_window_state.c -- WuBuOS DosGui WM: window state + modal dialogs
 *
 * Self-contained concern split out of dosgui_wm_ctxmenu.c: window geometry
 * state transitions (resize / move / maximize / minimize / restore) and the
 * modal-dialog helpers (create_modal / is_modal). These operate on the
 * shared WM window array (dosgui_wm_internal.h). No context-menu engine,
 * no desktop/icon actions, no rendering.
 */

#include "dosgui_wm_internal.h"

#include <string.h>

void dosgui_wm_resize(DosGuiWindow *win, int w, int h) {
    if (!win) return;
    if (w < 100) w = 100;
    if (h < 50) h = 50;
    if (w > g_dwm.screen_w) w = g_dwm.screen_w;
    if (h > g_dwm.screen_h - dosgui_taskbar_height()) h = g_dwm.screen_h - dosgui_taskbar_height();
    win->w = w;
    win->h = h;
    if (win->on_resize) {
        win->on_resize(win, w, h);
    }
}

void dosgui_wm_move(DosGuiWindow *win, int x, int y) {
    if (!win) return;
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (x + win->w > g_dwm.screen_w) x = g_dwm.screen_w - win->w;
    if (y + win->h > g_dwm.screen_h - dosgui_taskbar_height()) y = g_dwm.screen_h - dosgui_taskbar_height() - win->h;
    win->x = x;
    win->y = y;
}

void dosgui_wm_maximize(DosGuiWindow *win) {
    if (!win) return;
    if (win->flags & DOSGUI_WIN_MAXIMIZED) return;
    win->min_x = win->x;
    win->min_y = win->y;
    win->min_w = win->w;
    win->min_h = win->h;
    win->x = 0;
    win->y = 0;
    win->w = g_dwm.screen_w;
    win->h = g_dwm.screen_h - dosgui_taskbar_height();
    win->flags |= DOSGUI_WIN_MAXIMIZED;
    if (win->on_resize) {
        win->on_resize(win, win->w, win->h);
    }
}

void dosgui_wm_minimize(DosGuiWindow *win) {
    if (!win) return;
    win->flags |= DOSGUI_WIN_MINIMIZED;
}

void dosgui_wm_restore(DosGuiWindow *win) {
    if (!win) return;
    if (win->flags & DOSGUI_WIN_MAXIMIZED) {
        win->x = win->min_x;
        win->y = win->min_y;
        win->w = win->min_w;
        win->h = win->min_h;
        win->flags &= ~DOSGUI_WIN_MAXIMIZED;
        if (win->on_resize) {
            win->on_resize(win, win->w, win->h);
        }
    }
    win->flags &= ~DOSGUI_WIN_MINIMIZED;
}

bool dosgui_wm_is_maximized(DosGuiWindow *win) {
    return win && (win->flags & DOSGUI_WIN_MAXIMIZED);
}

bool dosgui_wm_is_minimized(DosGuiWindow *win) {
    return win && (win->flags & DOSGUI_WIN_MINIMIZED);
}

/* -- Modal Dialog Support ------------------------------------------- */

DosGuiWindow *dosgui_wm_create_modal(int x, int y, int w, int h,
                                      const char *title,
                                      DosGuiWindow *parent) {
    DosGuiWindow *win = dosgui_wm_create(x, y, w, h, title);
    if (!win) return NULL;
    win->is_modal = true;
    win->parent = parent;
    /* Raise above parent */
    if (parent) {
        int parent_idx = -1;
        for (int i = 0; i < DOSGUI_MAX_WINDOWS; i++) {
            if (&g_dwm.windows[i] == parent) { parent_idx = i; break; }
        }
        if (parent_idx >= 0) {
            /* Insert modal just above parent in z-order */
            for (int j = g_dwm.nz - 1; j >= 0; j--) {
                if (g_dwm.zorder[j] == parent_idx) {
                    if (j + 1 < g_dwm.nz) {
                        memmove(&g_dwm.zorder[j + 2], &g_dwm.zorder[j + 1],
                                (g_dwm.nz - j - 1) * sizeof(int));
                    }
                    g_dwm.zorder[j + 1] = win - g_dwm.windows;
                    break;
                }
            }
        }
    }
    return win;
}

bool dosgui_wm_is_modal(DosGuiWindow *win) {
    return win && win->is_modal;
}
