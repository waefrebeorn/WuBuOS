/*
 * win11_widgets.c -- Win+W widgets board (Windows 11 parity).
 *
 * One concern: the widgets board. Cards are laid out top-to-bottom in a
 * left-anchored panel; the clock and the thermal readings are real (the
 * thermal driver is the same source the buddy uses for its OVERHEAT pose).
 * Geometry lives in wubu_tokens.h (WUBU_WIDGETS_*); the panel rect has ONE
 * computation shared by render and click.
 *
 * C11, opaque API (win11_widgets.h), no god headers.
 */

#include "wubu_gui_metrics.h"
#include "win11_widgets.h"
#include "../wm_internal.h"
#include "../wm.h"
#include "../wubu_tokens.h"
#include "../kernel/vbe.h"
#include "../kernel/wubu_drv_thermal.h"
#include "../kernel/wubu_thermal_read.h"
#include "../wubu_theme.h"

#include <string.h>
#include <stdio.h>
#include <time.h>

static bool g_open = false;

void win11_widgets_toggle(void) { g_open = !g_open; }
bool win11_widgets_is_open(void) { return g_open; }
void win11_widgets_close(void)   { g_open = false; }

/* One panel-rect owner: render and click must agree by construction. */
static void widgets_panel_rect(int scr_w, int scr_h, int *pw, int *ph) {
    int w = WUBU_WIDGETS_PANEL_W;
    if (w > scr_w - WUBU_WIDGETS_EDGE_MARGIN)
        w = scr_w - WUBU_WIDGETS_EDGE_MARGIN;
    *pw = w;
    *ph = scr_h - 2 * WUBU_WIDGETS_PANEL_Y;
}

static void render_card(uint32_t *fb, int x, int y, int w, int h,
                        const char *title) {
    const WubuThemeColors *c = tc();
    vbe_fill_rect_rounded(x, y, w, h, WUBU_WIDGETS_CARD_RADIUS,
                          c->startmenu_bg);
    vbe_rect_rounded(x, y, w, h, WUBU_WIDGETS_CARD_RADIUS, c->border_light);
    vbe_draw_text(x + WUBU_WIDGETS_CARD_PAD, y + WUBU_WIDGETS_CARD_TITLE_DY,
                  title, c->startmenu_text, 1);
}

