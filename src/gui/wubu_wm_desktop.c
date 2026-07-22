/*
 * wubu_wm_desktop.c  --  WuBuOS Window Manager Virtual Desktops
 *
 * Virtual desktop (workspace) management for the WuBuOS WM.
 * Supports 1-9 desktops with switch/next/prev/move operations.
 */
#include "wubu_wm_internal.h"

void wubu_wm_desktop_switch(int desktop) {
    if (desktop < 0 || desktop >= g_wm.desktops.count) return;
    g_wm.desktops.current = desktop;
    /* Refocus a window on the new desktop */
    for (int i = 0; i < WUBU_WM_MAX_WINDOWS; i++) {
        WubuWin *w = &g_wm.windows[i];
        if (w->flags != WUBU_WIN_UNUSED && !(w->flags & WUBU_WIN_MINIMIZED)
            && (w->desktop == desktop || (w->flags & WUBU_WIN_STICKY))) {
            wubu_wm_set_focus(w);
            return;
        }
    }
    g_wm.focused_id = -1;
}

void wubu_wm_desktop_next(void) {
    wubu_wm_desktop_switch((g_wm.desktops.current + 1) % g_wm.desktops.count);
}

void wubu_wm_desktop_prev(void) {
    wubu_wm_desktop_switch((g_wm.desktops.current - 1 + g_wm.desktops.count)
                           % g_wm.desktops.count);
}

int wubu_wm_desktop_current(void) {
    return g_wm.desktops.current;
}

int wubu_wm_desktop_count(void) {
    return g_wm.desktops.count;
}

void wubu_wm_desktop_set_count(int count) {
    if (count >= 1 && count <= WUBU_WM_MAX_DESKTOPS)
        g_wm.desktops.count = count;
}

void wubu_wm_desktop_move_win(WubuWin *win, int desktop) {
    if (!win || desktop < 0 || desktop >= g_wm.desktops.count) return;
    win->desktop = desktop;
}
