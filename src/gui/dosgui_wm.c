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
#include "wubu_wallpaper.h"
#include <sys/stat.h>
#include <errno.h>
#include <dirent.h>

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
    if (g_dwm.wallpaper) return;  /* already loaded */

    /* ReactOS-style: prefer the configured wallpaper path from settings
     * (written by Control Panel / Display Properties). Decode the real image. */
    const WubuSettings *s = wubu_settings_get();
    if (s && s->theme.wallpaper_path[0]) {
        WubuWallpaper wp;
        if (wubu_wallpaper_load(s->theme.wallpaper_path, &wp) && wp.pixels) {
            g_dwm.wallpaper = wp.pixels;          /* takes ownership */
            g_dwm.wallpaper_w = wp.w;
            g_dwm.wallpaper_h = wp.h;
            g_dwm.wallpaper_mode = s->theme.wallpaper_mode;  /* 0..4 */
            return;
        }
        /* Decode failed -> fall through to bundled default. */
    }

    /* Bundled default wallpaper (ships with source, WuBuOS teal-blue gradient
     * with centered "W" logo). Better than a raw gradient. */
    if (!(s && s->theme.wallpaper_path[0])) {
        const char *def_path = wubu_wallpaper_default_path();
        if (def_path) {
            WubuWallpaper wp;
            if (wubu_wallpaper_load(def_path, &wp) && wp.pixels) {
                g_dwm.wallpaper = wp.pixels;
                g_dwm.wallpaper_w = wp.w;
                g_dwm.wallpaper_h = wp.h;
                g_dwm.wallpaper_mode = WUBU_WP_CENTER;  /* show logo centered */
                return;
            }
        }
    }

    /* Gradient fallback (classic WuBu teal→blue). */
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
    g_dwm.wallpaper_mode = (s && s->theme.wallpaper_path[0]) ? s->theme.wallpaper_mode : 1;
}

