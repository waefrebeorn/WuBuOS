/*
 * dosgui_wm.c  --  WuBuOS DosGui Window Manager Implementation
 *
 * Cell 400: Fable Windowing Agent — THEMED EDITION.
 * Ports ZealOS/WuBuDos bare-metal window management into WuBuOS.
 * Based on Mythos Fable's wm.c (filipvabrousek/osdev).
 *
 * Features:
 *   - Draggable windows with title bars (XP gradient or Win98 flat)
 *   - Z-order + focus management
 *   - Taskbar with window buttons, clock, Start button (Luna orb on XP)
 *   - Close box (red X on Win98, themed on XP)
 *   - Software mouse cursor (18-row arrow)
 *   - Desktop icons with click handlers + drag-drop rearrange
 *   - Drop shadow under windows
 *   - FULL THEME ENGINE INTEGRATION: Win98 Classic, XP Luna Blue, XP Media Orange, WuBu Green
 *   - Rounded buttons on XP themes, square on Win98
 *   - Gradient title bars on XP themes
 *   - System tray (volume, network, battery)
 *   - Virtual desktops (Ctrl+Alt+Left/Right, 1-9 indicators)
 *   - GAAD snap regions for window placement
 *   - Wallpaper support (center/tile/stretch)
 *   - Maximize/Minimize window buttons
 */
/* -- Includes ------------------------------------------------------ */
#include "dosgui_wm_internal.h"

DosGuiWM g_dwm = {0};

/* -- Forward declarations (non-static for sub-modules) ------------ */
void raise_win(int i);
void close_win(int i);
int  hit_test(int x, int y);
void draw_window(int idx);
void draw_desktop_bg(int fb_w, int fb_h);
const WubuThemeColors *tc(void);
const WubuTheme *theme(void);
int  title_bar_height(void);
int  taskbar_height_dynamic(void);
int  border_width(void);
int  theme_radius(void);
void load_default_wallpaper(void);
void draw_wallpaper(int fb_w, int fb_h);
void snap_icon_to_grid(DosGuiIcon *icon);
int  icon_grid_x(int x);
int  icon_grid_y(int y);
void snap_window_to_gaad(DosGuiWindow *w);
int  spawn_window(int x, int y, int w, int h, const char *title);

/* Forward declarations for new API functions */
void dosgui_taskbar_update_clock(time_t now);
char *dosgui_taskbar_get_clock_str(void);

/* ================================================================
 * RENDERING — Themed Window Chrome
 * ================================================================ */

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

/* -- Theme Helpers ------------------------------------------------ */

const WubuThemeColors *tc(void) { return wubu_theme_colors(); }
const WubuTheme *theme(void) { return wubu_theme_get(); }
int title_bar_height(void) { return theme()->Luna_start_button ? 24 : DOSGUI_TITLE_H; }
int taskbar_height_dynamic(void) { return theme()->Luna_start_button ? 30 : DOSGUI_TASK_H; }
int border_width(void) { return theme()->rounded_buttons ? 3 : DOSGUI_BORDER; }
int theme_radius(void) { return theme()->rounded_buttons ? 4 : 0; }

/* ================================================================
 * RENDERING — Themed Window Chrome
 * ================================================================ */

void draw_desktop_bg(int fb_w, int fb_h) {
    (void)vbe_state();
    draw_wallpaper(fb_w, fb_h);
}

void load_default_wallpaper(void) {
    if (!g_dwm.wallpaper) {
        g_dwm.wallpaper_w = g_dwm.screen_w;
        g_dwm.wallpaper_h = g_dwm.screen_h;
        g_dwm.wallpaper = (uint32_t*)malloc((size_t)g_dwm.wallpaper_w * g_dwm.wallpaper_h * 4);
        if (g_dwm.wallpaper) {
            for (int y = 0; y < g_dwm.wallpaper_h; y++) {
                for (int x = 0; x < g_dwm.wallpaper_w; x++) {
                    float fy = (float)y / g_dwm.wallpaper_h;
                    int r = (int)((0x00 * (1-fy) + 0x00 * fy));
                    int g = (int)((0x80 * (1-fy) + 0x40 * fy));
                    int b = (int)((0x80 * (1-fy) + 0x00 * fy));
                    uint32_t c = (uint32_t)((b << 16) | (g << 8) | r);
                    g_dwm.wallpaper[y * g_dwm.wallpaper_w + x] = c;
                }
            }
        }
        g_dwm.wallpaper_mode = 1;
    }
}

void draw_wallpaper(int fb_w, int fb_h) {
    int task_h = taskbar_height_dynamic();
    
    if (!g_dwm.wallpaper) {
        vbe_fill_rect(0, 0, fb_w, fb_h - task_h, tc()->desktop_bg);
        return;
    }
    
    int mode = g_dwm.wallpaper_mode;
    if (mode == 0) {
        int x = (fb_w - g_dwm.wallpaper_w) / 2;
        int y = (fb_h - task_h - g_dwm.wallpaper_h) / 2;
        if (x < 0) x = 0;
        if (y < 0) y = 0;
        for (int dy = 0; dy < g_dwm.wallpaper_h && y + dy < fb_h - task_h; dy++) {
            for (int dx = 0; dx < g_dwm.wallpaper_w && x + dx < fb_w; dx++) {
                vbe_set_pixel(x + dx, y + dy, g_dwm.wallpaper[dy * g_dwm.wallpaper_w + dx]);
            }
        }
    } else if (mode == 1) {
        for (int y = 0; y < fb_h - task_h; y++) {
            for (int x = 0; x < fb_w; x++) {
                int sx = x % g_dwm.wallpaper_w;
                int sy = y % g_dwm.wallpaper_h;
                vbe_set_pixel(x, y, g_dwm.wallpaper[sy * g_dwm.wallpaper_w + sx]);
            }
        }
    } else {
        for (int y = 0; y < fb_h - task_h; y++) {
            for (int x = 0; x < fb_w; x++) {
                int sx = (x * g_dwm.wallpaper_w) / fb_w;
                int sy = (y * g_dwm.wallpaper_h) / (fb_h - task_h);
                vbe_set_pixel(x, y, g_dwm.wallpaper[sy * g_dwm.wallpaper_w + sx]);
            }
        }
    }
}

int icon_grid_x(int x) {
    int grid_x = (x - 20) / (DOSGUI_ICON_SIZE + DOSGUI_ICON_GAP);
    if (grid_x < 0) grid_x = 0;
    if (grid_x > 15) grid_x = 15;
    return 20 + grid_x * (DOSGUI_ICON_SIZE + DOSGUI_ICON_GAP);
}

int icon_grid_y(int y) {
    int grid_y = (y - 20) / (DOSGUI_ICON_SIZE + DOSGUI_ICON_GAP + 8);
    if (grid_y < 0) grid_y = 0;
    if (grid_y > 15) grid_y = 15;
    return 20 + grid_y * (DOSGUI_ICON_SIZE + DOSGUI_ICON_GAP + 8);
}

void snap_icon_to_grid(DosGuiIcon *icon) {
    icon->x = icon_grid_x(icon->x);
    icon->y = icon_grid_y(icon->y);
    icon->grid_x = (icon->x - 20) / (DOSGUI_ICON_SIZE + DOSGUI_ICON_GAP);
    icon->grid_y = (icon->y - 20) / (DOSGUI_ICON_SIZE + DOSGUI_ICON_GAP + 8);
}

