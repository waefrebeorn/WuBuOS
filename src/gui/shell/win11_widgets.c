/*
 * win11_widgets.c -- Win+W widgets board (Windows 11 parity).
 *
 * One concern: the widgets board.  Cards are laid out top-to-bottom in a
 * left-anchored panel; the clock and the thermal readings are real (the
 * thermal driver is the same source the buddy uses for its OVERHEAT pose).
 *
 * C11, opaque API (win11_widgets.h), no god headers.
 */

#include "win11_widgets.h"
#include "../wm_internal.h"
#include "../wm.h"
#include "../kernel/vbe.h"
#include "../kernel/wubu_drv_thermal.h"
#include "../kernel/wubu_thermal_read.h"
#include "../wubu_theme.h"

#include <string.h>
#include <stdio.h>
#include <time.h>

#define WID_PANEL_W  360
#define WID_PANEL_X  8
#define WID_PANEL_Y  8
#define WID_CARD_PAD 10
#define WID_CARD_GAP 10

static bool g_open = false;

void win11_widgets_toggle(void) { g_open = !g_open; }
bool win11_widgets_is_open(void) { return g_open; }
void win11_widgets_close(void)   { g_open = false; }

static void render_card(uint32_t *fb, int x, int y, int w, int h,
                        const char *title) {
    const WubuThemeColors *c = tc();
    vbe_fill_rect_rounded(x, y, w, h, 8, c->startmenu_bg);
    vbe_rect_rounded(x, y, w, h, 8, c->border_light);
    vbe_draw_text(x + WID_CARD_PAD, y + 6, title, c->startmenu_text, 1);
}

void win11_widgets_render(uint32_t *fb, int fb_w, int fb_h) {
    if (!g_open) return;
    const WubuThemeColors *c = tc();

    int pw = WID_PANEL_W;
    if (pw > fb_w - 16) pw = fb_w - 16;
    int ph = fb_h - 2 * WID_PANEL_Y;

    vbe_fill_rect_rounded(WID_PANEL_X, WID_PANEL_Y, pw, ph, 10,
                          c->startmenu_bg);
    vbe_rect_rounded(WID_PANEL_X, WID_PANEL_Y, pw, ph, 10,
                     c->border_light);
    vbe_draw_text(WID_PANEL_X + WID_CARD_PAD, WID_PANEL_Y + 10,
                  "Widgets", c->startmenu_text, 2);

    int cx = WID_PANEL_X + WID_CARD_PAD;
    int cw = pw - 2 * WID_CARD_PAD;
    int cy = WID_PANEL_Y + 40;

    /* Clock + date card. */
    int clock_h = 92;
    render_card(fb, cx, cy, cw, clock_h, "Clock");
    time_t now = time(NULL);
    struct tm tmv;
    localtime_r(&now, &tmv);
    char buf[64];
    snprintf(buf, sizeof(buf), "%02d:%02d", tmv.tm_hour, tmv.tm_min);
    vbe_draw_text(cx + WID_CARD_PAD, cy + 22, buf, c->startmenu_text, 3);
    strftime(buf, sizeof(buf), "%A, %B %e", &tmv);
    vbe_draw_text(cx + WID_CARD_PAD, cy + 58, buf, c->startmenu_text, 1);
    cy += clock_h + WID_CARD_GAP;

    /* Weather card (placeholder -- no forecast source yet). */
    int wx_h = 76;
    render_card(fb, cx, cy, cw, wx_h, "Weather");
    vbe_draw_text(cx + WID_CARD_PAD, cy + 24, "Partly cloudy", c->startmenu_text, 1);
    vbe_draw_text(cx + WID_CARD_PAD, cy + 42, "21 C  /  14 C", c->startmenu_text, 2);
    cy += wx_h + WID_CARD_GAP;

    /* Live machine stats -- actually read from the host sensors.  This used
     * to print whatever was left in the driver state, which nothing ever wrote,
     * so the card showed 0 C forever regardless of how hot the machine ran. */
    wubu_thermal_refresh();

    int sys_h = 92;
    render_card(fb, cx, cy, cw, sys_h, "System");
    int cpu = wubu_thermal_read_temp(WUBU_THERMAL_CPU);
    int gpu = wubu_thermal_read_temp(WUBU_THERMAL_GPU);
    /* A missing sensor prints "n/a".  Printing 0 would be a temperature
     * reading, and we do not have one. */
    if (cpu >= 0) snprintf(buf, sizeof(buf), "CPU   %d C", cpu);
    else          snprintf(buf, sizeof(buf), "CPU   n/a");
    vbe_draw_text(cx + WID_CARD_PAD, cy + 24, buf, c->startmenu_text, 1);
    if (gpu >= 0) snprintf(buf, sizeof(buf), "GPU   %d C", gpu);
    else          snprintf(buf, sizeof(buf), "GPU   n/a");
    vbe_draw_text(cx + WID_CARD_PAD, cy + 40, buf, c->startmenu_text, 1);
    int nvme = wubu_thermal_nvme();
    if (nvme >= 0) snprintf(buf, sizeof(buf), "SSD   %d C", nvme);
    else           snprintf(buf, sizeof(buf), "SSD   n/a");
    vbe_draw_text(cx + WID_CARD_PAD, cy + 56, buf, c->startmenu_text, 1);
}

bool win11_widgets_click(int x, int y) {
    if (!g_open) return false;
    int pw = WID_PANEL_W;
    if (pw > wm_screen_w() - 16) pw = wm_screen_w() - 16;
    int ph = wm_screen_h() - 2 * WID_PANEL_Y;

    if (x < WID_PANEL_X || x >= WID_PANEL_X + pw ||
        y < WID_PANEL_Y || y >= WID_PANEL_Y + ph) {
        g_open = false;
        return false;      /* closed, but the press still belongs to the desktop */
    }
    return true;           /* inside the board: consume */
}
