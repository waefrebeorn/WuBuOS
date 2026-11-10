/*
 * wm_overview.c -- Win+Tab desktop overview grid
 *
 * Virtual desktops existed with no way to see them: you could switch with
 * Ctrl+Shift+Left/Right but nothing showed which desktops had windows, or how
 * many.  This is the overview grid -- a dimmed full-screen layer with one tile
 * per desktop, its window count, and the current desktop highlighted.
 *
 * Rendering and hit-testing share wm_overview_tile_rect() so a click
 * lands on exactly the tile that was drawn.
 */

#include "../wm.h"
#include "../wm_internal.h"
#include "../kernel/vbe.h"
#include "../wubu_theme.h"

#include <stdio.h>

void wm_overview_toggle(void) {
    g_dwm.overview_open = !g_dwm.overview_open;
}

bool wm_overview_is_open(void) {
    return g_dwm.overview_open;
}

void wm_overview_tile_rect(int fb_w, int fb_h, int desktop,
                                  int *x, int *y, int *w, int *h) {
    int n = g_dwm.desktop_count;
    if (n < 1) n = 1;
    if (n > 9) n = 9;

    int cols = (n <= 2) ? n : 3;
    int rows = (n + cols - 1) / cols;
    int pad = 16;
    int gw = (fb_w - pad * (cols + 1)) / cols;
    int gh = (fb_h - pad * (rows + 1)) / rows;
    if (gw < 40) gw = 40;
    if (gh < 40) gh = 40;

    int col = desktop % cols;
    int row = desktop / cols;
    *x = pad + col * (gw + pad);
    *y = pad + row * (gh + pad);
    *w = gw;
    *h = gh;
}

static int overview_hit(int mx, int my, int fb_w, int fb_h) {
    for (int d = 0; d < g_dwm.desktop_count; d++) {
        int x, y, w, h;
        wm_overview_tile_rect(fb_w, fb_h, d, &x, &y, &w, &h);
        if (mx >= x && mx < x + w && my >= y && my < y + h) return d;
    }
    return -1;
}

bool wm_overview_handle_click(int mx, int my, int fb_w, int fb_h) {
    if (!g_dwm.overview_open) return false;

    int d = overview_hit(mx, my, fb_w, fb_h);
    g_dwm.overview_open = false;
    if (d >= 0) wm_set_current_desktop(d);
    return true;   /* the click was consumed either way */
}

void wm_overview_render(uint32_t *fb, int fb_w, int fb_h) {
    if (!g_dwm.overview_open) return;

    const WubuThemeColors *c = tc();

    /* Dim the whole screen so the tiles read as an overlay. */
    vbe_fill_rect(0, 0, fb_w, fb_h, 0x202020);

    for (int d = 0; d < g_dwm.desktop_count; d++) {
        int x, y, w, h;
        wm_overview_tile_rect(fb_w, fb_h, d, &x, &y, &w, &h);

        int nwin = 0;
        for (int i = 0; i < DOSGUI_MAX_WINDOWS; i++) {
            if (g_dwm.windows[i].alive && g_dwm.windows[i].desktop == d) nwin++;
        }

        bool current = (d == g_dwm.current_desktop);
        uint32_t bg = current ? c->select_bg : c->win_face;

        vbe_fill_rect_rounded(x, y, w, h, 6, bg);
        vbe_rect_rounded(x, y, w, h, 6,
                         current ? c->border_light : c->border_dark);

        char label[48];
        snprintf(label, sizeof(label), "Desktop %d", d + 1);
        vbe_draw_text(x + 8, y + 8, label, c->btn_text, 1);
        snprintf(label, sizeof(label), "%d window%s", nwin, nwin == 1 ? "" : "s");
        vbe_draw_text(x + 8, y + 24, label, c->btn_text, 1);
    }

    vbe_draw_text(8, 2, "Desktop Overview -- Esc closes, click a tile to switch",
                  c->btn_text, 1);
}
