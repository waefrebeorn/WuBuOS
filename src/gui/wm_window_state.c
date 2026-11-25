/*
 * wm_window_state.c -- WuBuOS DosGui WM: window state + modal dialogs
 *
 * Self-contained concern split out of wm_ctxmenu.c: window geometry
 * state transitions (resize / move / maximize / minimize / restore) and the
 * modal-dialog helpers (create_modal / is_modal). These operate on the
 * shared WM window array (wm_internal.h). No context-menu engine,
 * no desktop/icon actions, no rendering.
 */

#include "wm_internal.h"

#include <string.h>

void wm_resize(DosGuiWindow *win, int w, int h) {
    if (!win) return;
    if (w < 100) w = 100;
    if (h < 50) h = 50;
    if (w > g_dwm.screen_w) w = g_dwm.screen_w;
    if (h > g_dwm.screen_h - taskbar_height()) h = g_dwm.screen_h - taskbar_height();
    win->w = w;
    win->h = h;
    if (win->on_resize) {
        win->on_resize(win, w, h);
    }
}

void wm_move(DosGuiWindow *win, int x, int y) {
    if (!win) return;
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (x + win->w > g_dwm.screen_w) x = g_dwm.screen_w - win->w;
    if (y + win->h > g_dwm.screen_h - taskbar_height()) y = g_dwm.screen_h - taskbar_height() - win->h;
    win->x = x;
    win->y = y;
}

void wm_maximize(DosGuiWindow *win) {
    if (!win) return;
    if (win->flags & DOSGUI_WIN_MAXIMIZED) return;
    win->min_x = win->x;
    win->min_y = win->y;
    win->min_w = win->w;
    win->min_h = win->h;
    win->x = 0;
    win->y = 0;
    win->w = g_dwm.screen_w;
    win->h = g_dwm.screen_h - taskbar_height();
    win->flags |= DOSGUI_WIN_MAXIMIZED;
    if (win->on_resize) {
        win->on_resize(win, win->w, win->h);
    }
}

/* Windows 11 snap, as the KEYBOARD does it: Win+Left / Win+Right put the window
 * in a half of the WORK AREA -- the screen minus the taskbar -- and Win+Up /
 * Win+Down maximize and restore.
 *
 * The shell already had a snap-layout FLYOUT (src/gui/shell/win11_snap.c) that
 * draws six layouts and applies none of them, and no keyboard snap at all, so a
 * user coming from Windows pressed Win+Left first and nothing happened. A snapped
 * window is deliberately NOT flagged maximized: Win+Down on a snapped window must
 * restore it to its floating rect, not un-maximize a half. */
/* Aero edge snap on drag release. Windows: drag a window so the POINTER touches the top or
 * a side edge and releasing snaps there. Judged by the pointer, not the window rect, because
 * that is what the user aimed with.
 *
 * Returns false when no zone was taken, so the caller can fall back to the grid snap. */
bool wm_snap_on_drag_release(DosGuiWindow *win, int px, int py) {
    if (!win || !win->alive) return false;
    const int th = WUBU_METRIC_GAP_SM + WUBU_SPACE_XS;   /* "close enough to the edge" */
    int sw = g_dwm.screen_w;
    int sh = g_dwm.screen_h - taskbar_height();
    if (px <= th)            { wm_snap_half(win, -1); return true; }
    if (px >= sw - th)       { wm_snap_half(win, +1); return true; }
    if (py <= th && py >= 0) { wm_maximize(win);      return true; }
    (void)sh;
    return false;
}

void wm_snap_half(DosGuiWindow *win, int side) {
    if (!win) return;
    /* A minimized window cannot be seen, so it must come back before it is sized
     * -- restoring also clears MAXIMIZED and returns the floating rect, which is
     * what `min_x/min_y/min_w/min_h` exist to remember. */
    if (win->flags & (DOSGUI_WIN_MINIMIZED | DOSGUI_WIN_MAXIMIZED)) wm_restore(win);

    const int gap = WUBU_METRIC_GAP_SM;
    int sw = g_dwm.screen_w;
    int sh = g_dwm.screen_h - taskbar_height();
    int w = (sw - 3 * gap) / 2;

    win->x = (side < 0) ? gap : (sw - gap - w);
    win->y = gap;
    win->w = w;
    win->h = sh - 2 * gap;
    win->flags &= ~DOSGUI_WIN_MAXIMIZED;
    if (win->on_resize) win->on_resize(win, win->w, win->h);

    /* Snap assist: offer the remaining windows in the half we did NOT fill. Weak, because a
     * WM-only link carries no shell -- same convention as the snap flyout in wm_input.c. */
    {
        extern __attribute__((weak)) void win11_snap_assist_open(int, int);
        if (win11_snap_assist_open) win11_snap_assist_open(side < 0 ? 1 : 0, win->id);
    }
}

void wm_minimize(DosGuiWindow *win) {
    if (!win) return;
    win->flags |= DOSGUI_WIN_MINIMIZED;
}

void wm_restore(DosGuiWindow *win) {
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

bool wm_is_maximized(DosGuiWindow *win) {
    return win && (win->flags & DOSGUI_WIN_MAXIMIZED);
}

bool wm_is_minimized(DosGuiWindow *win) {
    return win && (win->flags & DOSGUI_WIN_MINIMIZED);
}

/* -- Modal Dialog Support ------------------------------------------- */

DosGuiWindow *wm_create_modal(int x, int y, int w, int h,
                                      const char *title,
                                      DosGuiWindow *parent) {
    DosGuiWindow *win = wm_create(x, y, w, h, title);
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

bool wm_is_modal(DosGuiWindow *win) {
    return win && win->is_modal;
}
