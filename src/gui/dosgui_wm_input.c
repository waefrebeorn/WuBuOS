/*
 * dosgui_wm_input.c -- WuBuOS DosGui WM: input dispatch (key + mouse)
 *
 * Self-contained concern split out of dosgui_wm.c (the WM facade):
 *   - dosgui_wm_handle_key(): Alt+Tab cycling, Win-key hotkeys, theme
 *     cycling, maximize, virtual-desktop switching, window dispatch.
 *   - dosgui_wm_handle_mouse(): taskbar / systray / notification-center hit
 *     testing, window chrome (close/max/min), title-bar drag, client-area
 *     routing, drag + icon drag with GAAD/icon-grid snapping.
 *
 * Depends only on the shared WM state (dosgui_wm_internal.h) and the public
 * APIs of the sub-systems it routes to (start menu, notif center, icons,
 * taskbar, holyc term). No window lifecycle, no rendering.
 */

#include "dosgui_wm_internal.h"
#include "dosgui_startmenu.h"
#include "dosgui_wm.h"

#include <string.h>
#include <stdio.h>
#include <time.h>

void dosgui_wm_handle_key(uint32_t key, uint32_t mods) {
    /* Alt+Tab: cycle through windows */
    bool alt_held = (mods & 0x08) != 0;
    if (alt_held && key == 0x09 && g_dwm.nz > 1) {
        /* Find current focused index in zorder */
        int cur_idx = 0;
        for (int j = 0; j < g_dwm.nz; j++) {
            if (g_dwm.zorder[j] == g_dwm.focused_id) { cur_idx = j; break; }
        }
        /* Focus next window (wrap around) */
        int next_idx = (cur_idx + 1) % g_dwm.nz;
        int next_id = g_dwm.zorder[next_idx];
        if (next_id >= 0 && next_id < DOSGUI_MAX_WINDOWS && g_dwm.windows[next_id].alive) {
            raise_win(next_id);
            g_dwm.focused_id = next_id;
        }
        return;
    }

    /* Win key (left or right): toggle start menu */
    if (key == 0xE05B || key == 0xE05C) {
        dosgui_startmenu_toggle();
        return;
    }

    /* Win+H: spawn HolyC terminal */
    if ((mods & 0x08) && (key == 0x48 || key == 'h' || key == 'H')) {
        dosgui_wm_spawn_holyc_term(100, 100, 700, 500);
        return;
    }

    /* First, try to dispatch to focused window */
    if (g_dwm.focused_id >= 0) {
        DosGuiWindow *w = &g_dwm.windows[g_dwm.focused_id];
        if (w->alive && w->on_key) {
            w->on_key(w, key, mods);
            return;
        }
    }

    /* Global hotkeys */
    if (key == 111 && g_dwm.focused_id >= 0) {
        close_win(g_dwm.focused_id);
    }
    if ((mods & 0x4) && key == 0x14) {
        wubu_theme_cycle();
        fprintf(stderr, "Theme cycled to: %s\n", wubu_theme_name(wubu_theme_current()));
    }
    if (key == 0x3F) {
        wubu_theme_cycle();
        fprintf(stderr, "Theme cycled to: %s\n", wubu_theme_name(wubu_theme_current()));
    }
    if (key == 0x57 && g_dwm.focused_id >= 0) {
        DosGuiWindow *w = &g_dwm.windows[g_dwm.focused_id];
        if (w->flags & DOSGUI_WIN_MAXIMIZED) {
            w->x = w->min_x; w->y = w->min_y;
            w->w = w->min_w; w->h = w->min_h;
            w->flags &= ~DOSGUI_WIN_MAXIMIZED;
        } else {
            w->min_x = w->x; w->min_y = w->y;
            w->min_w = w->w; w->min_h = w->h;
            w->x = 0; w->y = 0;
            w->w = g_dwm.screen_w; w->h = g_dwm.screen_h - taskbar_height_dynamic();
            w->flags |= DOSGUI_WIN_MAXIMIZED;
        }
    }
    if ((mods & 0x4) && (mods & 0x8)) {
        if (key == 0xE04B) {
            g_dwm.current_desktop = (g_dwm.current_desktop - 1 + g_dwm.desktop_count) % g_dwm.desktop_count;
        } else if (key == 0xE04D) {
            g_dwm.current_desktop = (g_dwm.current_desktop + 1) % g_dwm.desktop_count;
        }
    }
    /* Win+Shift+Left/Right: Move focused window to adjacent desktop */
    if ((mods & 0x09) == 0x09) {  /* Win + Shift */
        if (key == 0xE04B) {  /* Left arrow */
            dosgui_wm_move_focused_window(-1);
            return;
        } else if (key == 0xE04D) {  /* Right arrow */
            dosgui_wm_move_focused_window(1);
            return;
        }
    }
}