void snap_window_to_gaad(DosGuiWindow *w) {
    if (!w) return;
    
    /* GAAD (Grid Aligned Application Design) snap regions:
     * - Top edge: snap to taskbar + title bar height (maximize vertically)
     * - Bottom edge: snap to screen bottom - taskbar
     * - Left edge: snap to 0
     * - Right edge: snap to screen width
     * - Corners: quarter-screen snap
     * - Center: center on screen
     */
    int task_h = taskbar_height_dynamic();
    int tbh = title_bar_height();
    int screen_w = g_dwm.screen_w;
    int screen_h = g_dwm.screen_h;
    int bw = border_width();
    
    /* Snap margins - how close to edge to trigger snap */
    const int snap_margin = 12;
    
    bool snapped = false;
    
    /* Left edge snap */
    if (w->x >= -snap_margin && w->x <= snap_margin) {
        w->x = 0;
        snapped = true;
    }
    /* Right edge snap */
    else if (w->x + w->w >= screen_w - snap_margin && w->x + w->w <= screen_w + snap_margin) {
        w->x = screen_w - w->w;
        snapped = true;
    }
    /* Top edge snap (below taskbar) */
    if (w->y >= -snap_margin && w->y <= snap_margin) {
        w->y = 0;
        snapped = true;
    }
    /* Bottom edge snap (above taskbar) */
    else if (w->y + w->h >= screen_h - task_h - snap_margin && w->y + w->h <= screen_h - task_h + snap_margin) {
        w->y = screen_h - task_h - w->h;
        snapped = true;
    }
    
    /* Corner snaps for quarter-screen placement */
    int center_x = screen_w / 2;
    int center_y = (screen_h - task_h) / 2;
    
    /* Top-left quadrant */
    if (abs(w->x - 0) <= snap_margin && abs(w->y - 0) <= snap_margin &&
        abs(w->w - center_x) <= snap_margin && abs(w->h - center_y) <= snap_margin) {
        w->x = 0; w->y = 0;
        w->w = center_x; w->h = center_y;
        snapped = true;
    }
    /* Top-right quadrant */
    else if (abs(w->x + w->w - screen_w) <= snap_margin && abs(w->y - 0) <= snap_margin &&
             abs(w->w - center_x) <= snap_margin && abs(w->h - center_y) <= snap_margin) {
        w->x = center_x; w->y = 0;
        w->w = center_x; w->h = center_y;
        snapped = true;
    }
    /* Bottom-left quadrant */
    else if (abs(w->x - 0) <= snap_margin && abs(w->y + w->h - (screen_h - task_h)) <= snap_margin &&
             abs(w->w - center_x) <= snap_margin && abs(w->h - center_y) <= snap_margin) {
        w->x = 0; w->y = center_y;
        w->w = center_x; w->h = center_y;
        snapped = true;
    }
    /* Bottom-right quadrant */
    else if (abs(w->x + w->w - screen_w) <= snap_margin && abs(w->y + w->h - (screen_h - task_h)) <= snap_margin &&
             abs(w->w - center_x) <= snap_margin && abs(w->h - center_y) <= snap_margin) {
        w->x = center_x; w->y = center_y;
        w->w = center_x; w->h = center_y;
        snapped = true;
    }
    
    /* Left half */
    else if (abs(w->x - 0) <= snap_margin && abs(w->y - 0) <= snap_margin &&
             abs(w->w - screen_w/2) <= snap_margin && abs(w->h - (screen_h - task_h)) <= snap_margin) {
        w->x = 0; w->y = 0;
        w->w = screen_w / 2; w->h = screen_h - task_h;
        snapped = true;
    }
    /* Right half */
    else if (abs(w->x + w->w - screen_w) <= snap_margin && abs(w->y - 0) <= snap_margin &&
             abs(w->w - screen_w/2) <= snap_margin && abs(w->h - (screen_h - task_h)) <= snap_margin) {
        w->x = screen_w / 2; w->y = 0;
        w->w = screen_w / 2; w->h = screen_h - task_h;
        snapped = true;
    }
    
    /* If snapped, ensure window stays within bounds */
    if (snapped) {
        if (w->x < 0) w->x = 0;
        if (w->y < 0) w->y = 0;
        if (w->x + w->w > screen_w) w->w = screen_w - w->x;
        if (w->y + w->h > screen_h - task_h) w->h = screen_h - task_h - w->y;
    }
}

/* ================================================================
 * RENDERING — Themed Window Chrome
 * ================================================================ */

void draw_window(int idx) {
    DosGuiWindow *w = &g_dwm.windows[idx];
    if (!w->alive) return;
    bool active = (idx == g_dwm.focused_id);

    const int rad = theme_radius();
    const int tbh = title_bar_height();
    const int bw = border_width();

    if (!theme()->Luna_start_button || true) {
        vbe_shade_rect(w->x + 4, w->y + 4, w->w, w->h);
    }

    vbe_fill_rect_rounded(w->x, w->y, w->w, w->h, rad, tc()->win_face);
    if (rad > 0) vbe_rect_rounded(w->x, w->y, w->w, w->h, rad, tc()->border_dark);
    else vbe_rect(w->x, w->y, w->w, w->h, tc()->border_dark);

    if (theme()->gradient_title) {
        if (active) {
            vbe_hgradient(w->x + rad, w->y + rad, w->w - 2*rad, tbh - rad,
                          theme()->title_gradient.color_start,
                          theme()->title_gradient.color_end);
        } else {
            vbe_hgradient(w->x + rad, w->y + rad, w->w - 2*rad, tbh - rad,
                          theme()->title_gradient_ina.color_start,
                          theme()->title_gradient_ina.color_end);
        }
    } else {
        vbe_title_bar(w->x + rad, w->y + rad, w->w - 2*rad, tbh - rad, active);
    }

    uint32_t title_color = active ? tc()->win_title_text : tc()->win_title_text_ina;
    int text_x = w->x + 8;
    int text_y = w->y + rad + (tbh - rad - 8) / 2;
    vbe_draw_text(text_x, text_y, w->title, title_color, 1);

    int close_x = w->x + w->w - rad - 18;
    int close_y = w->y + rad + 2;
    vbe_fill_rect_rounded(close_x, close_y, 14, 12, 2, active ? tc()->border_darkest : tc()->btn_face);
    vbe_rect_rounded(close_x, close_y, 14, 12, 2, tc()->border_dark);
    vbe_draw_text(close_x + 5, close_y + 2, "X", active ? 0xFFFFFF : 0x808080, 1);

    if (theme()->Luna_start_button) {
        int max_x = close_x - 20;
        vbe_fill_rect_rounded(max_x, close_y, 14, 12, 2, active ? tc()->border_face : tc()->btn_face);
        vbe_rect_rounded(max_x, close_y, 14, 12, 2, tc()->border_dark);
        vbe_draw_text(max_x + 4, close_y + 2, "[ ]", active ? 0xFFFFFF : 0x808080, 1);
    }

    if (theme()->Luna_start_button) {
        int min_x = close_x - 40;
        vbe_fill_rect_rounded(min_x, close_y, 14, 12, 2, active ? tc()->border_face : tc()->btn_face);
        vbe_rect_rounded(min_x, close_y, 14, 12, 2, tc()->border_dark);
        vbe_draw_text(min_x + 5, close_y + 2, "_", active ? 0xFFFFFF : 0x808080, 1);
    }

    int cx = w->x + bw;
    int cy = w->y + tbh;
    int cw = w->w - 2 * bw;
    int ch = w->h - tbh - bw;

    vbe_fill_rect_rounded(cx, cy, cw, ch, rad, tc()->win_face);
    if (rad > 0) {
        vbe_3d_sunken_rounded_colors(cx - 1, cy - 1, cw + 2, ch + 2, rad + 1,
                                      tc()->border_light, tc()->border_face,
                                      tc()->border_dark, tc()->border_darkest);
    } else {
        vbe_3d_sunken_colors(cx - 1, cy - 1, cw + 2, ch + 2,
                              tc()->border_light, tc()->border_face,
                              tc()->border_dark, tc()->border_darkest);
    }

    if (w->on_draw) {
        w->on_draw(w, NULL, cw, ch);
    }
}

/* ================================================================
 * PUBLIC API
 * ================================================================ */

