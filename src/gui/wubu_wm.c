/*
 * wubu_wm.c  --  WuBuOS Window Manager Implementation
 *
 * Cell 395: Full WM with drag, GAAD snap, virtual desktops, themes.
 */
#include "wubu_wm.h"
#include "../kernel/vbe.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

/* -- Global WM State ---------------------------------------------- */

static WubuWM g_wm;

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
    /* Unfocus all */
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
        /* Restore */
        wubu_wm_restore(win);
        return;
    }
    win->save_x = win->x; win->save_y = win->y;
    win->save_w = win->w; win->save_h = win->h;
    win->x = 0; win->y = 0;
    win->w = g_wm.screen_w;
    win->h = g_wm.screen_h - 28; /* Minus taskbar */
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

    /* Try feng shui snap first (cardinal mirrors) */
    int sx, sy, sw, sh;
    if (wubu_gaad_feng_shui_snap(&g_wm.feng_shui,
                                  win->x, win->y, win->w, win->h,
                                  WUBU_WM_SNAP_DIST,
                                  &sx, &sy, &sw, &sh)) {
        win->x = sx; win->y = sy;
        win->w = sw; win->h = sh;
        win->was_snapped = true;
        win->snap_region = -2; /* Feng shui snap */
        return;
    }

    /* Try regular GAAD snap */
    int idx = wubu_gaad_find_snap(&g_wm.gaad,
                                   win->x, win->y, win->w, win->h,
                                   WUBU_WM_SNAP_DIST);
    if (idx >= 0) {
        int rx, ry, rw, rh;
        wubu_gaad_snap_pos(&g_wm.gaad, idx, &rx, &ry, &rw, &rh);
        win->x = rx; win->y = ry;
        /* Don't resize to GAAD region  --  just snap position */
        win->was_snapped = true;
        win->snap_region = idx;
        return;
    }

    win->was_snapped = false;
    win->snap_region = -1;
}

/* -- Virtual Desktops --------------------------------------------- */

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

int  wubu_wm_desktop_current(void) { return g_wm.desktops.current; }
int  wubu_wm_desktop_count(void) { return g_wm.desktops.count; }

void wubu_wm_desktop_set_count(int count) {
    if (count >= 1 && count <= WUBU_WM_MAX_DESKTOPS)
        g_wm.desktops.count = count;
}

void wubu_wm_desktop_move_win(WubuWin *win, int desktop) {
    if (!win || desktop < 0 || desktop >= g_wm.desktops.count) return;
    win->desktop = desktop;
}

/* -- Input Handling ----------------------------------------------- */

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
        /* Check title bar buttons: close, minimize, maximize */
        /* Check title bar: start drag */
        /* Check edges: start resize */
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
            /* End drag  --  GAAD snap if it was a move */
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
                /* Show GAAD snap preview while dragging near regions */
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

/* -- Rendering ---------------------------------------------------- */

