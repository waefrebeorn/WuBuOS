/*
 * WuBuOS -- extracted module (auto-split, C11, opaque-safe)
 */

#include "wm.h"
#include "wm_internal.h"
#include "wubu_theme.h"
#include <stdint.h>
#include <string.h>

int title_bar_height(void) { return theme()->Luna_start_button ? 24 : DOSGUI_TITLE_H; }
int taskbar_height_dynamic(void) {
    if (wubu_theme_is_fluent()) return WUBU_METRIC_TASKBAR_H;   /* 48, Win11 */
    return theme()->Luna_start_button ? 30 : DOSGUI_TASK_H;
}
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
            if (wubu_theme_current() == THEME_DARK)
                wubu_wallpaper_darken(g_dwm.wallpaper, wp.w, wp.h, 0.35f);
            return;
        }
        /* Decode failed -> fall through to bundled default. */
    }

    /* Default (no user-configured wallpaper): prefer the shipped Steam Deck
     * style wallpaper decoded into the real framebuffer (crop-to-fill so it
     * covers the desktop edge-to-edge). The classic gradient is a last-resort
     * fallback only when the asset can't be found or decoded. */
    if (!(s && s->theme.wallpaper_path[0])) {
        const char *def_path = wubu_wallpaper_default_path();
        WubuWallpaper wp;
        if (def_path && wubu_wallpaper_load(def_path, &wp) && wp.pixels) {
            g_dwm.wallpaper = wp.pixels;
            g_dwm.wallpaper_w = wp.w;
            g_dwm.wallpaper_h = wp.h;
            g_dwm.wallpaper_mode = WUBU_WP_FILL;  /* crop-to-fill, no banding */
            /* A light shipped wallpaper on the dark shell is the "blinding" the user
             * reported: 29% of the screen at luminance > 200. Dark mode owns this. */
            if (wubu_theme_current() == THEME_DARK)
                wubu_wallpaper_darken(g_dwm.wallpaper, wp.w, wp.h, 0.35f);
            return;
        }
        /* Fallback gradient: draw the "W" logo badge so the default still has
         * identity; the real wallpaper needs no badge on top of it. */
        const char *logo_path = wubu_wallpaper_logo_path();
        if (logo_path) {
            WubuWallpaper lw;
            if (wubu_wallpaper_load(logo_path, &lw) && lw.pixels) {
                g_dwm.logo = lw.pixels;          /* badge on the gradient */
                g_dwm.logo_w = lw.w;
                g_dwm.logo_h = lw.h;
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

/* ---- The wallpaper layer is static: compose once, blit after ------------
 * Measured with `wubu -h --bench-render 240`: a 1024x768 desktop frame cost
 * 3.24 ms and draw_wallpaper owned ~3.1 ms of it, because the FILL/STRETCH
 * paths ran two integer divisions plus vbe_set_pixel for each of 786k pixels.
 * The image, its crop and the theme background only change when the user
 * changes them, so compose into a cache and memcpy the rows.
 * Every input that can alter the layer is part of the key, and the fast path
 * is refused whenever a non-full clip rect is active. */
static uint32_t *wp_cache;
static int wp_cache_w, wp_cache_h, wp_cache_task_h;
static const uint32_t *wp_cache_src;
static int wp_cache_sw, wp_cache_sh, wp_cache_mode;
static uint32_t wp_cache_bg;

static int wallpaper_clip_is_full(int fb_w, int fb_h) {
    int x = 0, y = 0, w = 0, h = 0;
    vbe_get_clip(&x, &y, &w, &h);
    /* Zero size means no clip rect is active (the hosted target switch leaves
     * it cleared), which is the same thing as a full-surface clip. */
    if (w <= 0 || h <= 0) return 1;
    return x == 0 && y == 0 && w >= fb_w && h >= fb_h;
}

static int wallpaper_cache_hit(int fb_w, int fb_h, int task_h) {
    if (!wp_cache || wp_cache_w != fb_w || wp_cache_h != fb_h) return 0;
    if (wp_cache_task_h != task_h || wp_cache_src != g_dwm.wallpaper) return 0;
    if (wp_cache_sw != g_dwm.wallpaper_w || wp_cache_sh != g_dwm.wallpaper_h) return 0;
    if (wp_cache_mode != g_dwm.wallpaper_mode) return 0;
    if (wp_cache_bg != tc()->desktop_bg) return 0;
    return wallpaper_clip_is_full(fb_w, fb_h);
}

static void wallpaper_cache_blit(int fb_w, int fb_h) {
    for (int y = 0; y < fb_h; y++) {
        uint32_t *dst = vbe_scanline(y);
        if (!dst) return;
        memcpy(dst, wp_cache + (size_t)y * (size_t)fb_w, (size_t)fb_w * 4u);
    }
}

static void wallpaper_cache_store(int fb_w, int fb_h, int task_h) {
    /* Never capture a clipped repaint: the snapshot must be the whole layer. */
    if (!wallpaper_clip_is_full(fb_w, fb_h)) return;
    if (!wp_cache || wp_cache_w != fb_w || wp_cache_h != fb_h) {
        free(wp_cache);
        wp_cache = (uint32_t *)malloc((size_t)fb_w * (size_t)fb_h * 4u);
        if (!wp_cache) { wp_cache_w = wp_cache_h = 0; return; }
        wp_cache_w = fb_w;
        wp_cache_h = fb_h;
    }
    for (int y = 0; y < fb_h; y++) {
        const uint32_t *src = vbe_scanline(y);
        if (!src) return;
        memcpy(wp_cache + (size_t)y * (size_t)fb_w, src, (size_t)fb_w * 4u);
    }
    wp_cache_task_h = task_h;
    wp_cache_src = g_dwm.wallpaper;
    wp_cache_sw = g_dwm.wallpaper_w;
    wp_cache_sh = g_dwm.wallpaper_h;
    wp_cache_mode = g_dwm.wallpaper_mode;
    wp_cache_bg = tc()->desktop_bg;
}

static void wallpaper_compose(int fb_w, int fb_h, int task_h) {

    /* Always base-fill the WHOLE screen with the theme background first.
     * Wallpaper modes (CENTER/FIT/FILL) leave surrounding margins unpainted,
     * and a centered image smaller than the screen would otherwise expose the
     * raw vbe_clear() black instead of the theme bg.
     *
     * The fill must reach the bottom edge, and so must the WALLPAPER. The wallpaper
     * used to stop at fb_h - task_h, which was right for a classic bar that paints its
     * own full-width band and wrong for the floating modern bar: the 8px gap under it
     * exposed the flat desktop colour (0x17) against wallpaper at ~0x62, which is the
     * "random black bar at the bottom" the user reported. A wall does not stop where a
     * bar begins; the bar floats OVER the wall. */
    vbe_fill_rect(0, 0, fb_w, fb_h, tc()->desktop_bg);

    if (!g_dwm.wallpaper) {
        return;
    }

    int mode = g_dwm.wallpaper_mode;
    if (mode == 0) {
        /* CENTER: native size, centered, no scaling. Leave theme bg outside. */
        int x = (fb_w - g_dwm.wallpaper_w) / 2;
        int y = (fb_h - g_dwm.wallpaper_h) / 2;
        if (x < 0) x = 0;
        if (y < 0) y = 0;
        for (int dy = 0; dy < g_dwm.wallpaper_h && y + dy < fb_h; dy++) {
            for (int dx = 0; dx < g_dwm.wallpaper_w && x + dx < fb_w; dx++) {
                vbe_set_pixel(x + dx, y + dy, g_dwm.wallpaper[dy * g_dwm.wallpaper_w + dx]);
            }
        }
    } else if (mode == 1) {
        /* TILE: repeat native-size image. */
        for (int y = 0; y < fb_h; y++) {
            for (int x = 0; x < fb_w; x++) {
                int sx = x % g_dwm.wallpaper_w;
                int sy = y % g_dwm.wallpaper_h;
                vbe_set_pixel(x, y, g_dwm.wallpaper[sy * g_dwm.wallpaper_w + sx]);
            }
        }
    } else {
        /* STRETCH / FIT / FILL: sample decoded image across dest rect. */
        int dx0, dy0, dw, dh;
        /* task_h = 0: the wallpaper is composed against the WHOLE screen. Passing the
         * bar height made every mode reserve a band at the bottom, which the floating
         * modern bar does not paint over -- the band then showed the flat desktop colour
         * (the "random black bar"). The bar is drawn after this and floats over it. */
        wubu_wallpaper_rect((WubuWallpaperMode)mode,
                            g_dwm.wallpaper_w, g_dwm.wallpaper_h,
                            fb_w, fb_h, 0,
                            &dx0, &dy0, &dw, &dh);
        for (int y = dy0; y < dy0 + dh && y < fb_h; y++) {
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

void draw_wallpaper(int fb_w, int fb_h) {
    int task_h = taskbar_height_dynamic();
    if (wallpaper_cache_hit(fb_w, fb_h, task_h)) {
        wallpaper_cache_blit(fb_w, fb_h);
        return;
    }
    wallpaper_compose(fb_w, fb_h, task_h);
    wallpaper_cache_store(fb_w, fb_h, task_h);
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

/* Which zone is "to the right" of this one?  Windows 11 has no answer at all -- the
 * picker is mouse-only -- so the rule is ours to state: among zones whose centre lies
 * strictly in the requested direction, take the nearest by squared centre distance,
 * and on a tie take the lower index. Deterministic, so a keyboard user learns it once
 * and a test can pin it. */
int wubu_snap_neighbor(int layout, int zone, int dx, int dy)
{
    if (!wubu_snap_valid(layout, zone)) return -1;
    if (dx == 0 && dy == 0) return zone;

    float cx, cy, cw, ch;
    wubu_snap_layout_rect(layout, zone, &cx, &cy, &cw, &ch);
    float mx = cx + cw * 0.5f, my = cy + ch * 0.5f;

    int best = zone;
    float best_d = 0.0f;
    for (int i = 0; i < kSnapLayouts[layout].zones; i++) {
        if (i == zone) continue;
        const snap_frac_t *r = &kSnapLayouts[layout].z[i];
        float ox = r->x + r->w * 0.5f, oy = r->y + r->h * 0.5f;
        float ax = ox - mx, ay = oy - my;
        /* Must be in the requested direction: a step right never lands on something
         * behind you, even when it is geometrically closer. */
        if (dx > 0 && ax <= 0.0f) continue;
        if (dx < 0 && ax >= 0.0f) continue;
        if (dy > 0 && ay <= 0.0f) continue;
        if (dy < 0 && ay >= 0.0f) continue;
        float d = ax * ax + ay * ay;
        if (best_d == 0.0f || d < best_d - 1e-6f) { best_d = d; best = i; }
    }
    return best;
}

wubu_snap_key_action wubu_snap_key_select(int *layout, int *zone, uint32_t key)
{
    if (!layout || !zone) return WUBU_SNAP_KEY_IGNORED;
    int l = *layout, z = *zone;
    if (l < 0 || l >= WUBU_SNAP_LAYOUTS) l = 0;
    int zones = wubu_snap_layout_zones(l);
    if (z < 0 || z >= zones) z = 0;

    if (key >= '1' && key <= (uint32_t)('0' + WUBU_SNAP_LAYOUTS)) {
        l = (int)(key - '1');
        zones = wubu_snap_layout_zones(l);
        /* Clamp instead of pointing at a zone the new layout does not have: a silent
         * out-of-range selection makes Enter snap into nothing at all. */
        if (z >= zones) z = zones - 1;
        *layout = l; *zone = z;
        return WUBU_SNAP_KEY_MOVED;
    }

    int dx = 0, dy = 0;
    if (key == 0x4B || key == 'h') dx = -1;
    else if (key == 0x4D || key == 'l') dx = +1;
    else if (key == 0x48 || key == 'k') dy = -1;
    else if (key == 0x50 || key == 'j') dy = +1;
    if (dx || dy) {
        *zone = wubu_snap_neighbor(l, z, dx, dy);
        *layout = l;
        return WUBU_SNAP_KEY_MOVED;
    }

    if (key == '\r' || key == '\n' || key == ' ') return WUBU_SNAP_KEY_APPLY;
    if (key == 0x1B) return WUBU_SNAP_KEY_CLOSE;
    return WUBU_SNAP_KEY_IGNORED;
}

int wubu_snap_zone_at(int layout, int px, int py, int w, int h)
{
    if (layout < 0 || layout >= WUBU_SNAP_LAYOUTS || w <= 0 || h <= 0) return -1;
    if (px < 0 || py < 0 || px >= w || py >= h) return -1;
    float fx = (float)px / (float)w, fy = (float)py / (float)h;
    for (int i = 0; i < kSnapLayouts[layout].zones; i++) {
        const snap_frac_t *r = &kSnapLayouts[layout].z[i];
        /* Half-open on every edge: a boundary pixel belongs to exactly one zone, so a
         * click on the seam cannot match two regions and be decided by table order. */
        if (fx >= r->x && fx < r->x + r->w && fy >= r->y && fy < r->y + r->h) return i;
    }
    return -1;
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