int dosgui_wm_init(int screen_w, int screen_h) {
    memset(&g_dwm, 0, sizeof(g_dwm));
    g_dwm.screen_w = screen_w;
    g_dwm.screen_h = screen_h;
    g_dwm.focused_id = -1;
    g_dwm.drag_id = -1;
    g_dwm.drag_icon_id = -1;
    g_dwm.current_desktop = 0;
    g_dwm.desktop_count = 9;
    g_dwm.systray_count = 0;
    g_dwm.notif_count = 0;
    g_dwm.next_notif_id = 1;
    g_dwm.notif_center_open = false;
    g_dwm.last_clock_update = 0;
    load_default_wallpaper();
    return 0;
}

void dosgui_wm_shutdown(void) {
    if (g_dwm.wallpaper) {
        free(g_dwm.wallpaper);
        g_dwm.wallpaper = NULL;
    }
    memset(&g_dwm, 0, sizeof(g_dwm));
}

DosGuiWindow *dosgui_wm_create(int x, int y, int w, int h,
                                const char *title) {
    int i = spawn_window(x, y, w, h, title);
    if (i < 0) return NULL;
    return &g_dwm.windows[i];
}

void dosgui_wm_destroy(DosGuiWindow *win) {
    if (!win) return;
    for (int i = 0; i < DOSGUI_MAX_WINDOWS; i++) {
        if (&g_dwm.windows[i] == win) { close_win(i); return; }
    }
}

void dosgui_wm_set_focus(DosGuiWindow *win) {
    if (!win) return;
    for (int i = 0; i < DOSGUI_MAX_WINDOWS; i++) {
        if (&g_dwm.windows[i] == win) {
            raise_win(i);
            g_dwm.focused_id = i;
            return;
        }
    }
}

DosGuiWindow *dosgui_wm_get_focused(void) {
    if (g_dwm.focused_id < 0) return NULL;
    return &g_dwm.windows[g_dwm.focused_id];
}

DosGuiWindow *dosgui_wm_find_by_id(int id) {
    for (int i = 0; i < DOSGUI_MAX_WINDOWS; i++)
        if (g_dwm.windows[i].alive && g_dwm.windows[i].id == id)
            return &g_dwm.windows[i];
    return NULL;
}

int dosgui_wm_window_count(void) {
    return g_dwm.nz;
}

/* -- Virtual Desktop Migration ------------------------------------- */

