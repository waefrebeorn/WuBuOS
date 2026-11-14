/*
 * WuBuOS -- extracted module (auto-split, C11, opaque-safe)
 */

#include "wm.h"
#include "wm_internal.h"
#include "wubu_theme.h"
#include <stdint.h>
#include <string.h>

int title_bar_height(void) { return theme()->Luna_start_button ? 24 : DOSGUI_TITLE_H; }
int taskbar_height_dynamic(void) { return theme()->Luna_start_button ? 30 : DOSGUI_TASK_H; }
int taskbar_top(void) {
    return g_dwm.screen_h - taskbar_height_dynamic() + g_dwm.taskbar_offset;
}
/* Must match chrome_border_width() in window_chrome.c (2:1) —
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

    /* Always base-fill the WHOLE screen with the theme background first.
     * Wallpaper modes (CENTER/FIT/FILL) leave surrounding margins unpainted,
     * and a centered image smaller than the screen would otherwise expose the
     * raw vbe_clear() black instead of the theme bg.
     *
     * The fill must reach the bottom edge, not stop at fb_h - task_h: the
     * classic bar paints over its own band, but the floating modern bar does
     * not, and the band around it would otherwise show the raw clear colour
     * (observed: a full-width black stripe under the dark theme's bar). */
    vbe_fill_rect(0, 0, fb_w, fb_h, tc()->desktop_bg);

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
 * WINDOWS 11 SNAP LAYOUTS
 *
 * The old picker offered six EQUAL zones (halves + quarters).  Windows 11 does not do
 * that: it offers six LAYOUTS, each an arrangement of two to four unequal regions, and
 * you pick the layout first and then which region of it to fill.  The difference is
 * visible immediately -- halves and quarters are only two of its six options.
 *
 * Fractions are normalised so the same table serves every resolution and every
 * taskbar height.  Rows below are the exact set from a wide Windows 11 display:
 *   0  two equal halves
 *   1  three equal columns
 *   2  left half, right half split top/bottom
 *   3  right half, left half split top/bottom
 *   4  four quadrants
 *   5  main-left two thirds, right third
 */
typedef struct { float x, y, w, h; } snap_frac_t;

typedef struct {
    int         zones;
    snap_frac_t z[4];
} snap_layout_t;

static const snap_layout_t kSnapLayouts[WUBU_SNAP_LAYOUTS] = {
    /* 0: [ 1/2 | 1/2 ] */
    { 2, { {0.0f, 0.0f, 0.5f, 1.0f},
           {0.5f, 0.0f, 0.5f, 1.0f} } },
    /* 1: [ 1/3 | 1/3 | 1/3 ] */
    { 3, { {0.0f,        0.0f, 1.0f/3.0f, 1.0f},
           {1.0f/3.0f,   0.0f, 1.0f/3.0f, 1.0f},
           {2.0f/3.0f,   0.0f, 1.0f/3.0f, 1.0f} } },
    /* 2: [ 1/2 | 1/4 ]
               | 1/4 ]   -- left column full height, right split */
    { 3, { {0.0f, 0.0f, 0.5f, 1.0f},
           {0.5f, 0.0f, 0.5f, 0.5f},
           {0.5f, 0.5f, 0.5f, 0.5f} } },
    /* 3: [ 1/4 | 1/2 ]
           [ 1/4 |     ]   -- mirror of 2 */
    { 3, { {0.0f, 0.0f, 0.5f, 0.5f},
           {0.0f, 0.5f, 0.5f, 0.5f},
           {0.5f, 0.0f, 0.5f, 1.0f} } },
    /* 4: four quadrants */
    { 4, { {0.0f, 0.0f, 0.5f, 0.5f},
           {0.5f, 0.0f, 0.5f, 0.5f},
           {0.0f, 0.5f, 0.5f, 0.5f},
           {0.5f, 0.5f, 0.5f, 0.5f} } },
    /* 5: main-left 2/3 + right 1/3 */
    { 2, { {0.0f, 0.0f, 2.0f/3.0f, 1.0f},
           {2.0f/3.0f, 0.0f, 1.0f/3.0f, 1.0f} } },
};