void win11_widgets_render(uint32_t *fb, int fb_w, int fb_h) {
    if (!g_open) return;
    const WubuThemeColors *c = tc();

    int pw, ph;
    widgets_panel_rect(fb_w, fb_h, &pw, &ph);

    vbe_fill_rect_rounded(WUBU_WIDGETS_PANEL_X, WUBU_WIDGETS_PANEL_Y, pw, ph,
                          WUBU_WIDGETS_PANEL_RADIUS, c->startmenu_bg);
    vbe_rect_rounded(WUBU_WIDGETS_PANEL_X, WUBU_WIDGETS_PANEL_Y, pw, ph,
                     WUBU_WIDGETS_PANEL_RADIUS, c->border_light);
    vbe_draw_text(WUBU_WIDGETS_PANEL_X + WUBU_WIDGETS_CARD_PAD,
                  WUBU_WIDGETS_PANEL_Y + WUBU_WIDGETS_HEADER_DY,
                  "Widgets", c->startmenu_text, 2);

    int cx = WUBU_WIDGETS_PANEL_X + WUBU_WIDGETS_CARD_PAD;
    int cw = pw - 2 * WUBU_WIDGETS_CARD_PAD;
    int cy = WUBU_WIDGETS_PANEL_Y + WUBU_WIDGETS_LIST_TOP;

    /* Clock + date card. */
    int clock_h = WUBU_WIDGETS_CLOCK_H;
    render_card(fb, cx, cy, cw, clock_h, "Clock");
    time_t now = time(NULL);
    struct tm tmv;
    localtime_r(&now, &tmv);
    char buf[64];
    snprintf(buf, sizeof(buf), "%02d:%02d", tmv.tm_hour, tmv.tm_min);
    vbe_draw_text(cx + WUBU_WIDGETS_CARD_PAD, cy + WUBU_WIDGETS_CLOCK_TIME_DY,
                  buf, c->startmenu_text, 3);
    strftime(buf, sizeof(buf), "%A, %B %e", &tmv);
    vbe_draw_text(cx + WUBU_WIDGETS_CARD_PAD, cy + WUBU_WIDGETS_CLOCK_DATE_DY,
                  buf, c->startmenu_text, 1);
    cy += clock_h + WUBU_WIDGETS_CARD_GAP;

    /* Weather card: NO forecast source is wired yet. It used to print
     * "Partly cloudy / 21 C 14 C" — invented data presented as a reading.
     * An honest placeholder until a forecast source exists (see the
     * de-hardcode ledger: weather source is an open wiring item). */
    int wx_h = WUBU_WIDGETS_WEATHER_H;
    render_card(fb, cx, cy, cw, wx_h, "Weather");
    vbe_draw_text(cx + WUBU_WIDGETS_CARD_PAD, cy + WUBU_WIDGETS_WEATHER_L1_DY,
                  "No forecast source yet", c->startmenu_text, 1);
    vbe_draw_text(cx + WUBU_WIDGETS_CARD_PAD, cy + WUBU_WIDGETS_WEATHER_L2_DY,
                  "wire one to light this card", c->startmenu_text, 1);
    cy += wx_h + WUBU_WIDGETS_CARD_GAP;

    /* Live machine stats -- actually read from the host sensors. This used
     * to print whatever was left in the driver state, which nothing ever wrote,
     * so the card showed 0 C forever regardless of how hot the machine ran. */
    wubu_thermal_refresh();

    int sys_h = WUBU_WIDGETS_SYSTEM_H;
    render_card(fb, cx, cy, cw, sys_h, "System");
    int cpu = wubu_thermal_read_temp(WUBU_THERMAL_CPU);
    int gpu = wubu_thermal_read_temp(WUBU_THERMAL_GPU);
    /* A missing sensor prints "n/a". Printing 0 would be a temperature
     * reading, and we do not have one. */
    if (cpu >= 0) snprintf(buf, sizeof(buf), "CPU   %d C", cpu);
    else          snprintf(buf, sizeof(buf), "CPU   n/a");
    vbe_draw_text(cx + WUBU_WIDGETS_CARD_PAD, cy + WUBU_WIDGETS_SYSTEM_CPU_DY,
                  buf, c->startmenu_text, 1);
    if (gpu >= 0) snprintf(buf, sizeof(buf), "GPU   %d C", gpu);
    else          snprintf(buf, sizeof(buf), "GPU   n/a");
    vbe_draw_text(cx + WUBU_WIDGETS_CARD_PAD, cy + WUBU_WIDGETS_SYSTEM_GPU_DY,
                  buf, c->startmenu_text, 1);
    int nvme = wubu_thermal_nvme();
    if (nvme >= 0) snprintf(buf, sizeof(buf), "SSD   %d C", nvme);
    else           snprintf(buf, sizeof(buf), "SSD   n/a");
    vbe_draw_text(cx + WUBU_WIDGETS_CARD_PAD, cy + WUBU_WIDGETS_SYSTEM_SSD_DY,
                  buf, c->startmenu_text, 1);
    int ram = wubu_thermal_ram();
    if (ram >= 0) snprintf(buf, sizeof(buf), "RAM   %d C", ram);
    else          snprintf(buf, sizeof(buf), "RAM   n/a");
    vbe_draw_text(cx + WUBU_WIDGETS_CARD_PAD, cy + WUBU_WIDGETS_SYSTEM_RAM_DY,
                  buf, c->startmenu_text, 1);
}

bool win11_widgets_click(int x, int y) {
    if (!g_open) return false;
    int pw, ph;
    widgets_panel_rect(wm_screen_w(), wm_screen_h(), &pw, &ph);

    if (x < WUBU_WIDGETS_PANEL_X || x >= WUBU_WIDGETS_PANEL_X + pw ||
        y < WUBU_WIDGETS_PANEL_Y || y >= WUBU_WIDGETS_PANEL_Y + ph) {
        g_open = false;
        return false;      /* closed, but the press still belongs to the desktop */
    }
    return true;           /* inside the board: consume */
}