void wubu_wm_render(uint32_t *fb, int fb_w, int fb_h) {
    (void)fb; (void)fb_w; (void)fb_h;
    const WubuThemeColors *tc = wubu_theme_colors();

    /* Desktop background */
    vbe_fill_rect(0, 0, g_wm.screen_w, g_wm.screen_h - 28, tc->desktop_bg);

    /* Draw windows on current desktop, sorted by z_order */
    for (int z = 0; z < 20000; z++) {
        for (int i = 0; i < WUBU_WM_MAX_WINDOWS; i++) {
            WubuWin *w = &g_wm.windows[i];
            if (w->flags == WUBU_WIN_UNUSED || (w->flags & WUBU_WIN_MINIMIZED))
                continue;
            if (w->z_order != z) continue;
            if (w->desktop != g_wm.desktops.current && !(w->flags & WUBU_WIN_STICKY))
                continue;

            /* Window chrome */
            int bw = WUBU_WM_BORDER_W;
            int th = WUBU_WM_TITLE_H;

            /* 3D border */
            vbe_3d_raised(w->x, w->y, w->w, w->h);

            /* Window body */
            vbe_fill_rect(w->x+bw, w->y+bw, w->w-2*bw, w->h-2*bw, tc->win_face);

            /* Title bar */
            uint32_t title_color = (w->flags & WUBU_WIN_FOCUSED)
                                   ? tc->win_title_active
                                   : tc->win_title_inactive;
            vbe_fill_rect(w->x+bw, w->y+bw, w->w-2*bw, th, title_color);

            /* Close button */
            int cbx = w->x + w->w - bw - 18;
            int cby = w->y + bw + 3;
            vbe_fill_rect(cbx, cby, 16, 14, tc->win_face);
            vbe_3d_raised(cbx, cby, 16, 14);

            /* Maximize button */
            int mbx = cbx - 18;
            vbe_fill_rect(mbx, cby, 16, 14, tc->win_face);
            vbe_3d_raised(mbx, cby, 16, 14);

            /* Minimize button */
            int mnx = mbx - 18;
            vbe_fill_rect(mnx, cby, 16, 14, tc->win_face);
            vbe_3d_raised(mnx, cby, 16, 14);

            /* Separator */
            vbe_hline(w->x+bw, w->x+w->w-bw-1, w->y+bw+th, tc->border_dark);

            /* Client area */
            vbe_3d_sunken(w->x+bw+1, w->y+bw+th+2,
                          w->w-2*bw-2, w->h-bw-th-3);

            /* GAAD snap indicator */
            if (w->was_snapped) {
                vbe_rect(w->x, w->y, w->w, w->h, tc->select_bg);
            }

            /* Window content */
            if (w->on_draw) w->on_draw(w, NULL, 0, 0);
        }
    }

    /* Taskbar */
    {
        int ty = g_wm.screen_h - 28;
        vbe_fill_rect(0, ty, g_wm.screen_w, 28, tc->taskbar_bg);
        vbe_3d_raised(0, ty, g_wm.screen_w, 28);

        /* Start button */
        vbe_fill_rect(4, ty+3, 60, 22, tc->start_btn_face);
        vbe_3d_raised(4, ty+3, 60, 22);

        /* Window buttons in taskbar */
        int bx = 70;
        for (int i = 0; i < WUBU_WM_MAX_WINDOWS; i++) {
            WubuWin *w = &g_wm.windows[i];
            if (w->flags == WUBU_WIN_UNUSED) continue;
            int bw = 80;
            vbe_fill_rect(bx, ty+3, bw, 22, tc->win_face);
            if (w->flags & WUBU_WIN_FOCUSED)
                vbe_3d_sunken(bx, ty+3, bw, 22);
            else
                vbe_3d_raised(bx, ty+3, bw, 22);
            bx += bw + 2;
        }

        /* Desktop switcher (right side) */
        int dx = g_wm.screen_w - g_wm.desktops.count * 24 - 8;
        for (int d = 0; d < g_wm.desktops.count; d++) {
            uint32_t bg = (d == g_wm.desktops.current)
                          ? tc->select_bg : tc->win_face;
            vbe_fill_rect(dx + d*24, ty+3, 22, 22, bg);
            vbe_3d_raised(dx + d*24, ty+3, 22, 22);
        }
    }
}

void wubu_wm_invalidate(WubuWin *win) { (void)win; }

/* -- GAAD / Resolution -------------------------------------------- */

void wubu_wm_gaad_recompute(void) {
    wubu_gaad_decompose_feng_shui(g_wm.screen_w, g_wm.screen_h, 4,
                                   &g_wm.gaad, &g_wm.feng_shui);
}

void wubu_wm_set_resolution(int w, int h) {
    g_wm.screen_w = w;
    g_wm.screen_h = h;
    wubu_wm_gaad_recompute();

    /* Reposition windows that are out of bounds */
    for (int i = 0; i < WUBU_WM_MAX_WINDOWS; i++) {
        WubuWin *win = &g_wm.windows[i];
        if (win->flags == WUBU_WIN_UNUSED) continue;
        if (win->x + win->w > w) win->x = w - win->w;
        if (win->y + win->h > h - 28) win->y = h - 28 - win->h;
        if (win->x < 0) win->x = 0;
        if (win->y < 0) win->y = 0;
    }
}

WubuWM *wubu_wm_state(void) { return &g_wm; }
