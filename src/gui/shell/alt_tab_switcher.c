/*
 * alt_tab_switcher.c -- Alt+Tab with live previews, committed on Alt release.
 *
 * Behaviour, matching Windows:
 *   Alt+Tab (down)  opens the grid with the NEXT window selected; each further Tab advances.
 *   Alt release     commits: that window is focused and the grid closes.
 *   Esc             cancels: nothing changes.
 *   click a card    commits that window.
 *
 * The grid order is z-order (most recently used first) and skips the window that already has
 * focus, which is what makes the first Alt+Tab move rather than re-focus the same window.
 *
 * Previews are sampled from the frame being composed (wm_thumb_capture), exactly as the
 * taskbar hover preview and snap assist do -- one sampler, three surfaces.
 */

#include "alt_tab_switcher.h"

#include "wubu_gui_metrics.h"
#include "../wm_internal.h"
#include "../kernel/vbe.h"
#include "../kernel/input.h"
#include "../wubu_theme.h"

#define AT_MAX_CARDS 8
#define AT_CARD_W    200
#define AT_CARD_H    132
#define AT_GAP       WUBU_SPACE_SM
#define AT_THUMB_W   176
#define AT_THUMB_H   92

static bool g_open;
static int  g_sel;                    /* index into the candidate list */
static int  g_ids[AT_MAX_CARDS];      /* z-order, most recent first */
static int  g_n;
static int  g_origin;                 /* focused id when the switcher opened */

bool alt_tab_is_open(void) { return g_open; }
int  alt_tab_count(void)    { return g_n; }
int  alt_tab_selected(void) { return g_sel; }
int  alt_tab_window_at(int i) { return (i >= 0 && i < g_n) ? g_ids[i] : -1; }

/* Z-order, most recent first. */
static void collect(void) {
    g_n = 0;
    for (int z = 0; z < g_dwm.nz && g_n < AT_MAX_CARDS; z++) {
        int id = g_dwm.zorder[z];
        if (id < 0 || id >= DOSGUI_MAX_WINDOWS) continue;
        DosGuiWindow *w = &g_dwm.windows[id];
        if (!w->alive) continue;
        if (w->desktop != g_dwm.current_desktop) continue;
        g_ids[g_n++] = id;
    }
}

void alt_tab_close(bool commit) {
    if (g_open && commit && g_n > 0 && g_sel >= 0 && g_sel < g_n) {
        DosGuiWindow *w = &g_dwm.windows[g_ids[g_sel]];
        if (w->alive) {
            if (w->flags & DOSGUI_WIN_MINIMIZED) w->flags &= ~DOSGUI_WIN_MINIMIZED;
            wm_set_focus(w);
        }
    }
    g_open = false;
}

bool alt_tab_key_down(uint32_t key, uint32_t mods) {
    bool alt = (mods & MOD_ALT) != 0;
    /* Tab with Alt is the switcher's; a plain Tab belongs to the focused widget. */
    if (!(alt && key == 0x09)) {
        if (g_open && key == 0x01) { alt_tab_close(false); return true; }   /* Esc cancels */
        return false;
    }
    if (!g_open) {
        collect();
        if (g_n < 2) return false;            /* nothing to switch between */
        g_origin = (g_dwm.focused_id >= 0) ? g_dwm.focused_id : g_ids[0];
        /* Start on the first window that is not the current one. */
        g_sel = (g_ids[0] == g_origin && g_n > 1) ? 1 : 0;
        g_open = true;
        return true;
    }
    g_sel = (g_sel + 1) % g_n;
    return true;
}

bool alt_tab_key_up(uint32_t key, uint32_t mods) {
    if (!g_open) return false;
    /* Committing on the Alt release is the whole point: the WM's press-driven model is
     * untouched and only this surface sees key-ups. */
    if ((mods & MOD_ALT) == 0 || key == 0x38 || key == 0x0) {
        alt_tab_close(true);
        return true;
    }
    return false;
}

bool alt_tab_card_rect(int i, int fb_w, int fb_h, int *x, int *y, int *w, int *h) {
    if (i < 0 || i >= g_n) return false;
    int cols = (g_n <= 4) ? g_n : 4;
    if (cols < 1) cols = 1;
    int rows = (g_n + cols - 1) / cols;
    int total_w = cols * AT_CARD_W + (cols - 1) * AT_GAP;
    int total_h = rows * AT_CARD_H + (rows - 1) * AT_GAP;
    int x0 = fb_w / 2 - total_w / 2;
    int y0 = fb_h / 2 - total_h / 2;
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    int r = i / cols, c = i % cols;
    if (x) *x = x0 + c * (AT_CARD_W + AT_GAP);
    if (y) *y = y0 + r * (AT_CARD_H + AT_GAP);
    if (w) *w = AT_CARD_W;
    if (h) *h = AT_CARD_H;
    return true;
}

bool alt_tab_click(int x, int y) {
    if (!g_open) return false;
    for (int i = 0; i < g_n; i++) {
        int cx, cy, cw, ch;
        if (!alt_tab_card_rect(i, g_dwm.screen_w, g_dwm.screen_h, &cx, &cy, &cw, &ch)) continue;
        if (x >= cx && x < cx + cw && y >= cy && y < cy + ch) {
            g_sel = i;
            alt_tab_close(true);
            return true;
        }
    }
    alt_tab_close(false);      /* a click elsewhere cancels, like Windows */
    return true;
}

void alt_tab_render(uint32_t *fb, int fb_w, int fb_h) {
    (void)fb;
    if (!g_open) return;

    /* Dim the desktop so the grid reads as a mode, not as floating windows. */
    int task_h = taskbar_height_dynamic();
    for (int y = 0; y < fb_h - task_h; y++)
        for (int x = 0; x < fb_w; x++) {
            uint32_t c = vbe_get_pixel(x, y);
            uint32_t r = ((c >> 16) & 0xFF) / 3, g = ((c >> 8) & 0xFF) / 3, b = (c & 0xFF) / 3;
            vbe_set_pixel(x, y, (r << 16) | (g << 8) | b);
        }

    for (int i = 0; i < g_n; i++) {
        int cx, cy, cw, ch;
        if (!alt_tab_card_rect(i, fb_w, fb_h, &cx, &cy, &cw, &ch)) continue;
        DosGuiWindow *w = &g_dwm.windows[g_ids[i]];
        bool sel = (i == g_sel);

        static uint32_t thumb[AT_THUMB_W * AT_THUMB_H];
        int have = fb && wm_thumb_capture(fb, fb_w, fb_h, w->x, w->y, w->w, w->h,
                                          thumb, AT_THUMB_W, AT_THUMB_H);

        vbe_fill_rect_rounded(cx, cy, cw, ch, WUBU_METRIC_RADIUS_CONTROL,
                              sel ? tc()->select_bg : tc()->win_face);
        if (sel) vbe_rect(cx, cy, cw, ch, tc()->select_text);

        int tx = cx + (cw - AT_THUMB_W) / 2, ty = cy + WUBU_SPACE_XS;
        for (int y = 0; y < AT_THUMB_H; y++) {
            for (int x = 0; x < AT_THUMB_W; x++) {
                uint32_t c = have ? thumb[y * AT_THUMB_W + x] : tc()->border_dark;
                vbe_set_pixel(tx + x, ty + y, c);
            }
        }
        const char *title = wm_window_title_at(g_ids[i]);
        vbe_draw_text(cx + WUBU_SPACE_XS, cy + AT_THUMB_H + WUBU_SPACE_XS,
                      title ? title : "", sel ? tc()->select_text : tc()->win_title_text, 1);
    }
}