void dosgui_wm_handle_mouse(int x, int y, int btn, int kind) {
    g_dwm.mouse_x = x;
    g_dwm.mouse_y = y;

    int task_h = taskbar_height_dynamic();
    int tbh = title_bar_height();
    border_width();  // ensure theme is loaded

    if (y >= g_dwm.screen_h - task_h) {
        int by = g_dwm.screen_h - task_h + (task_h - 24) / 2;
        int start_w = theme()->Luna_start_button ? 54 : 60;

        if (x >= 4 && x < 4 + start_w + 20 && y >= by && y < by + 24) {
            dosgui_startmenu_toggle();
            return;
        }

        int bx = theme()->Luna_start_button ? 82 : 72;
        for (int j = 0; j < g_dwm.nz; j++) {
            DosGuiWindow *w = &g_dwm.windows[g_dwm.zorder[j]];
            if (!w->alive || (w->flags & DOSGUI_WIN_MINIMIZED)) continue;
            int bw = (int)strlen(w->title) * 6 + 16;
            if (bw > 160) bw = 160;
            if (x >= bx && x < bx + bw && y >= by && y < by + 22) {
                if (w->flags & DOSGUI_WIN_MINIMIZED) {
                    w->flags &= ~DOSGUI_WIN_MINIMIZED;
                } else if (g_dwm.focused_id == g_dwm.zorder[j]) {
                    w->flags |= DOSGUI_WIN_MINIMIZED;
                } else {
                    dosgui_wm_set_focus(w);
                }
                return;
            }
            bx += bw + 2;
            if (bx > g_dwm.screen_w - 160) break;
        }

        int desk_x = g_dwm.screen_w - 150;
        for (int d = 0; d < g_dwm.desktop_count; d++) {
            int dx = desk_x + d * 16;
            if (x >= dx && x < dx + 14 && y >= by && y < by + 16) {
                g_dwm.current_desktop = d;
                return;
            }
        }

        /* Check system tray icons */
        int tray_x = g_dwm.screen_w - 10;
        dosgui_taskbar_update_clock(time(NULL));
        char *clk = dosgui_taskbar_get_clock_str();
        int clk_w = vbe_text_width(clk, 1);
        tray_x -= clk_w + 10;

        for (int i = g_dwm.systray_count - 1; i >= 0; i--) {
            if (g_dwm.systray_icons[i].visible) {
                int sx = tray_x - DOSGUI_SYSTRAY_SIZE - 4;
                int sy = g_dwm.screen_h - task_h + (task_h - DOSGUI_SYSTRAY_SIZE) / 2;
                if (x >= sx && x < sx + DOSGUI_SYSTRAY_SIZE && y >= sy && y < sy + DOSGUI_SYSTRAY_SIZE) {
                    if (kind == 1 && g_dwm.systray_icons[i].on_click) {
                        g_dwm.systray_icons[i].on_click();
                    } else if (kind == 1 && btn == 2 && g_dwm.systray_icons[i].on_right_click) {
                        g_dwm.systray_icons[i].on_right_click();
                    }
                    return;
                }
                tray_x -= DOSGUI_SYSTRAY_SIZE + 4;
            }
        }

        /* Check notification center toggle (far right before clock) */
        if (x >= tray_x - 30 && x < tray_x && y >= by && y < by + 22) {
            dosgui_notif_center_toggle();
            return;
        }

        return;
    }

    if (kind == 1) {
        if (y >= g_dwm.screen_h - task_h) {
            return;
        }

        for (int j = g_dwm.nz - 1; j >= 0; j--) {
            int idx = g_dwm.zorder[j];
            DosGuiWindow *w = &g_dwm.windows[idx];
            if (!w->alive || (w->flags & DOSGUI_WIN_MINIMIZED)) continue;
            if (!(w->flags & DOSGUI_WIN_MAXIMIZED)) {
                int close_x = w->x + w->w - theme_radius() - 18;
                int close_y = w->y + theme_radius() + 2;
                if (x >= close_x && x < close_x + 14 && y >= close_y && y < close_y + 12) {
                    close_win(idx);
                    return;
                }
                if (theme()->Luna_start_button) {
                    int max_x = close_x - 20;
                    if (x >= max_x && x < max_x + 14 && y >= close_y && y < close_y + 12) {
                        if (w->flags & DOSGUI_WIN_MAXIMIZED) {
                            w->x = w->min_x; w->y = w->min_y;
                            w->w = w->min_w; w->h = w->min_h;
                            w->flags &= ~DOSGUI_WIN_MAXIMIZED;
                        } else {
                            w->min_x = w->x; w->min_y = w->y;
                            w->min_w = w->w; w->min_h = w->h;
                            w->x = 0; w->y = 0;
                            w->w = g_dwm.screen_w; w->h = g_dwm.screen_h - task_h;
                            w->flags |= DOSGUI_WIN_MAXIMIZED;
                        }
                        return;
                    }
                    int min_x = close_x - 40;
                    if (x >= min_x && x < min_x + 14 && y >= close_y && y < close_y + 12) {
                        w->flags |= DOSGUI_WIN_MINIMIZED;
                        return;
                    }
                }
            }
        }

        int i = hit_test(x, y);
        if (i < 0) {
            int icon_idx = dosgui_icon_hit_test(x, y);
            if (icon_idx >= 0) {
                if (btn == 2) { /* Right click */
                    dosgui_icon_show_context_menu(icon_idx, x, y);
                    return;
                }
                if (g_dwm.icons[icon_idx].on_click) {
                    g_dwm.icons[icon_idx].on_click();
                } else if (g_dwm.icons[icon_idx].on_execute) {
                    g_dwm.icons[icon_idx].on_execute();
                }
                g_dwm.drag_icon_id = icon_idx;
                g_dwm.drag_icon_ox = x - g_dwm.icons[icon_idx].x;
                g_dwm.drag_icon_oy = y - g_dwm.icons[icon_idx].y;
                g_dwm.focused_id = -1;
                return;
            }
            g_dwm.focused_id = -1;
            if (btn == 2) { /* Right click on empty desktop */
                dosgui_desktop_show_context_menu(x, y);
                return;
            }
            return;
        }

        raise_win(i);
        g_dwm.focused_id = i;
        DosGuiWindow *w = &g_dwm.windows[i];

        int close_x = w->x + w->w - theme_radius() - 18;
        int close_y = w->y + theme_radius() + 2;
        if (x >= close_x && x < close_x + 14 && y >= close_y && y < close_y + 12) {
            close_win(i);
            return;
        }

        if (theme()->Luna_start_button) {
            int max_x = close_x - 20;
            if (x >= max_x && x < max_x + 14 && y >= close_y && y < close_y + 12) {
                if (w->flags & DOSGUI_WIN_MAXIMIZED) {
                    w->x = w->min_x; w->y = w->min_y;
                    w->w = w->min_w; w->h = w->min_h;
                    w->flags &= ~DOSGUI_WIN_MAXIMIZED;
                } else {
                    w->min_x = w->x; w->min_y = w->y;
                    w->min_w = w->w; w->min_h = w->h;
                    w->x = 0; w->y = 0;
                    w->w = g_dwm.screen_w; w->h = g_dwm.screen_h - task_h;
                    w->flags |= DOSGUI_WIN_MAXIMIZED;
                }
                return;
            }
            int min_x = close_x - 40;
            if (x >= min_x && x < min_x + 14 && y >= close_y && y < close_y + 12) {
                w->flags |= DOSGUI_WIN_MINIMIZED;
                return;
            }
        }

        if (y < w->y + tbh) {
            g_dwm.drag_id = i;
            g_dwm.drag_ox = x - w->x;
            g_dwm.drag_oy = y - w->y;
        } else {
            /* Client area click - dispatch to window */
            if (w->on_mouse) {
                w->on_mouse(w, x - w->x, y - w->y, btn, kind);
            }
        }
    } else if (kind == 2) {
        if (g_dwm.drag_id >= 0 && g_dwm.windows[g_dwm.drag_id].alive) {
            DosGuiWindow *w = &g_dwm.windows[g_dwm.drag_id];
            /* Apply GAAD snap on drag end */
            snap_window_to_gaad(w);
        }
        g_dwm.drag_id = -1;
        if (g_dwm.drag_icon_id >= 0) {
            snap_icon_to_grid(&g_dwm.icons[g_dwm.drag_icon_id]);
            g_dwm.drag_icon_id = -1;
        }
    } else if (kind == 0) {
        if (g_dwm.drag_id >= 0 && g_dwm.windows[g_dwm.drag_id].alive) {
            DosGuiWindow *w = &g_dwm.windows[g_dwm.drag_id];
            if (!(w->flags & DOSGUI_WIN_MAXIMIZED)) {
                w->x = x - g_dwm.drag_ox;
                w->y = y - g_dwm.drag_oy;
                if (w->x < -w->w + 60) w->x = -w->w + 60;
                if (w->x > g_dwm.screen_w - 60) w->x = g_dwm.screen_w - 60;
                if (w->y < 0) w->y = 0;
                if (w->y > g_dwm.screen_h - task_h - tbh)
                    w->y = g_dwm.screen_h - task_h - tbh;
            }
        } else {
            /* Mouse move over client area - dispatch to focused window */
            if (g_dwm.focused_id >= 0) {
                DosGuiWindow *w = &g_dwm.windows[g_dwm.focused_id];
                if (w->alive && w->on_mouse) {
                    w->on_mouse(w, x - w->x, y - w->y, btn, kind);
                }
            }
        }
        if (g_dwm.drag_icon_id >= 0) {
            DosGuiIcon *icon = &g_dwm.icons[g_dwm.drag_icon_id];
            icon->x = x - g_dwm.drag_icon_ox;
            icon->y = y - g_dwm.drag_icon_oy;
            if (icon->x < 0) icon->x = 0;
            if (icon->x > g_dwm.screen_w - DOSGUI_ICON_SIZE) icon->x = g_dwm.screen_w - DOSGUI_ICON_SIZE;
            if (icon->y < 0) icon->y = 0;
            if (icon->y > g_dwm.screen_h - task_h - DOSGUI_ICON_SIZE) icon->y = g_dwm.screen_h - task_h - DOSGUI_ICON_SIZE;
        }
    }
}
