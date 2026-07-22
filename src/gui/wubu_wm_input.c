/*
 * wubu_wm_input.c  --  WuBuOS Window Manager Input Handling
 *
 * Keyboard and mouse input dispatch for the WM.
 * Routes events to windows, handles title bar buttons,
 * window dragging and resize operations.
 */
#include "wubu_wm_internal.h"
#include "../kernel/vbe.h"
#include <string.h>

void wubu_wm_handle_key(uint32_t key, uint32_t mods) {
    /* Ctrl+Alt+Left/Right = switch desktop */
    if ((mods & 0x06) == 0x06) { /* Ctrl+Alt */
        if (key == 0x25) { wubu_wm_desktop_prev(); return; }  /* Left */
        if (key == 0x27) { wubu_wm_desktop_next(); return; }  /* Right */
    }
    /* F5 = cycle theme */
    if (key == 0x3E) { wubu_theme_cycle(); return; }

    /* Route to focused window */
    WubuWin *focused = wubu_wm_get_focused();
    if (focused && focused->on_key) focused->on_key(focused, key, mods);
}

void wubu_wm_handle_mouse(int x, int y, int btn, int kind) {
    if (kind == 1) {  /* Mouse down */
        WubuWin *hit = NULL;
        int best_z = -1;
        for (int i = 0; i < WUBU_WM_MAX_WINDOWS; i++) {
            WubuWin *w = &g_wm.windows[i];
            if (w->flags == WUBU_WIN_UNUSED || (w->flags & WUBU_WIN_MINIMIZED))
                continue;
            if (w->desktop != g_wm.desktops.current && !(w->flags & WUBU_WIN_STICKY))
                continue;
            if (x >= w->x && x < w->x + w->w &&
                y >= w->y && y < w->y + w->h) {
                if (w->z_order > best_z) { hit = w; best_z = w->z_order; }
            }
        }

        if (!hit) return;
        wubu_wm_set_focus(hit);

        /* Check if click is on title bar (drag) */
        if (y < hit->y + WUBU_WM_TITLE_H + WUBU_WM_BORDER_W) {
            /* Check close button (top-right) */
            int cbx = hit->x + hit->w - WUBU_WM_BORDER_W - 18;
            int cby = hit->y + WUBU_WM_BORDER_W + 3;
            if (x >= cbx && x < cbx + 16 && y >= cby && y < cby + 14) {
                wubu_wm_close(hit);
                return;
            }
            /* Check maximize button */
            int mbx = cbx - 18;
            if (x >= mbx && x < mbx + 16 && y >= cby && y < cby + 14) {
                wubu_wm_maximize(hit);
                return;
            }
            /* Check minimize button */
            int mnx = mbx - 18;
            if (x >= mnx && x < mnx + 16 && y >= cby && y < cby + 14) {
                wubu_wm_minimize(hit);
                return;
            }

            /* Start drag */
            g_wm.drag_kind = DRAG_MOVE;
            g_wm.drag_win_id = hit->id;
            g_wm.drag_start_x = x;
            g_wm.drag_start_y = y;
            g_wm.drag_win_x = hit->x;
            g_wm.drag_win_y = hit->y;
            g_wm.gaad_snap_preview = false;
            return;
        }

        /* Check edges for resize */
        int bw = WUBU_WM_BORDER_W;
        bool on_right = (x >= hit->x + hit->w - bw);
        bool on_bottom = (y >= hit->y + hit->h - bw);
        bool on_left = (x <= hit->x + bw);
        bool on_top = (y <= hit->y + bw);

        if (on_right && on_bottom) g_wm.drag_kind = DRAG_RESIZE_SE;
        else if (on_left && on_bottom) g_wm.drag_kind = DRAG_RESIZE_SW;
        else if (on_right && on_top) g_wm.drag_kind = DRAG_RESIZE_NE;
        else if (on_left && on_top) g_wm.drag_kind = DRAG_RESIZE_NW;
        else if (on_right) g_wm.drag_kind = DRAG_RESIZE_E;
        else if (on_left) g_wm.drag_kind = DRAG_RESIZE_W;
        else if (on_bottom) g_wm.drag_kind = DRAG_RESIZE_S;
        else if (on_top) g_wm.drag_kind = DRAG_RESIZE_N;
        else g_wm.drag_kind = DRAG_NONE;

        if (g_wm.drag_kind != DRAG_NONE) {
            g_wm.drag_win_id = hit->id;
            g_wm.drag_start_x = x; g_wm.drag_start_y = y;
            g_wm.drag_win_x = hit->x; g_wm.drag_win_y = hit->y;
            g_wm.drag_win_w = hit->w; g_wm.drag_win_h = hit->h;
        }

        /* Route to window content */
        if (hit->on_mouse) hit->on_mouse(hit, x - hit->x, y - hit->y, btn, kind);

    } else if (kind == 2) {  /* Mouse up */
        if (g_wm.drag_kind != DRAG_NONE) {
            /* End drag — GAAD snap if it was a move */
            if (g_wm.drag_kind == DRAG_MOVE) {
                WubuWin *win = wubu_wm_find(g_wm.drag_win_id);
                if (win) wubu_wm_gaad_snap(win);
            }
            g_wm.drag_kind = DRAG_NONE;
            g_wm.gaad_snap_preview = false;
        }

    } else if (kind == 0) {  /* Mouse move */
        if (g_wm.drag_kind != DRAG_NONE) {
            WubuWin *win = wubu_wm_find(g_wm.drag_win_id);
            if (!win) { g_wm.drag_kind = DRAG_NONE; return; }

            int dx = x - g_wm.drag_start_x;
            int dy = y - g_wm.drag_start_y;

            if (g_wm.drag_kind == DRAG_MOVE) {
                win->x = g_wm.drag_win_x + dx;
                win->y = g_wm.drag_win_y + dy;
                win->flags &= ~WUBU_WIN_MAXIMIZED;
                g_wm.gaad_snap_preview = true;
            } else {
                /* Resize */
                int nw = g_wm.drag_win_w;
                int nh = g_wm.drag_win_h;
                int nx = g_wm.drag_win_x;
                int ny = g_wm.drag_win_y;

                switch (g_wm.drag_kind) {
                    case DRAG_RESIZE_E:  nw += dx; break;
                    case DRAG_RESIZE_W:  nw -= dx; nx += dx; break;
                    case DRAG_RESIZE_S:  nh += dy; break;
                    case DRAG_RESIZE_N:  nh -= dy; ny += dy; break;
                    case DRAG_RESIZE_SE: nw += dx; nh += dy; break;
                    case DRAG_RESIZE_SW: nw -= dx; nx += dx; nh += dy; break;
                    case DRAG_RESIZE_NE: nw += dx; nh -= dy; ny += dy; break;
                    case DRAG_RESIZE_NW: nw -= dx; nx += dx; nh -= dy; ny += dy; break;
                    default: break;
                }
                win->x = nx; win->y = ny;
                wubu_wm_resize(win, nw, nh);
            }
        }
    }
}
