/*
 * WuBuOS -- extracted module (auto-split, C11, opaque-safe)
 */

#include "dosgui_wm.h"
#include "dosgui_wm_internal.h"
#include "wubu_theme.h"
#include <stdint.h>
#include <string.h>

int title_bar_height(void) { return theme()->Luna_start_button ? 24 : DOSGUI_TITLE_H; }
int taskbar_height_dynamic(void) { return theme()->Luna_start_button ? 30 : DOSGUI_TASK_H; }
/* Must match chrome_border_width() in dosgui_window_chrome.c (2:1) —
 * a mismatch shifts every app's content rect and WM hit-testing by 1px. */
int border_width(void) { return theme()->rounded_buttons ? 2 : 1; }
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

    /* Bundled default: full-screen procedural teal→blue gradient (crisp at
     * any resolution) + the shipped "W" logo BMP drawn as a centered overlay
     * badge on top (NOT stretched — a 256×144 bitmap scaled to 1024×768
     * would be a blurry mess; xfdesktop keeps the wallpaper native and lets
     * the compositor fill the rest). The logo badge is drawn by
     * draw_wallpaper() from g_dwm.logo_* fields. */
    if (!(s && s->theme.wallpaper_path[0])) {
        const char *def_path = wubu_wallpaper_default_path();
        if (def_path) {
            WubuWallpaper wp;
            if (wubu_wallpaper_load(def_path, &wp) && wp.pixels) {
                g_dwm.logo = wp.pixels;          /* keep as badge, not wallpaper */
                g_dwm.logo_w = wp.w;
                g_dwm.logo_h = wp.h;
            }
        }
    }

    /* Gradient fallback (classic WuBu teal→blue) — full-screen, no bitmap. */
    g_dwm.wallpaper_w = g_dwm.screen_w;
    g_dwm.wallpaper_h = g_dwm.screen_h - taskbar_height_dynamic();
    g_dwm.wallpaper = (uint32_t*)malloc((size_t)g_dwm.wallpaper_w * g_dwm.wallpaper_h * 4);
    if (g_dwm.wallpaper) {
        for (int y = 0; y < g_dwm.wallpaper_h; y++) {
            for (int x = 0; x < g_dwm.wallpaper_w; x++) {
                float fy = (float)y / (g_dwm.wallpaper_h > 1 ? g_dwm.wallpaper_h - 1 : 1);
                int r = (int)((0x00 * (1-fy) + 0x08 * fy));
                int g = (int)((0x80 * (1-fy) + 0x38 * fy));
                int b = (int)((0x80 * (1-fy) + 0x90 * fy));
                /* XRGB8888 framebuffer: R in bits 16-23, G in 8-15, B in 0-7.
                 * The previous pack (b<<16)|(g<<8)|r swapped red/blue and
                 * rendered the teal→blue gradient as orange-brown. */
                uint32_t c = (uint32_t)((r << 16) | (g << 8) | b);
                g_dwm.wallpaper[y * g_dwm.wallpaper_w + x] = c;
            }
        }
    }
    g_dwm.wallpaper_mode = (s && s->theme.wallpaper_path[0]) ? s->theme.wallpaper_mode : 1;
}

void draw_wallpaper(int fb_w, int fb_h) {
    int task_h = taskbar_height_dynamic();

    /* Always base-fill the desktop rect with the theme background first.
     * Wallpaper modes (CENTER/FIT/FILL) leave surrounding margins unpainted,
     * and a centered image smaller than the screen would otherwise expose the
     * raw vbe_clear() black instead of the theme bg. TILE/STRETCH fully cover
     * the rect, so this fill is harmless there. */
    vbe_fill_rect(0, 0, fb_w, fb_h - task_h, tc()->desktop_bg);

    if (!g_dwm.wallpaper) {
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

    /* NOTE: the old centered "W" logo-badge overlay (g_dwm.logo) was
     * removed 2026-08-06 — it read as a stuck blue dialog box in the
     * middle of the desktop (user: "you never removed that thing").
     * The logo belongs on a boot/login splash, not the live desktop. */
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

