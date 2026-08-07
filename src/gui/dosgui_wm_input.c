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
#include "wubu_a11y.h"
#include "wubu_bonzi.h"

#include <string.h>
#include <stdio.h>
#include <time.h>

/* Chicago/Mac edge + corner resize: classify which border(s) the point is
 * over for the given window. Returns a bitmask: 1=left 2=right 4=top 8=bottom
 * (combinations give the 8 resize handles). Returns 0 if not on a border. */
static int hit_test_edge(DosGuiWindow *w, int x, int y) {
    int bw = border_width();
    int corner = bw * 2;            /* diagonal grab zones */
    /* The main edge grab zone must be at least 2px so a cursor positioned
     * 2px inside the edge (a common grab in synthetic + real dragging)
     * still classifies as a resize, not a title-bar / client drag. */
    int grab = corner > 2 ? corner : 2;
    int edge = 0;
    bool on_top = (y >= w->y && y < w->y + grab) || (y >= w->y && y < w->y + corner && x >= w->x && x < w->x + corner);
    bool on_bot = (y >= w->y + w->h - grab && y < w->y + w->h) ||
                  (y >= w->y + w->h - corner && y < w->y + w->h && x >= w->x + w->w - corner && x < w->x + w->w);
    bool on_left  = (x >= w->x && x < w->x + grab) ||
                    (x >= w->x && x < w->x + corner && y >= w->y && y < w->y + corner);
    bool on_right = (x >= w->x + w->w - grab && x < w->x + w->w) ||
                    (x >= w->x + w->w - corner && x < w->x + w->w && y >= w->y + w->h - corner && y < w->y + w->h);
    if (on_left)  edge |= 1;
    if (on_right) edge |= 2;
    if (on_top)   edge |= 4;
    if (on_bot)   edge |= 8;
    return edge;
}

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

    /* Global hotkeys take precedence over the focused window's on_key
     * handler, so system keys (close / maximize / theme / virtual-desktop)
     * work even when an app captures keystrokes. */
    if (key == 111 && g_dwm.focused_id >= 0) {
        close_win(g_dwm.focused_id);
        return;
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
        return;
    }
    if ((mods & 0x4) && key == 0x14) {
        wubu_theme_cycle();
        fprintf(stderr, "Theme cycled to: %s\n", wubu_theme_name(wubu_theme_current()));
        return;
    }
    if (key == 0x3F) {
        wubu_theme_cycle();
        fprintf(stderr, "Theme cycled to: %s\n", wubu_theme_name(wubu_theme_current()));
        return;
    }
    if ((mods & 0x4) && (mods & 0x8)) {
        if (key == 0xE04B) {
            g_dwm.current_desktop = (g_dwm.current_desktop - 1 + g_dwm.desktop_count) % g_dwm.desktop_count;
        } else if (key == 0xE04D) {
            g_dwm.current_desktop = (g_dwm.current_desktop + 1) % g_dwm.desktop_count;
        }
        return;
    }

    /* Then dispatch ordinary keys to the focused window. */
    if (g_dwm.focused_id >= 0) {
        DosGuiWindow *w = &g_dwm.windows[g_dwm.focused_id];
        if (w->alive && w->on_key) {
            w->on_key(w, key, mods);
            return;
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

    /* Start menu: when open, own the click — route PRESS events to the
     * menu's hit-test before anything else grabs it.  A press (kind==1)
     * on a menu item fires its action; a press outside the menu closes it.
     * Releases (kind==2) are consumed here to prevent a second toggle
     * from the Start-button release landing on a now-closed menu. */
    if (dosgui_startmenu_is_open() && kind == 1) {
        dosgui_startmenu_handle_click(x, y);
        return;
    }
    if (dosgui_startmenu_is_open() && kind == 2) {
        /* Swallow release while menu is open so the Start button's own
         * release handler in the fall-through below can't re-toggle. */
        return;
    }

    /* Clock popup (Win98 parity): a press OUTSIDE the open popup closes it
     * (and the press still processes normally). */
    if (g_dwm.clock_menu_open && kind == 1) {
        int px, py, pw, ph;
        dosgui_clock_menu_rect(g_dwm.screen_w, g_dwm.screen_h,
                               &px, &py, &pw, &ph);
        if (x < px || x >= px + pw || y < py || y >= py + ph)
            dosgui_clock_menu_close();
    }

    /* Accessibility cluster: give it first crack at the event when enabled
     * and a window is focused. Returning true consumes the event so the
     * normal taskbar/chromeo logic below cannot interfere. */
    if (wubu_a11y_is_enabled() && g_dwm.focused_id >= 0) {
        DosGuiWindow *fw = &g_dwm.windows[g_dwm.focused_id];
        if (fw->alive && !(fw->flags & DOSGUI_WIN_MINIMIZED) &&
            wubu_a11y_mouse(fw, x, y, btn, kind))
            return;
    }

    /* Taskbar / Start button: must be checked BEFORE the Bonzi Buddy hit-test,
     * because the buddy sits in the lower-left corner and its bounding box
     * overlaps the taskbar region.  A click on the Start button or taskbar
     * buttons must not be swallowed by the mascot. */
    if (y >= g_dwm.screen_h - task_h) {
        int by = g_dwm.screen_h - task_h + (task_h - 24) / 2;
        int start_w = theme()->Luna_start_button ? 54 : 60;
        /* Start button hit region must match the DRAWN width
         * (dosgui_wm_taskbar.c: Win98 draws 60px, Luna draws start_w+20).
         * The old +20 made the hit zone extend to x<84 while the Win98
         * button is only 60px wide — the phantom 20px swallowed the first
         * taskbar button's left 12px (task buttons start at x=72), so
         * clicking a minimized window's restore button's left edge opened
         * the Start menu instead of restoring the window. */
        int start_hit_w = theme()->Luna_start_button ? start_w + 20 : start_w;

        if (x >= 4 && x < 4 + start_hit_w && y >= by && y < by + 24) {
            /* Act on PRESS only — release at the same spot would toggle the
             * menu closed again (same double-fire as the taskbar buttons). */
            if (kind == 1) dosgui_startmenu_toggle();
            return;
        }

        int bx = theme()->Luna_start_button ? 82 : 72;
        for (int j = 0; j < g_dwm.nz; j++) {
            DosGuiWindow *w = &g_dwm.windows[g_dwm.zorder[j]];
            if (!w->alive) continue;   /* MINIMIZED windows must be hit-testable:
                                          their taskbar button restores them */
            /* Button width MUST match dosgui_wm_taskbar.c render exactly
             * (tw + 24, min 120, max 200) or the right part of a drawn
             * button is unclickable. */
            int tw = vbe_text_width(w->title, 1);
            int bw = tw + 24;
            if (bw < 120) bw = 120;
            if (bw > 200) bw = 200;
            if (x >= bx && x < bx + bw && y >= by && y < by + 22) {
                /* Act on PRESS only (kind==1). The same (x,y) arrives again
                 * as kind==2 (release); acting there too makes one click
                 * restore-then-immediately-reminimize (double-fire toggle). */
                if (kind != 1) return;
                if (w->flags & DOSGUI_WIN_MINIMIZED) {
                    w->flags &= ~DOSGUI_WIN_MINIMIZED;
                    dosgui_wm_set_focus(w);   /* restore also refocuses */
                } else if (g_dwm.focused_id == g_dwm.zorder[j]) {
                    w->flags |= DOSGUI_WIN_MINIMIZED;
                } else {
                    dosgui_wm_set_focus(w);
                }
                return;
            }
            bx += bw + 3;   /* MUST match render gap (taskbar.c) */
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

        /* Taskbar clock well (Win98 parity): clicking it opens/closes the
         * clock + calendar popup. */
        if (x >= g_dwm.clock_well_x && x < g_dwm.clock_well_x + g_dwm.clock_well_w &&
            y >= g_dwm.clock_well_y && y < g_dwm.clock_well_y + g_dwm.clock_well_h) {
            if (kind == 1) dosgui_clock_menu_toggle();
            return;
        }

        return;
    }

    /* Desktop mascot (WuBu Buddy): chrome-less element above windows.
     * Checked AFTER the taskbar region so Start button clicks aren't
     * swallowed by the mascot's lower-left bounding box. */
    if (wubu_bonzi_is_enabled() && wubu_bonzi_mouse(x, y, btn, kind))
        return;

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
            /* Left press on empty desktop: start the rubber-band
             * drag-select lasso (Classic Mac / Win98 lesson). */
            g_dwm.lasso_active = true;
            g_dwm.lasso_x0 = x; g_dwm.lasso_y0 = y;
            g_dwm.lasso_x1 = x; g_dwm.lasso_y1 = y;
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

        /* Edge/corner resize (Chicago/Mac baseline) -- takes priority over
         * title-bar drag when the grab is on a window border. */
        if (!(w->flags & DOSGUI_WIN_MAXIMIZED)) {
            int e = hit_test_edge(w, x, y);
            if (e) {
                g_dwm.resize_id = i;
                g_dwm.resize_edge = e;
                g_dwm.resize_ox = x;
                g_dwm.resize_oy = y;
                g_dwm.resize_ow = w->w;
                g_dwm.resize_oh = w->h;
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
        if (g_dwm.resize_id >= 0) g_dwm.resize_id = -1;
        if (g_dwm.drag_id >= 0 && g_dwm.windows[g_dwm.drag_id].alive) {
            DosGuiWindow *w = &g_dwm.windows[g_dwm.drag_id];
            /* Apply GAAD snap on drag end */
            snap_window_to_gaad(w);
        }
        g_dwm.drag_id = -1;
        if (g_dwm.drag_icon_id >= 0) {
            snap_icon_to_grid(&g_dwm.icons[g_dwm.drag_icon_id]);
            dosgui_wm_save_icon_layout();   /* persist position (ReactOS-style) */
            g_dwm.drag_icon_id = -1;
        }
        /* Release the lasso: select every icon the rubber band covers. */
        if (g_dwm.lasso_active) {
            dosgui_icon_select_in_rect(g_dwm.lasso_x0, g_dwm.lasso_y0,
                                       g_dwm.lasso_x1, g_dwm.lasso_y1);
            g_dwm.lasso_active = false;
        }
    } else if (kind == 0) {
        /* Live lasso update: stretch the rubber band with the cursor. */
        if (g_dwm.lasso_active) {
            g_dwm.lasso_x1 = x;
            g_dwm.lasso_y1 = y;
        }
        if (g_dwm.resize_id >= 0 && g_dwm.windows[g_dwm.resize_id].alive) {
            DosGuiWindow *w = &g_dwm.windows[g_dwm.resize_id];
            int dx = x - g_dwm.resize_ox;
            int dy = y - g_dwm.resize_oy;
            int e = g_dwm.resize_edge;
            int min_w = 80, min_h = 40;
            if (e & 1) {  /* left */
                int nw = g_dwm.resize_ow - dx;
                if (nw >= min_w) { w->x = g_dwm.resize_ox - (g_dwm.resize_ow - nw); w->w = nw; }
            }
            if (e & 2) {  /* right */
                int nw = g_dwm.resize_ow + dx;
                if (nw >= min_w) w->w = nw;
            }
            if (e & 4) {  /* top */
                int nh = g_dwm.resize_oh - dy;
                if (nh >= min_h) { w->y = g_dwm.resize_oy - (g_dwm.resize_oh - nh); w->h = nh; }
            }
            if (e & 8) {  /* bottom */
                int nh = g_dwm.resize_oh + dy;
                if (nh >= min_h) w->h = nh;
            }
        } else if (g_dwm.drag_id >= 0 && g_dwm.windows[g_dwm.drag_id].alive) {
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
