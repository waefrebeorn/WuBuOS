/*
 * wubu_wm_render.c  --  WuBuOS Window Manager Rendering
 *
 * Draws the WM desktop: window chrome (title bar, borders, buttons),
 * taskbar, GAAD snap previews, and virtual desktop indicators.
 * Uses the VBE framebuffer for direct pixel access.
 */
#include "wubu_wm_internal.h"
#include "../kernel/vbe.h"
#include <string.h>
#include <stdio.h>

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
            vbe_3d_raised_colors(w->x, w->y, w->w, w->h,
                                  tc->border_light, tc->border_face,
                                  tc->border_dark, tc->border_darkest);

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
            vbe_3d_raised_colors(cbx, cby, 16, 14,
                                  tc->border_light, tc->border_face,
                                  tc->border_dark, tc->border_darkest);

            /* Maximize button */
            int mbx = cbx - 18;
            vbe_fill_rect(mbx, cby, 16, 14, tc->win_face);
            vbe_3d_raised_colors(mbx, cby, 16, 14,
                                  tc->border_light, tc->border_face,
                                  tc->border_dark, tc->border_darkest);

            /* Minimize button */
            int mnx = mbx - 18;
            vbe_fill_rect(mnx, cby, 16, 14, tc->win_face);
            vbe_3d_raised_colors(mnx, cby, 16, 14,
                                  tc->border_light, tc->border_face,
                                  tc->border_dark, tc->border_darkest);

            /* Title text */
            vbe_draw_text(w->x + bw + 3, w->y + bw + 2, w->title,
                          tc->win_title_text, 0x00000000);

            /* Window content area */
            int cx = w->x + bw;
            int cy = w->y + bw + th;
            int cw = w->w - 2*bw;
            int ch = w->h - 2*bw - th;

            /* Content area — no clip support in VBE */
            if (w->on_draw) {
                w->on_draw(w, fb, fb_w, fb_h);
            } else {
                vbe_fill_rect(cx, cy, cw, ch, tc->win_face);
            }
        }
    }

    /* Taskbar at bottom */
    int tb_y = g_wm.screen_h - 28;
    vbe_fill_rect(0, tb_y, g_wm.screen_w, 28, tc->taskbar_bg);

    /* Taskbar buttons for each window */
    int btn_x = 2;
    for (int i = 0; i < WUBU_WM_MAX_WINDOWS; i++) {
        WubuWin *w = &g_wm.windows[i];
        if (w->flags == WUBU_WIN_UNUSED) continue;
        if (w->desktop != g_wm.desktops.current && !(w->flags & WUBU_WIN_STICKY))
            continue;

        int btn_w = 120;
        if (btn_x + btn_w > g_wm.screen_w) break;

        uint32_t btn_bg = (w->flags & WUBU_WIN_FOCUSED)
                          ? tc->btn_face
                          : tc->win_face;
        vbe_fill_rect(btn_x, tb_y + 2, btn_w, 24, btn_bg);
        vbe_draw_text(btn_x + 4, tb_y + 5, w->title, tc->btn_text, 0);

        btn_x += btn_w + 2;
    }

    /* Desktop name (bottom-left taskbar) */
    char desktop_name[32];
    snprintf(desktop_name, sizeof(desktop_name), "Desktop %d/%d",
             g_wm.desktops.current + 1, g_wm.desktops.count);
    vbe_draw_text(4, tb_y + 6, desktop_name, tc->btn_text, 0);

    /* GAAD snap preview overlay */
    if (g_wm.gaad_snap_preview) {
        for (int i = 0; i < g_wm.gaad.n_regions; i++) {
            int rx, ry, rw, rh;
            wubu_gaad_snap_pos(&g_wm.gaad, i, &rx, &ry, &rw, &rh);
            vbe_rect(rx, ry, rw, rh, 0x40FFFFFF);
        }
    }
}

void wubu_wm_invalidate(WubuWin *win) {
    (void)win;
}

void wubu_wm_gaad_recompute(void) {
    wubu_gaad_decompose_feng_shui(g_wm.screen_w, g_wm.screen_h, 4,
                                   &g_wm.gaad, &g_wm.feng_shui);
}

void wubu_wm_set_resolution(int w, int h) {
    g_wm.screen_w = w;
    g_wm.screen_h = h;
    wubu_wm_gaad_recompute();
}