/* Shared guard: one definition, so rendering and snapping cannot disagree about
 * whether a (layout, zone) pair exists. */
static bool wubu_snap_valid(int layout, int zone)
{
    return layout >= 0 && layout < WUBU_SNAP_LAYOUTS &&
           zone >= 0 && zone < kSnapLayouts[layout].zones;
}

int wubu_snap_layout_zones(int layout)
{
    if (layout < 0 || layout >= WUBU_SNAP_LAYOUTS) return 0;
    return kSnapLayouts[layout].zones;
}

void wubu_snap_layout_rect(int layout, int zone,
                           float *nx, float *ny, float *nw, float *nh)
{
    if (!wubu_snap_valid(layout, zone)) {
        if (nx) *nx = 0.0f;
        if (ny) *ny = 0.0f;
        if (nw) *nw = 0.0f;
        if (nh) *nh = 0.0f;
        return;
    }
    const snap_frac_t *r = &kSnapLayouts[layout].z[zone];
    if (nx) *nx = r->x;
    if (ny) *ny = r->y;
    if (nw) *nw = r->w;
    if (nh) *nh = r->h;
}

bool wubu_snap_apply_layout_zone(DosGuiWindow *w, int layout, int zone)
{
    if (!w || !wubu_snap_valid(layout, zone)) return false;

    float nx, ny, nw, nh;
    wubu_snap_layout_rect(layout, zone, &nx, &ny, &nw, &nh);

    int task_h = taskbar_height_dynamic();
    int sw = g_dwm.screen_w;
    int sh = g_dwm.screen_h - task_h;
    if (sw <= 0 || sh <= 0) return false;

    w->x = (int)(nx * sw);
    w->y = (int)(ny * sh);
    w->w = (int)(nw * sw);
    w->h = (int)(nh * sh);

    /* Integer truncation otherwise leaves a one-pixel DEAD STRIP along the right and
     * bottom edges: a third of 1024 is 341, and 682 + 341 is 1023, not 1024.  Any
     * zone that touches an edge is extended to it, so the regions tile the screen
     * exactly instead of leaving a sliver no window can occupy. */
    if (nx + nw >= 0.999f) w->w = sw - w->x;
    if (ny + nh >= 0.999f) w->h = sh - w->y;

    /* A fraction can round a thin layout down to zero at small resolutions; keep the
     * window usable rather than letting a zone collapse. */
    if (w->w < 80)  w->w = 80;
    if (w->h < 60)  w->h = 60;
    if (w->x + w->w > sw) w->x = sw - w->w;
    if (w->y + w->h > sh) w->y = sh - w->h;
    if (w->x < 0) w->x = 0;
    if (w->y < 0) w->y = 0;

    w->flags &= ~DOSGUI_WIN_MAXIMIZED;
    return true;
}

/* Snap Layouts (keyboard, Win+Arrow): place a window in a named zone.
 * 0=left half, 1=right half, 2=top-left quarter, 3=top-right quarter,
 * 4=bottom-left quarter, 5=bottom-right quarter. */
void snap_window_to_zone(DosGuiWindow *w, int zone) {
    if (!w) return;

    int task_h = taskbar_height_dynamic();
    int sw = g_dwm.screen_w;
    int sh = g_dwm.screen_h - task_h;
    int hw = sw / 2;
    int hh = sh / 2;

    switch (zone) {
        case 0:  w->x = 0;  w->y = 0;  w->w = hw; w->h = sh; break;
        case 1:  w->x = hw; w->y = 0;  w->w = hw; w->h = sh; break;
        case 2:  w->x = 0;  w->y = 0;  w->w = hw; w->h = hh; break;
        case 3:  w->x = hw; w->y = 0;  w->w = hw; w->h = hh; break;
        case 4:  w->x = 0;  w->y = hh; w->w = hw; w->h = hh; break;
        case 5:  w->x = hw; w->y = hh; w->w = hw; w->h = hh; break;
        default: return;
    }
    w->flags &= ~DOSGUI_WIN_MAXIMIZED;
}

/* ================================================================
 * RENDERING — Themed Window Chrome
 * ================================================================ */