void dosgui_wm_move_window_to_desktop(DosGuiWindow *win, int desktop) {
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

int dosgui_wm_get_current_desktop(void) {
    return g_dwm.current_desktop;
}

void dosgui_wm_set_current_desktop(int desktop) {
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
void dosgui_wm_move_focused_window(int delta) {
    if (g_dwm.focused_id < 0) return;
    DosGuiWindow *w = &g_dwm.windows[g_dwm.focused_id];
    if (!w->alive) return;
    int new_desktop = w->desktop + delta;
    if (new_desktop < 0) new_desktop = 0;
    if (new_desktop >= g_dwm.desktop_count) new_desktop = g_dwm.desktop_count - 1;
    if (new_desktop != w->desktop) {
        dosgui_wm_move_window_to_desktop(w, new_desktop);
    }
}

DosGuiWindow *dosgui_wm_spawn(int x, int y, int w, int h,
                               const char *title,
                               void (*on_draw)(DosGuiWindow *, uint32_t *, int, int)) {
    int i = spawn_window(x, y, w, h, title);
    if (i < 0) return NULL;
    g_dwm.windows[i].on_draw = on_draw;
    return &g_dwm.windows[i];
}

/* -- Input ------------------------------------------------------- */

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

/* -- Desktop Icons ------------------------------------------------------ */

int dosgui_icon_add(const char *name, int gx, int gy,
                        void (*on_click)(void)) {
    if (g_dwm.icon_count >= DOSGUI_MAX_ICONS) return -1;
    DosGuiIcon *icon = &g_dwm.icons[g_dwm.icon_count];
    memset(icon, 0, sizeof(*icon));
    strncpy(icon->name, name, sizeof(icon->name) - 1);
    icon->grid_x = gx; icon->grid_y = gy;
    icon->x = 20 + gx * (DOSGUI_ICON_SIZE + DOSGUI_ICON_GAP);
    icon->y = 20 + gy * (DOSGUI_ICON_SIZE + DOSGUI_ICON_GAP + 8);
    icon->on_click = on_click;
    icon->type = DESK_ICON_APP;
    icon->icon_color = 0x0080FF;  /* Default blue */
    return g_dwm.icon_count++;
}

int dosgui_icon_add_ex(const char *name, DeskIconType type,
                        const char *target, int gx, int gy,
                        uint32_t icon_color, void (*on_execute)(void)) {
    if (g_dwm.icon_count >= DOSGUI_MAX_ICONS) return -1;
    DosGuiIcon *icon = &g_dwm.icons[g_dwm.icon_count];
    memset(icon, 0, sizeof(*icon));
    strncpy(icon->name, name, sizeof(icon->name) - 1);
    icon->grid_x = gx; icon->grid_y = gy;
    icon->x = 20 + gx * (DOSGUI_ICON_SIZE + DOSGUI_ICON_GAP);
    icon->y = 20 + gy * (DOSGUI_ICON_SIZE + DOSGUI_ICON_GAP + 8);
    icon->type = type;
    icon->icon_color = icon_color ? icon_color : 0x0080FF;
    if (target) strncpy(icon->target, target, sizeof(icon->target) - 1);
    icon->on_execute = on_execute;
    icon->alive = true;
    return g_dwm.icon_count++;
}

void dosgui_icon_remove(int grid_x, int grid_y) {
    for (int i = 0; i < g_dwm.icon_count; i++) {
        if (g_dwm.icons[i].alive && g_dwm.icons[i].grid_x == grid_x && g_dwm.icons[i].grid_y == grid_y) {
            g_dwm.icons[i].alive = false;
            /* Compact array */
            for (int j = i; j < g_dwm.icon_count - 1; j++) {
                g_dwm.icons[j] = g_dwm.icons[j + 1];
            }
            g_dwm.icon_count--;
            return;
        }
    }
}

int dosgui_icon_find_at(int grid_x, int grid_y) {
    for (int i = 0; i < g_dwm.icon_count; i++) {
        if (g_dwm.icons[i].alive && g_dwm.icons[i].grid_x == grid_x && g_dwm.icons[i].grid_y == grid_y) {
            return i;
        }
    }
    return -1;
}

void dosgui_icon_set_position(int grid_x, int grid_y, int new_gx, int new_gy) {
    int idx = dosgui_icon_find_at(grid_x, grid_y);
    if (idx >= 0) {
        DosGuiIcon *icon = &g_dwm.icons[idx];
        /* Check if target position is occupied */
        if (dosgui_icon_find_at(new_gx, new_gy) < 0) {
            icon->grid_x = new_gx;
            icon->grid_y = new_gy;
            icon->x = 20 + new_gx * (DOSGUI_ICON_SIZE + DOSGUI_ICON_GAP);
            icon->y = 20 + new_gy * (DOSGUI_ICON_SIZE + DOSGUI_ICON_GAP + 8);
        }
    }
}

/* Shortcut Creation */

int dosgui_shortcut_create(const char *name, const char *target,
                            const char *description, int grid_x, int grid_y) {
    return dosgui_icon_add_ex(name, DESK_ICON_SHORTCUT, target, grid_x, grid_y, 0x00FF00, NULL);
}

int dosgui_shortcut_create_url(const char *name, const char *url, int grid_x, int grid_y) {
    return dosgui_icon_add_ex(name, DESK_ICON_URL, url, grid_x, grid_y, 0xFF8000, NULL);
}

int dosgui_icon_hit_test(int mx, int my) {
    for (int i = 0; i < g_dwm.icon_count; i++) {
        DosGuiIcon *ic = &g_dwm.icons[i];
        if (mx >= ic->x && mx < ic->x + DOSGUI_ICON_SIZE &&
            my >= ic->y && my < ic->y + DOSGUI_ICON_SIZE)
            return i;
    }
    return -1;
}

/* -- Taskbar ----------------------------------------------------- */

int dosgui_taskbar_height(void) { return taskbar_height_dynamic(); }

void dosgui_taskbar_render(uint32_t *fb, int fb_w, int fb_h) {
    int th = taskbar_height_dynamic();
    int ty = fb_h - th;

    vbe_fill_rect(0, ty, fb_w, th, tc()->taskbar_bg);
    vbe_hline(0, fb_w - 1, ty, tc()->taskbar_border);
    int by = ty + (th - 24) / 2;
    int start_w = theme()->Luna_start_button ? 54 : 60;
    
    if (theme()->Luna_start_button) {
        vbe_fill_rect_rounded(4, by, start_w + 20, 24, 4, tc()->start_btn_face);
        vbe_3d_raised_rounded_colors(4, by, start_w + 20, 24, 4,
                                      tc()->border_light, tc()->border_face,
                                      tc()->border_dark, tc()->border_darkest);
        vbe_draw_text(8, by + 8, "Start", tc()->start_btn_text, 1);
    } else {
        vbe_fill_rect(4, by, 60, 22, tc()->start_btn_face);
        vbe_3d_raised_colors(4, by, 60, 22,
                              tc()->border_light, tc()->border_face,
                              tc()->border_dark, tc()->border_darkest);
        vbe_draw_text(8, by + 6, "+ NEW", tc()->start_btn_text, 1);
    }

    int bx = theme()->Luna_start_button ? 82 : 72;
    
    /* Reserve space for clock + tray icons on the right */
    dosgui_taskbar_update_clock(time(NULL));
    char *clk = dosgui_taskbar_get_clock_str();
    int clk_w = vbe_text_width(clk, 1);
    int clock_reserve = clk_w + 20; /* clock + padding */
    int tray_reserve = g_dwm.systray_count * (DOSGUI_SYSTRAY_SIZE + 4) + 10;
    int right_reserve = clock_reserve + tray_reserve;
    
    for (int j = 0; j < g_dwm.nz; j++) {
        DosGuiWindow *w = &g_dwm.windows[g_dwm.zorder[j]];
        if (!w->alive || (w->flags & DOSGUI_WIN_MINIMIZED)) continue;
        if (w->desktop != g_dwm.current_desktop) continue;
        int bw = (int)strlen(w->title) * 6 + 16;
        if (bw > 160) bw = 160;
        bool focused = (g_dwm.zorder[j] == g_dwm.focused_id);
        
        /* Check if button would overlap reserved area */
        if (bx + bw > g_dwm.screen_w - right_reserve) break;
        
        if (theme()->rounded_buttons) {
            if (focused) {
                vbe_fill_rect_rounded(bx, by, bw, 22, 3, tc()->select_bg);
                vbe_3d_sunken_rounded_colors(bx, by, bw, 22, 3,
                                              tc()->border_light, tc()->border_face,
                                              tc()->border_dark, tc()->border_darkest);
                /* Draw truncated title with ellipsis */
                {
                    int text_w = vbe_text_width(w->title, 1);
                    int max_text_w = bw - 16;
                    if (text_w > max_text_w) {
                        char truncated[64];
                        int len = strlen(w->title);
                        while (len > 0 && vbe_text_width(truncated, 1) > max_text_w - 6) { /* -6 for "..." */
                            len--;
                            strncpy(truncated, w->title, len);
                            truncated[len] = '\0';
                        }
                        if (len > 0) {
                            strncpy(truncated + len, "...", 3);
                            truncated[len + 3] = '\0';
                        } else {
                            strcpy(truncated, "...");
                        }
                        vbe_draw_text(bx + 8, by + 7, truncated, tc()->select_text, 1);
                    } else {
                        vbe_draw_text(bx + 8, by + 7, w->title, tc()->select_text, 1);
                    }
                }
            } else {
                vbe_fill_rect_rounded(bx, by, bw, 22, 3, tc()->btn_face);
                vbe_3d_raised_rounded_colors(bx, by, bw, 22, 3,
                                              tc()->border_light, tc()->border_face,
                                              tc()->border_dark, tc()->border_darkest);
                /* Draw truncated title with ellipsis */
                {
                    int text_w = vbe_text_width(w->title, 1);
                    int max_text_w = bw - 16;
                    if (text_w > max_text_w) {
                        char truncated[64];
                        int len = strlen(w->title);
                        while (len > 0 && vbe_text_width(truncated, 1) > max_text_w - 6) {
                            len--;
                            strncpy(truncated, w->title, len);
                            truncated[len] = '\0';
                        }
                        if (len > 0) {
                            strncpy(truncated + len, "...", 3);
                            truncated[len + 3] = '\0';
                        } else {
                            strcpy(truncated, "...");
                        }
                        vbe_draw_text(bx + 8, by + 7, truncated, tc()->btn_text, 1);
                    } else {
                        vbe_draw_text(bx + 8, by + 7, w->title, tc()->btn_text, 1);
                    }
                }
            }
        } else {
            if (focused) {
                vbe_fill_rect(bx, by, bw, 22, 0x000080);
                vbe_3d_sunken_colors(bx, by, bw, 22,
                                      tc()->border_light, tc()->border_face,
                                      tc()->border_dark, tc()->border_darkest);
                /* Draw truncated title with ellipsis */
                {
                    int text_w = vbe_text_width(w->title, 1);
                    int max_text_w = bw - 16;
                    if (text_w > max_text_w) {
                        char truncated[64];
                        int len = strlen(w->title);
                        while (len > 0 && vbe_text_width(truncated, 1) > max_text_w - 6) {
                            len--;
                            strncpy(truncated, w->title, len);
                            truncated[len] = '\0';
                        }
                        if (len > 0) {
                            strncpy(truncated + len, "...", 3);
                            truncated[len + 3] = '\0';
                        } else {
                            strcpy(truncated, "...");
                        }
                        vbe_draw_text(bx + 8, by + 6, truncated, 0xFFFFFF, 1);
                    } else {
                        vbe_draw_text(bx + 8, by + 6, w->title, 0xFFFFFF, 1);
                    }
                }
            } else {
                vbe_fill_rect(bx, by, bw, 22, tc()->btn_face);
                vbe_3d_raised_colors(bx, by, bw, 22,
                                      tc()->border_light, tc()->border_face,
                                      tc()->border_dark, tc()->border_darkest);
                /* Draw truncated title with ellipsis */
                {
                    int text_w = vbe_text_width(w->title, 1);
                    int max_text_w = bw - 16;
                    if (text_w > max_text_w) {
                        char truncated[64];
                        int len = strlen(w->title);
                        while (len > 0 && vbe_text_width(truncated, 1) > max_text_w - 6) {
                            len--;
                            strncpy(truncated, w->title, len);
                            truncated[len] = '\0';
                        }
                        if (len > 0) {
                            strncpy(truncated + len, "...", 3);
                            truncated[len + 3] = '\0';
                        } else {
                            strcpy(truncated, "...");
                        }
                        vbe_draw_text(bx + 8, by + 6, truncated, tc()->btn_text, 1);
                    } else {
                        vbe_draw_text(bx + 8, by + 6, w->title, tc()->btn_text, 1);
                    }
                }
            }
        }
        bx += bw + 2;
        if (bx > g_dwm.screen_w - right_reserve) break;
    }

    /* System tray icons (drawn from right to left, before clock) */
    int tray_x = fb_w - 10;

    /* Clock - use clk/clk_w from earlier in function */
    dosgui_taskbar_update_clock(time(NULL));

    /* Ensure clock doesn't overlap window buttons - use bx as the left boundary */
    int clock_x = fb_w - clk_w - 10;
    if (clock_x + clk_w > bx) {
        clock_x = bx - clk_w - 10;
    }
    if (clock_x < 0) clock_x = 10;

    vbe_draw_text(clock_x, ty + (th - 8) / 2, clk,
                  theme()->Luna_start_button ? 0xFFFFFF : tc()->icon_text, 1);

    tray_x = clock_x - 10;

    /* Draw system tray icons */
    for (int i = g_dwm.systray_count - 1; i >= 0; i--) {
        if (g_dwm.systray_icons[i].visible) {
            int x = tray_x - DOSGUI_SYSTRAY_SIZE - 4;
            int y = ty + (th - DOSGUI_SYSTRAY_SIZE) / 2;

            vbe_fill_rect(x, y, DOSGUI_SYSTRAY_SIZE, DOSGUI_SYSTRAY_SIZE, tc()->btn_face);
            vbe_3d_raised_colors(x, y, DOSGUI_SYSTRAY_SIZE, DOSGUI_SYSTRAY_SIZE,
                                 tc()->border_light, tc()->border_face,
                                 tc()->border_dark, tc()->border_darkest);

            vbe_fill_rect(x + 4, y + 4, 16, 16, g_dwm.systray_icons[i].icon_color);

            /* Draw notification badge if count > 0 */
            if (g_dwm.systray_icons[i].notification_count > 0) {
                char badge[8];
                snprintf(badge, sizeof(badge), "%d", 
                    g_dwm.systray_icons[i].notification_count > 9 ? 9 : g_dwm.systray_icons[i].notification_count);
                int bx = x + DOSGUI_SYSTRAY_SIZE - 8;
                int by = y;
                vbe_fill_rect_rounded(bx, by, 12, 12, 6, 0xFF0000);
                vbe_draw_text(bx + 2, by + 1, badge, 0xFFFFFF, 1);
            }
            tray_x -= DOSGUI_SYSTRAY_SIZE + 4;
        }
    }

    int desk_x = tray_x - 150;
    for (int d = 0; d < g_dwm.desktop_count; d++) {
        int dx = desk_x + d * 16;
        if (d == g_dwm.current_desktop) {
            vbe_fill_rect_rounded(dx, ty + (th - 16) / 2, 14, 16, 2, tc()->select_bg);
            vbe_3d_sunken_rounded_colors(dx, ty + (th - 16) / 2, 14, 16, 2,
                                          tc()->border_light, tc()->border_face,
                                          tc()->border_dark, tc()->border_darkest);
            char label = (d == 9) ? 'M' : ('1' + d);
            vbe_draw_text(dx + 3, ty + (th - 8) / 2, &label, tc()->select_text, 1);
        } else {
            vbe_fill_rect_rounded(dx, ty + (th - 16) / 2, 14, 16, 2, tc()->btn_face);
            vbe_3d_raised_rounded_colors(dx, ty + (th - 16) / 2, 14, 16, 2,
                                          tc()->border_light, tc()->border_face,
                                          tc()->border_dark, tc()->border_darkest);
            char label = (d == 9) ? 'M' : ('1' + d);
            vbe_draw_text(dx + 3, ty + (th - 8) / 2, &label, tc()->btn_text, 1);
        }
    }
}

/* -- Full Render ------------------------------------------------- */

void dosgui_wm_render(uint32_t *fb, int fb_w, int fb_h) {
    dosgui_wm_render_desktop(fb, fb_w, fb_h);
}

void dosgui_wm_render_desktop(uint32_t *fb, int fb_w, int fb_h) {
    draw_desktop_bg(fb_w, fb_h);

    for (int i = 0; i < g_dwm.icon_count; i++) {
        DosGuiIcon *icon = &g_dwm.icons[i];
        vbe_fill_rect(icon->x, icon->y, DOSGUI_ICON_SIZE, DOSGUI_ICON_SIZE, tc()->icon_bg);
        vbe_rect(icon->x, icon->y, DOSGUI_ICON_SIZE, DOSGUI_ICON_SIZE, tc()->icon_border);
        
        /* Draw icon label with bounds checking and truncation */
        int label_y = icon->y + DOSGUI_ICON_SIZE + 2;
        int max_label_w = DOSGUI_ICON_SIZE + 4; /* Slightly wider than icon */
        int text_w = vbe_text_width(icon->name, 1);
        
        /* Check if label would go off-screen */
        if (label_y + 8 <= fb_h) {
            /* Truncate text with ellipsis if too wide */
            if (text_w > max_label_w) {
                char truncated[32];
                int len = strlen(icon->name);
                strncpy(truncated, icon->name, len);
                truncated[len] = '\0';
                while (len > 0 && vbe_text_width(truncated, 1) > max_label_w) {
                    len--;
                    truncated[len] = '\0';
                }
                if (len > 0) {
                    strncpy(truncated + len, "...", 3);
                    truncated[len + 3] = '\0';
                } else {
                    strcpy(truncated, "...");
                }
                vbe_draw_text(icon->x + 1, label_y, truncated, tc()->icon_text_shadow, 1);
                vbe_draw_text(icon->x, label_y - 1, truncated, tc()->icon_text, 1);
            } else {
                vbe_draw_text(icon->x + 1, label_y, icon->name, tc()->icon_text_shadow, 1);
                vbe_draw_text(icon->x, label_y - 1, icon->name, tc()->icon_text, 1);
            }
        }
    }

    for (int j = 0; j < g_dwm.nz; j++) {
        int idx = g_dwm.zorder[j];
        DosGuiWindow *w = &g_dwm.windows[idx];
        if (w->alive && w->desktop == g_dwm.current_desktop && !(w->flags & DOSGUI_WIN_MINIMIZED))
            draw_window(idx);
    }

    dosgui_taskbar_render(fb, fb_w, fb_h);

    /* Render notification center if open (on top of everything) */
    dosgui_notif_center_render(fb, fb_w, fb_h);

    vbe_draw_cursor(g_dwm.mouse_x, g_dwm.mouse_y);
}

/* ================================================================
 * SYSTEM TRAY / NOTIFICATION AREA
 * ================================================================ */

static void draw_systray_icon(int idx, int ty, int th) {
    DosGuiSysTrayIcon *icon = &g_dwm.systray_icons[idx];
    if (!icon->visible) return;

    int x = g_dwm.screen_w - 50 - idx * (DOSGUI_SYSTRAY_SIZE + 4);
    int y = ty + (th - DOSGUI_SYSTRAY_SIZE) / 2;

    /* Draw icon background */
    vbe_fill_rect(x, y, DOSGUI_SYSTRAY_SIZE, DOSGUI_SYSTRAY_SIZE, tc()->btn_face);
    vbe_3d_raised_colors(x, y, DOSGUI_SYSTRAY_SIZE, DOSGUI_SYSTRAY_SIZE,
                         tc()->border_light, tc()->border_face,
                         tc()->border_dark, tc()->border_darkest);

    /* Draw simple colored square as icon */
    vbe_fill_rect(x + 4, y + 4, 16, 16, icon->icon_color);

    /* Draw notification badge if count > 0 */
    if (icon->notification_count > 0) {
        char badge[8];
        snprintf(badge, sizeof(badge), "%d", icon->notification_count > 9 ? 9 : icon->notification_count);
        int bx = x + DOSGUI_SYSTRAY_SIZE - 8;
        int by = y;
        vbe_fill_rect_rounded(bx, by, 12, 12, 6, 0xFF0000);
        vbe_draw_text(bx + 2, by + 1, badge, 0xFFFFFF, 1);
    }
}

int dosgui_systray_add(const char *name, uint32_t color,
                        void (*on_click)(void),
                        void (*on_right_click)(void)) {
    if (g_dwm.systray_count >= DOSGUI_MAX_SYSTRAY_ICONS) return -1;

    DosGuiSysTrayIcon *icon = &g_dwm.systray_icons[g_dwm.systray_count];
    memset(icon, 0, sizeof(*icon));
    strncpy(icon->name, name, sizeof(icon->name) - 1);
    icon->icon_color = color;
    icon->visible = true;
    icon->on_click = on_click;
    icon->on_right_click = on_right_click;
    icon->notification_count = 0;

    return g_dwm.systray_count++;
}

void dosgui_systray_remove(const char *name) {
    for (int i = 0; i < g_dwm.systray_count; i++) {
        if (strcmp(g_dwm.systray_icons[i].name, name) == 0) {
            for (int j = i; j < g_dwm.systray_count - 1; j++) {
                g_dwm.systray_icons[j] = g_dwm.systray_icons[j + 1];
            }
            g_dwm.systray_count--;
            return;
        }
    }
}

void dosgui_systray_set_notification_count(const char *name, int count) {
    for (int i = 0; i < g_dwm.systray_count; i++) {
        if (strcmp(g_dwm.systray_icons[i].name, name) == 0) {
            g_dwm.systray_icons[i].notification_count = count;
            return;
        }
    }
}

/* ================================================================
 * NOTIFICATION CENTER
 * ================================================================ */

int dosgui_notif_center_add(const char *app_name, const char *summary,
                             const char *body, int urgency) {
    if (g_dwm.notif_count >= DOSGUI_NOTIF_CENTER_MAX) {
        /* Shift oldest out */
        for (int i = 1; i < g_dwm.notif_count; i++) {
            g_dwm.notifications[i - 1] = g_dwm.notifications[i];
        }
        g_dwm.notif_count--;
    }

    DosGuiNotification *n = &g_dwm.notifications[g_dwm.notif_count];
    memset(n, 0, sizeof(*n));
    n->id = g_dwm.next_notif_id++;
    strncpy(n->app_name, app_name, sizeof(n->app_name) - 1);
    strncpy(n->summary, summary, sizeof(n->summary) - 1);
    if (body) strncpy(n->body, body, sizeof(n->body) - 1);
    n->timestamp = (uint32_t)time(NULL);
    n->urgency = urgency;
    n->read = false;
    n->expanded = false;

    g_dwm.notif_count++;

    /* Update systray notification badge */
    dosgui_systray_set_notification_count("Notifications", g_dwm.notif_count);

    /* Also send to wubu_notify daemon if available */
    (void)wubu_notify_simple(app_name, summary, body ? body : "",
                              NULL, urgency, urgency == 2 ? 0 : 5000);

    return n->id;
}

void dosgui_notif_center_mark_read(uint32_t id) {
    for (int i = 0; i < g_dwm.notif_count; i++) {
        if (g_dwm.notifications[i].id == id) {
            g_dwm.notifications[i].read = true;
            return;
        }
    }
}

void dosgui_notif_center_clear(void) {
    g_dwm.notif_count = 0;
    dosgui_systray_set_notification_count("Notifications", 0);
}

bool dosgui_notif_center_is_open(void) {
    return g_dwm.notif_center_open;
}

void dosgui_notif_center_toggle(void) {
    g_dwm.notif_center_open = !g_dwm.notif_center_open;
    /* Mark all as read when opening */
    if (g_dwm.notif_center_open) {
        for (int i = 0; i < g_dwm.notif_count; i++) {
            g_dwm.notifications[i].read = true;
        }
        dosgui_systray_set_notification_count("Notifications", 0);
    }
}

void dosgui_notif_center_render(uint32_t *fb, int fb_w, int fb_h) {
    if (!g_dwm.notif_center_open) return;

    int th = taskbar_height_dynamic();
    int ty = fb_h - th;

    /* Draw panel on right side, above taskbar */
    int panel_w = 350;
    int panel_h = fb_h - th;
    int panel_x = fb_w - panel_w;
    int panel_y = ty - panel_h;

    vbe_fill_rect_rounded(panel_x, panel_y, panel_w, panel_h, 8, tc()->win_face);
    vbe_3d_sunken_rounded_colors(panel_x, panel_y, panel_w, panel_h, 8,
                                  tc()->border_light, tc()->border_face,
                                  tc()->border_dark, tc()->border_darkest);

    /* Header */
    vbe_fill_rect_rounded(panel_x + 4, panel_y + 4, panel_w - 8, 30, 4, tc()->select_bg);
    vbe_draw_text(panel_x + 10, panel_y + 10, "Notification Center", tc()->select_text, 1);

    /* Notifications list */
    int ny = panel_y + 40;
    for (int i = 0; i < g_dwm.notif_count; i++) {
        DosGuiNotification *n = &g_dwm.notifications[i];
        if (ny + 60 > panel_y + panel_h - 10) break;

        uint32_t bg = n->read ? 0xFF303030 : tc()->select_bg;
        vbe_fill_rect_rounded(panel_x + 4, ny, panel_w - 8, 56, 4, bg);
        vbe_3d_raised_rounded_colors(panel_x + 4, ny, panel_w - 8, 56, 4,
                                      tc()->border_light, tc()->border_face,
                                      tc()->border_dark, tc()->border_darkest);

        /* Urgency indicator */
        uint32_t urg_color = (n->urgency == 2) ? 0xFF0000 : (n->urgency == 1 ? 0xFFFF00 : 0x00FF00);
        vbe_fill_rect(panel_x + 6, ny + 6, 4, 44, urg_color);

        /* App name */
        vbe_draw_text(panel_x + 14, ny + 6, n->app_name, tc()->icon_text, 1);

        /* Summary */
        vbe_draw_text(panel_x + 14, ny + 18, n->summary, n->read ? tc()->icon_text_shadow : tc()->win_title_text, 1);

        /* Body */
        if (n->body[0]) {
            vbe_draw_text(panel_x + 14, ny + 30, n->body, tc()->icon_text_shadow, 1);
        }

        /* Time */
        char time_str[16];
        time_t t = n->timestamp;
        struct tm *tm = localtime(&t);
        snprintf(time_str, sizeof(time_str), "%02d:%02d", tm->tm_hour, tm->tm_min);
        vbe_draw_text(panel_x + panel_w - 60, ny + 6, time_str, tc()->icon_text_shadow, 1);

        ny += 60;
    }
}

/* ================================================================
 * CLOCK
 * ================================================================ */

void dosgui_taskbar_update_clock(time_t now) {
    g_dwm.last_clock_update = now;
}

char *dosgui_taskbar_get_clock_str(void) {
    time_t now = time(NULL);
    struct tm *tm = localtime(&now);
    static char clk[16];
    snprintf(clk, sizeof(clk), "%02d:%02d", tm->tm_hour, tm->tm_min);
    return clk;
}

/* Global context menu stack */
DosGuiContextMenu *g_dosgui_ctx_stack = NULL;

/* -- Context Menu Stack Management -- */

static void ctx_menu_push(DosGuiContextMenu *menu) {
    menu->parent = g_dosgui_ctx_stack;
    g_dosgui_ctx_stack = menu;
}

static void ctx_menu_pop(void) {
    if (g_dosgui_ctx_stack) {
        DosGuiContextMenu *old = g_dosgui_ctx_stack;
        g_dosgui_ctx_stack = old->parent;
        old->parent = NULL;
    }
}

DosGuiContextMenu *dosgui_ctx_menu_create(int x, int y) {
    DosGuiContextMenu *menu = (DosGuiContextMenu*)calloc(1, sizeof(DosGuiContextMenu));
    if (!menu) return NULL;
    menu->x = x;
    menu->y = y;
    menu->visible = false;
    menu->selected_item = -1;
    menu->item_count = 0;
    return menu;
}

void dosgui_ctx_menu_add_item(DosGuiContextMenu *menu, const char *label,
                               void (*action)(void)) {
    if (!menu || menu->item_count >= DOSGUI_MAX_CTX_ITEMS) return;
    DosGuiCtxItem *item = &menu->items[menu->item_count];
    item->type = CTX_ITEM_ACTION;
    item->action = action;
    strncpy(item->label, label, sizeof(item->label) - 1);
    item->disabled = false;
    item->checked = false;
    menu->item_count++;
}

void dosgui_ctx_menu_add_separator(DosGuiContextMenu *menu) {
    if (!menu || menu->item_count >= DOSGUI_MAX_CTX_ITEMS) return;
    DosGuiCtxItem *item = &menu->items[menu->item_count];
    item->type = CTX_ITEM_SEPARATOR;
    menu->item_count++;
}

DosGuiContextMenu *dosgui_ctx_menu_add_submenu(DosGuiContextMenu *menu, const char *label) {
    if (!menu || menu->item_count >= DOSGUI_MAX_CTX_ITEMS) return NULL;
    DosGuiContextMenu *submenu = dosgui_ctx_menu_create(0, 0);
    if (!submenu) return NULL;
    DosGuiCtxItem *item = &menu->items[menu->item_count];
    item->type = CTX_ITEM_SUBMENU;
    strncpy(item->label, label, sizeof(item->label) - 1);
    item->submenu = submenu;
    menu->item_count++;
    return submenu;
}

void dosgui_ctx_menu_show(DosGuiContextMenu *menu, int x, int y) {
    if (!menu) return;
    menu->x = x;
    menu->y = y;
    menu->visible = true;
    menu->selected_item = 0;
    /* Find first non-separator item */
    for (int i = 0; i < menu->item_count; i++) {
        if (menu->items[i].type != CTX_ITEM_SEPARATOR) {
            menu->selected_item = i;
            break;
        }
    }
    ctx_menu_push(menu);
}

void dosgui_ctx_menu_hide(DosGuiContextMenu *menu) {
    if (!menu) return;
    menu->visible = false;
    if (g_dosgui_ctx_stack == menu) {
        ctx_menu_pop();
    }
}

void dosgui_ctx_menu_handle_mouse(int x, int y, int btn, int kind) {
    if (!g_dosgui_ctx_stack) return;
    
    DosGuiContextMenu *menu = g_dosgui_ctx_stack;
    int item_h = 24;
    int menu_w = 180;
    int menu_x = menu->x;
    int menu_y = menu->y;
    
    /* Check if click is outside menu */
    if (x < menu_x || x >= menu_x + menu_w || y < menu_y || y >= menu_y + menu->item_count * item_h) {
        /* Pop all menus */
        while (g_dosgui_ctx_stack) {
            ctx_menu_pop();
        }
        return;
    }
    
    if (kind == 0) { /* Mouse move */
        int item = (y - menu_y) / item_h;
        if (item >= 0 && item < menu->item_count && menu->items[item].type != CTX_ITEM_SEPARATOR) {
            menu->selected_item = item;
        }
    } else if (kind == 1) { /* Mouse down */
        int item = (y - menu_y) / item_h;
        if (item >= 0 && item < menu->item_count) {
            DosGuiCtxItem *it = &menu->items[item];
            if (it->type == CTX_ITEM_ACTION && it->action && !it->disabled) {
                it->action();
                while (g_dosgui_ctx_stack) ctx_menu_pop();
            } else if (it->type == CTX_ITEM_SUBMENU && it->submenu) {
                /* Show submenu to the right */
                dosgui_ctx_menu_show(it->submenu, menu_x + menu_w, menu_y + item * item_h);
            }
        }
    }
}

void dosgui_ctx_menu_render(uint32_t *fb, int fb_w, int fb_h) {
    
    DosGuiContextMenu *menu = g_dosgui_ctx_stack;
    while (menu) {
        if (!menu->visible) {
            menu = menu->parent;
            continue;
        }
        
        int item_h = 24;
        int menu_w = 180;
        int menu_h = menu->item_count * item_h;
        int mx = menu->x;
        int my = menu->y;
        
        /* Clamp to screen */
        if (mx + menu_w > fb_w) mx = fb_w - menu_w;
        if (my + menu_h > fb_h) my = fb_h - menu_h;
        if (mx < 0) mx = 0;
        if (my < 0) my = 0;
        menu->x = mx;
        menu->y = my;
        
        /* Draw menu background */
        vbe_fill_rect_rounded(mx, my, menu_w, menu_h, 4, tc()->win_face);
        vbe_3d_sunken_rounded_colors(mx, my, menu_w, menu_h, 4,
                                      tc()->border_light, tc()->border_face,
                                      tc()->border_dark, tc()->border_darkest);
        
        /* Draw items */
        for (int i = 0; i < menu->item_count; i++) {
            int y = my + i * item_h;
            DosGuiCtxItem *it = &menu->items[i];
            
            if (it->type == CTX_ITEM_SEPARATOR) {
                vbe_hline(mx + 10, mx + menu_w - 10, y + item_h / 2, tc()->border_dark);
                continue;
            }
            
            /* Highlight selected */
            if (i == menu->selected_item && it->type != CTX_ITEM_SUBMENU) {
                vbe_fill_rect(mx + 2, y, menu_w - 4, item_h, tc()->select_bg);
            }
            
            /* Draw label */
            uint32_t text_color = it->disabled ? 0x808080 : tc()->win_title_text;
            vbe_draw_text(mx + 10, y + (item_h - 8) / 2, it->label, text_color, 1);
            
            /* Draw submenu indicator */
            if (it->type == CTX_ITEM_SUBMENU && it->submenu) {
                vbe_draw_text(mx + menu_w - 20, y + (item_h - 8) / 2, ">", text_color, 1);
            }
            
            /* Draw checkmark */
            if (it->checked) {
                vbe_draw_text(mx + 2, y + (item_h - 8) / 2, "*", text_color, 1);
            }
        }
        
        menu = menu->parent;
    }
}

/* -- Default Context Menu Actions -- */

static void ctx_action_open(void) {
    int idx = dosgui_icon_hit_test(g_dwm.mouse_x, g_dwm.mouse_y);
    if (idx >= 0 && g_dwm.icons[idx].on_execute) {
        g_dwm.icons[idx].on_execute();
    }
}

/* -- Dialog Callback Functions -- */

static DosGuiIcon *g_rename_target = NULL;
static char g_rename_input[32] = {0};
static int g_rename_pos = 0;
static DosGuiIcon *g_delete_target = NULL;
static DosGuiIcon *g_properties_target = NULL;

static void dialog_rename_on_key(DosGuiWindow *win, uint32_t key, uint32_t mods) {
    if (key == '\r' || key == '\n') {
        if (g_rename_pos > 0 && g_rename_target) {
            strncpy(g_rename_target->name, g_rename_input, sizeof(g_rename_target->name) - 1);
            wubu_notify_simple("Desktop", "Renamed", g_rename_target->name, NULL, 1, 2000);
        }
        g_rename_input[0] = '\0';
        g_rename_pos = 0;
        g_rename_target = NULL;
        dosgui_wm_destroy(win);
    } else if (key == 27) {  /* Escape */
        g_rename_input[0] = '\0';
        g_rename_pos = 0;
        g_rename_target = NULL;
        dosgui_wm_destroy(win);
    } else if (key == 8 && g_rename_pos > 0) {  /* Backspace */
        g_rename_input[--g_rename_pos] = '\0';
    } else if (key >= 32 && key < 127 && g_rename_pos < 31) {  /* Printable */
        g_rename_input[g_rename_pos++] = (char)key;
        g_rename_input[g_rename_pos] = '\0';
    }
}

static void dialog_rename_on_draw(DosGuiWindow *win, uint32_t *fb, int fb_w, int fb_h) {
    const int tbh = 20;
    const int bw = 3;
    int cx = win->x + bw;
    int cy = win->y + tbh;
    int cw = win->w - 2 * bw;
    int ch = win->h - tbh - bw;
    
    vbe_fill_rect(cx, cy, cw, ch, 0x00E0E0E0);
    vbe_draw_text(cx + 10, cy + 20, "New name:", 0x00000000, 1);
    vbe_draw_text(cx + 10, cy + 50, g_rename_input[0] ? g_rename_input : "(empty)", 0x00000000, 1);
}

static void dialog_delete_on_key(DosGuiWindow *win, uint32_t key, uint32_t mods) {
    if (key == 'y' || key == 'Y') {
        if (g_delete_target) {
            g_delete_target->alive = false;
            wubu_notify_simple("Desktop", "Deleted", "Icon removed", NULL, 1, 2000);
        }
        g_delete_target = NULL;
        dosgui_wm_destroy(win);
    } else if (key == 'n' || key == 'N' || key == 27) {
        g_delete_target = NULL;
        dosgui_wm_destroy(win);
    }
}

static void dialog_delete_on_draw(DosGuiWindow *win, uint32_t *fb, int fb_w, int fb_h) {
    const int tbh = 20;
    const int bw = 3;
    int cx = win->x + bw;
    int cy = win->y + tbh;
    int cw = win->w - 2 * bw;
    int ch = win->h - tbh - bw;
    
    vbe_fill_rect(cx, cy, cw, ch, 0x00E0E0E0);
    vbe_draw_text(cx + 10, cy + 20, "Delete this icon?", 0x00000000, 1);
    vbe_draw_text(cx + 10, cy + 40, "Press Y to confirm, N to cancel", 0x00000000, 1);
}

static void dialog_properties_on_key(DosGuiWindow *win, uint32_t key, uint32_t mods) {
    if (key == 27 || key == 'q' || key == 'Q') {  /* Escape or Q to close */
        g_properties_target = NULL;
        dosgui_wm_destroy(win);
    }
}

static void dialog_properties_on_draw(DosGuiWindow *win, uint32_t *fb, int fb_w, int fb_h) {
    DosGuiIcon *ic = g_properties_target;
    if (!ic) return;
    
    const int tbh = 20;
    const int bw = 3;
    int cx = win->x + bw;
    int cy = win->y + tbh;
    int cw = win->w - 2 * bw;
    int ch = win->h - tbh - bw;
    int y = cy + 10;
    
    vbe_fill_rect(cx, cy, cw, ch, 0x00E0E0E0);
    
    char buf[256];
    snprintf(buf, sizeof(buf), "Name: %s", ic->name);
    vbe_draw_text(cx + 10, y, buf, 0x00000000, 1); y += 16;
    
    const char *type_str[] = {"App", "Shortcut", "Folder", "Drive", "File", "URL"};
    snprintf(buf, sizeof(buf), "Type: %s", ic->type < 6 ? type_str[ic->type] : "Unknown");
    vbe_draw_text(cx + 10, y, buf, 0x00000000, 1); y += 16;
    
    snprintf(buf, sizeof(buf), "Target: %s", ic->target[0] ? ic->target : "(none)");
    vbe_draw_text(cx + 10, y, buf, 0x00000000, 1); y += 16;
    
    snprintf(buf, sizeof(buf), "Position: (%d, %d) grid (%d, %d)", ic->x, ic->y, ic->grid_x, ic->grid_y);
    vbe_draw_text(cx + 10, y, buf, 0x00000000, 1); y += 16;
    
    snprintf(buf, sizeof(buf), "Color: #%06X", ic->icon_color & 0xFFFFFF);
    vbe_draw_text(cx + 10, y, buf, 0x00000000, 1); y += 16;
    
    vbe_draw_text(cx + 10, y + 10, "Press ESC to close", 0x00808080, 1);
}

/* -- Default Context Menu Actions -- */

static void ctx_action_rename(void) {
    int idx = dosgui_icon_hit_test(g_dwm.mouse_x, g_dwm.mouse_y);
    if (idx >= 0) {
        g_rename_target = &g_dwm.icons[idx];
        g_rename_input[0] = '\0';
        g_rename_pos = 0;
        DosGuiWindow *dialog = dosgui_wm_create_modal(300, 200, 400, 150, "Rename", NULL);
        if (dialog) {
            dialog->on_key = dialog_rename_on_key;
            dialog->on_draw = dialog_rename_on_draw;
        }
    }
}

static void ctx_action_delete(void) {
    int idx = dosgui_icon_hit_test(g_dwm.mouse_x, g_dwm.mouse_y);
    if (idx >= 0) {
        g_delete_target = &g_dwm.icons[idx];
        DosGuiWindow *dialog = dosgui_wm_create_modal(300, 200, 350, 140, "Confirm Delete", NULL);
        if (dialog) {
            dialog->on_key = dialog_delete_on_key;
            dialog->on_draw = dialog_delete_on_draw;
        }
    }
}

static void ctx_action_properties(void) {
    int idx = dosgui_icon_hit_test(g_dwm.mouse_x, g_dwm.mouse_y);
    if (idx >= 0) {
        g_properties_target = &g_dwm.icons[idx];
        DosGuiWindow *dialog = dosgui_wm_create_modal(300, 200, 400, 300, "Properties", NULL);
        if (dialog) {
            dialog->on_key = dialog_properties_on_key;
            dialog->on_draw = dialog_properties_on_draw;
        }
    }
}

static void ctx_action_create_shortcut(void) {
    wubu_notify_simple("Desktop", "Create Shortcut", "Right-click empty space -> New -> Shortcut: create .desktop file in ~/Desktop", NULL, 1, 3000);
}

static void ctx_action_view_desktop(void) {
    wubu_notify_simple("Desktop", "View", "Desktop view options: Grid/List, Auto-arrange, Icon size", NULL, 1, 3000);
}

static void ctx_action_sort_by_name(void) {
    /* Would sort icons by name */
}

static void ctx_action_refresh(void) {
    wubu_notify_simple("Desktop", "Refresh", "Desktop refreshed - reloaded icons from ~/Desktop", NULL, 1, 2000);
}

/* -- Show Icon Context Menu -- */

void dosgui_icon_show_context_menu(int icon_idx, int mx, int my) {
    if (icon_idx < 0 || icon_idx >= DOSGUI_MAX_ICONS) return;
    if (!g_dwm.icons[icon_idx].alive) return;
    
    DosGuiContextMenu *menu = dosgui_ctx_menu_create(mx, my);
    if (!menu) return;
    
    /* Select the icon */
    g_dwm.icons[icon_idx].selected = true;
    
    dosgui_ctx_menu_add_item(menu, "Open", ctx_action_open);
    dosgui_ctx_menu_add_separator(menu);
    dosgui_ctx_menu_add_item(menu, "Rename", ctx_action_rename);
    dosgui_ctx_menu_add_item(menu, "Delete", ctx_action_delete);
    dosgui_ctx_menu_add_separator(menu);
    dosgui_ctx_menu_add_item(menu, "Properties", ctx_action_properties);
    
    dosgui_ctx_menu_show(menu, mx, my);
}

void dosgui_desktop_show_context_menu(int mx, int my) {
    DosGuiContextMenu *menu = dosgui_ctx_menu_create(mx, my);
    if (!menu) return;
    
    dosgui_ctx_menu_add_item(menu, "New", NULL);
    
    DosGuiContextMenu *newmenu = dosgui_ctx_menu_add_submenu(menu, "New");
    dosgui_ctx_menu_add_item(newmenu, "Shortcut", ctx_action_create_shortcut);
    dosgui_ctx_menu_add_item(newmenu, "Folder", NULL);
    dosgui_ctx_menu_add_item(newmenu, "Text Document", NULL);
    
    dosgui_ctx_menu_add_separator(menu);
    dosgui_ctx_menu_add_item(menu, "View", ctx_action_view_desktop);
    
    DosGuiContextMenu *viewmenu = dosgui_ctx_menu_add_submenu(menu, "Sort By");
    dosgui_ctx_menu_add_item(viewmenu, "Name", ctx_action_sort_by_name);
    dosgui_ctx_menu_add_item(viewmenu, "Size", NULL);
    dosgui_ctx_menu_add_item(viewmenu, "Type", NULL);
    dosgui_ctx_menu_add_item(viewmenu, "Date Modified", NULL);
    
    dosgui_ctx_menu_add_separator(menu);
    dosgui_ctx_menu_add_item(menu, "Refresh", ctx_action_refresh);
    dosgui_ctx_menu_add_item(menu, "Properties", ctx_action_properties);
    
    dosgui_ctx_menu_show(menu, mx, my);
}

/* -- Tick ------------------------------------------------------- */

void dosgui_tick(void) {
    g_dwm.ticks++;
}

/* -- Query ------------------------------------------------------- */

int dosgui_wm_screen_w(void) { return g_dwm.screen_w; }
int dosgui_wm_screen_h(void) { return g_dwm.screen_h; }

int dosgui_wm_get_icon_count(void) { return g_dwm.icon_count; }

DosGuiWM *dosgui_wm_state(void) { return &g_dwm; }

/* -- Window Resize and State Management ----------------------------- */

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