void draw_wallpaper(int fb_w, int fb_h) {
    int task_h = taskbar_height_dynamic();

    if (!g_dwm.wallpaper) {
        vbe_fill_rect(0, 0, fb_w, fb_h - task_h, tc()->desktop_bg);
        return;
    }

    int mode = g_dwm.wallpaper_mode;
    if (mode == 0) {
        /* CENTER: native size, centered, no scaling. Leave theme bg outside. */
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
        /* TILE: repeat native-size image. */
        for (int y = 0; y < fb_h - task_h; y++) {
            for (int x = 0; x < fb_w; x++) {
                int sx = x % g_dwm.wallpaper_w;
                int sy = y % g_dwm.wallpaper_h;
                vbe_set_pixel(x, y, g_dwm.wallpaper[sy * g_dwm.wallpaper_w + sx]);
            }
        }
    } else {
        /* STRETCH / FIT / FILL: sample decoded image across dest rect. */
        int dx0, dy0, dw, dh;
        wubu_wallpaper_rect((WubuWallpaperMode)mode,
                            g_dwm.wallpaper_w, g_dwm.wallpaper_h,
                            fb_w, fb_h, task_h,
                            &dx0, &dy0, &dw, &dh);
        for (int y = dy0; y < dy0 + dh && y < fb_h - task_h; y++) {
            for (int x = dx0; x < dx0 + dw && x < fb_w; x++) {
                int sx = (x - dx0) * g_dwm.wallpaper_w / dw;
                int sy = (y - dy0) * g_dwm.wallpaper_h / dh;
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

/* -- Desktop view options (Stream 3) --------------------------------- */

void dosgui_wm_set_auto_arrange(bool on) {
    g_dwm.auto_arrange = on;
}

bool dosgui_wm_get_auto_arrange(void) {
    return g_dwm.auto_arrange;
}

/* Re-flow all live desktop icons into a single top-left column,
 * preserving their current order. Mirrors ReactOS desktop arrange. */
static void reflow_icons_column(void) {
    const int x0 = 20;
    const int y0 = 20;
    const int step = DOSGUI_ICON_SIZE + DOSGUI_ICON_GAP + 8;
    int y = y0;
    for (int i = 0; i < g_dwm.icon_count; i++) {
        DosGuiIcon *ic = &g_dwm.icons[i];
        if (!ic->alive) continue;
        ic->x = x0;
        ic->y = y;
        ic->grid_x = 0;
        ic->grid_y = (y - y0) / step;
        y += step;
    }
}

/* Public wrapper so other modules (context menu) can re-flow icons. */
void reflow_all_icons_column(void) {
    reflow_icons_column();
}

/* Resolve the user's Desktop directory (XDG or ~/Desktop). */
static void desktop_dir_path(char *out, size_t n) {
    const char *xdg = getenv("XDG_DESKTOP_DIR");
    if (xdg && *xdg) { snprintf(out, n, "%s", xdg); return; }
    const char *home = getenv("HOME");
    if (home && *home) { snprintf(out, n, "%s/Desktop", home); return; }
    snprintf(out, n, "/tmp/Desktop");
}

/* Write a real Freedesktop .desktop shortcut into ~/Desktop and return 0
 * on success. The shortcut is also surfaced as a live desktop icon. */
int dosgui_wm_write_desktop_shortcut(const char *name, const char *exec) {
    if (!name || !*name) return -1;
    char dir[512];
    desktop_dir_path(dir, sizeof(dir));
    struct stat st;
    if (stat(dir, &st) != 0) {
        if (mkdir(dir, 0755) != 0 && errno != EEXIST) return -1;
    }
    char fname[320];
    snprintf(fname, sizeof(fname), "%s/%.200s.desktop", dir, name);
    for (char *p = fname; *p; p++) if (*p == ' ') *p = '_';

    char exec_buf[512];
    if (exec && *exec) snprintf(exec_buf, sizeof(exec_buf), "%s", exec);
    else snprintf(exec_buf, sizeof(exec_buf), "wubu-app %s", name);

    FILE *f = fopen(fname, "w");
    if (!f) return -1;
    fprintf(f, "[Desktop Entry]\n");
    fprintf(f, "Type=Application\n");
    fprintf(f, "Version=1.0\n");
    fprintf(f, "Name=%s\n", name);
    fprintf(f, "Comment=WuBuOS desktop shortcut\n");
    fprintf(f, "Exec=%s\n", exec_buf);
    fprintf(f, "Terminal=false\n");
    fprintf(f, "Categories=WuBuOS;\n");
    fclose(f);

    /* Surface it immediately as a live desktop icon (auto-arranged column). */
    if (g_dwm.auto_arrange) {
        int gy = 0;
        for (int i = 0; i < g_dwm.icon_count; i++)
            if (g_dwm.icons[i].alive) gy++;
        dosgui_shortcut_create(name, fname, "WuBuOS desktop shortcut", 0, gy);
    }
    return 0;
}

/* Sort the live desktop icons alphabetically by name (case-insensitive),
 * preserving the alive flag, then re-flow them into the auto-arrange column.
 * Mirrors ReactOS "Arrange Icons By Name". */
void dosgui_wm_sort_icons_by_name(void) {
    /* Simple insertion sort over the live icon array. */
    for (int i = 1; i < g_dwm.icon_count; i++) {
        DosGuiIcon key = g_dwm.icons[i];
        int j = i - 1;
        while (j >= 0 &&
               strcasecmp(g_dwm.icons[j].name, key.name) > 0) {
            g_dwm.icons[j + 1] = g_dwm.icons[j];
            j--;
        }
        g_dwm.icons[j + 1] = key;
    }
    reflow_icons_column();
}

/* Real filesystem refresh: scan ~/Desktop for *.desktop files and add live
 * icons for any not already present (dedup by name). Mirrors Explorer refresh. */
void dosgui_wm_refresh_desktop(void) {
    char dir[512];
    desktop_dir_path(dir, sizeof(dir));
    DIR *d = opendir(dir);
    if (!d) return;
    struct dirent *e;
    int gy = 0;
    for (int i = 0; i < g_dwm.icon_count; i++)
        if (g_dwm.icons[i].alive) gy++;
    while ((e = readdir(d)) != NULL) {
        size_t len = strlen(e->d_name);
        if (len < 8 || strcmp(e->d_name + len - 8, ".desktop") != 0) continue;
        /* Dedup by name (strip .desktop). */
        char base[256];
        snprintf(base, sizeof(base), "%.*s", (int)(len - 8), e->d_name);
        bool dup = false;
        for (int i = 0; i < g_dwm.icon_count; i++)
            if (g_dwm.icons[i].alive &&
                strncasecmp(g_dwm.icons[i].name, base, 255) == 0) { dup = true; break; }
        if (dup) continue;
        char path[640];
        snprintf(path, sizeof(path), "%s/%s", dir, e->d_name);
        if (g_dwm.icon_count < DOSGUI_MAX_ICONS) {
            dosgui_shortcut_create(base, path, "Desktop shortcut", 0, gy++);
        }
    }
    closedir(d);
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
    icon->alive = true;           /* Registered icon is live (matches dosgui_icon_add_ex) */
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

/* -- Icon persistence (Stream 2) & wallpaper reload (Stream 4) ---- */

DosGuiIcon *dosgui_icon_get(int idx) {
    if (idx < 0 || idx >= g_dwm.icon_count) return NULL;
    return &g_dwm.icons[idx];
}

bool dosgui_wm_is_initialized(void) { return g_dwm.screen_w > 0; }

int dosgui_wm_wallpaper_mode(void) { return g_dwm.wallpaper_mode; }
int dosgui_wm_wallpaper_w(void)    { return g_dwm.wallpaper_w; }
int dosgui_wm_wallpaper_h(void)    { return g_dwm.wallpaper_h; }

/* Persist the current live desktop icon grid into settings
 * (ReactOS-style: store string name + position, not callbacks). */
void dosgui_wm_save_icon_layout(void) {
    WubuSettings *s = wubu_settings_mut();
    if (!s) return;
    s->theme.icon_layout_count = 0;
    for (int i = 0; i < g_dwm.icon_count && s->theme.icon_layout_count < WUBU_ICON_LAYOUT_MAX; i++) {
        DosGuiIcon *ic = &g_dwm.icons[i];
        if (!ic->alive) continue;
        IconLayoutEntry *e = &s->theme.icon_layout[s->theme.icon_layout_count++];
        strncpy(e->name, ic->name, sizeof(e->name) - 1);
        e->grid_x = ic->grid_x;
        e->grid_y = ic->grid_y;
        e->alive  = true;
    }
    wubu_settings_save();
}

/* Restore live icon grid positions from persisted settings, matching
 * by icon name. Positions survive a desktop restart. */
void dosgui_wm_restore_icon_layout(void) {
    const WubuSettings *s = wubu_settings_get();
    if (!s || s->theme.icon_layout_count <= 0) return;
    for (int i = 0; i < g_dwm.icon_count; i++) {
        DosGuiIcon *ic = &g_dwm.icons[i];
        if (!ic->alive) continue;
        for (int j = 0; j < s->theme.icon_layout_count; j++) {
            const IconLayoutEntry *e = &s->theme.icon_layout[j];
            if (e->alive && strncmp(e->name, ic->name, sizeof(e->name)) == 0) {
                ic->grid_x = e->grid_x;
                ic->grid_y = e->grid_y;
                ic->x = 20 + e->grid_x * (DOSGUI_ICON_SIZE + DOSGUI_ICON_GAP);
                ic->y = 20 + e->grid_y * (DOSGUI_ICON_SIZE + DOSGUI_ICON_GAP + 8);
                break;
            }
        }
    }
}

/* Re-decode the configured wallpaper from settings (Control Panel apply). */
void dosgui_wm_reload_wallpaper(void) {
    if (g_dwm.wallpaper) { free(g_dwm.wallpaper); g_dwm.wallpaper = NULL; }
    load_default_wallpaper();
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

    /* -- Status bar tip (cycling keyboard hint) ------------------ */
    {
        static time_t last_tip_swap = 0;
        static int tip_index = 0;
        time_t now = time(NULL);

        if (now != last_tip_swap) {
            /* Cycle tip every ~10 seconds (checked every render frame) */
            if (now - last_tip_swap >= 10) {
                tip_index = (tip_index + 1) % 6;
                last_tip_swap = now;
            }
            /* Initialize clock on first render */
            if (last_tip_swap == 0) {
                last_tip_swap = now;
                tip_index = 0;
            }
        }

        const char *tips[] = {
            "Ctrl+T = cycle theme",
            "Alt+F4 = close window",
            "Win key = Start menu",
            "Ctrl+Alt+Left/Right = desktop",
            "Shift+F10 = context menu",
            "Ctrl+T = cycle theme",
        };
        const char *tip = tips[tip_index % 6];
        int tip_x = desk_x - vbe_text_width(tip, 1) - 20;
        if (tip_x > start_w + 90) {
            vbe_draw_text(tip_x, ty + (th - 8) / 2, tip, tc()->icon_text, 1);
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

/* ══════════════════════════════════════════════════════════════════
 * Invalidation tracking (legacy wm_invalidate / wm_invalidate_all)
 *
 * The hosted render loop (wubu_syscall.c) calls dosgui_wm_render() every
 * frame, so the whole screen is redrawn unconditionally.  These helpers
 * still do *real* bookkeeping: they record which windows were explicitly
 * invalidated so a smarter (dirty-region) renderer can later skip clean
 * windows.  dosgui_wm_poll_dirty() drains the queue; -1 means "redraw all".
 * ══════════════════════════════════════════════════════════════════ */

#define DOSGUI_DIRTY_MAX 64
static int    g_dirty_ids[DOSGUI_DIRTY_MAX];
static int    g_dirty_count = 0;
static bool   g_dirty_all   = false;

void dosgui_wm_invalidate(DosGuiWindow *win) {
    if (!win) { g_dirty_all = true; return; }
    if (g_dirty_count < DOSGUI_DIRTY_MAX)
        g_dirty_ids[g_dirty_count++] = win->id;
}

void dosgui_wm_invalidate_all(void) {
    g_dirty_all = true;
}

/* Drain one invalidated window id.  Returns true and writes *out_id:
 *   *out_id >= 0  -> that window was invalidated
 *   *out_id == -1 -> a full-redraw was requested (wm_invalidate_all)
 * Returns false when the queue is empty. */
bool dosgui_wm_poll_dirty(int *out_id) {
    if (g_dirty_all) {
        g_dirty_all = false;
        g_dirty_count = 0;
        if (out_id) *out_id = -1;
        return true;
    }
    if (g_dirty_count > 0) {
        if (out_id) *out_id = g_dirty_ids[--g_dirty_count];
        return true;
    }
    return false;
}

int dosgui_wm_dirty_count(void) {
    return g_dirty_all ? -1 : g_dirty_count;
}

/* Platform lifecycle hook.  The hosted binary (src/hosted/hosted.c) provides
 * the real implementation (tears down the Wayland surface).  Standalone app
 * binaries (paint, doom, ...) link this weak no-op default so they build
 * without pulling in the full hosted stack. */
__attribute__((weak))
void dosgui_platform_shutdown(void) {
    /* No-op for standalone app binaries. */
}

__attribute__((weak))
void dosgui_shutdown(void) {
    /* No-op for standalone app binaries. */
}


/* EOF */
