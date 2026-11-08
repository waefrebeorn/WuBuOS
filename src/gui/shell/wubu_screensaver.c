/*
 * wubu_screensaver.c -- idle screen saver.
 *
 * See wubu_screensaver.h.  Engages after a timeout with no input, dims the
 * whole screen, and shows the clock in the corner.  Any activity call wakes
 * it immediately, so a keypress or a mouse move restores the desktop.
 */

#include "wubu_screensaver.h"
#include "../dosgui_wm.h"
#include "../dosgui_wm_internal.h"
#include "../kernel/vbe.h"
#include "../wubu_theme.h"

#include <stdio.h>
#include <time.h>

#define SS_DEFAULT_TIMEOUT_MS   (5u * 60u * 1000u)

static uint32_t g_timeout_ms = SS_DEFAULT_TIMEOUT_MS;
static uint32_t g_idle_ms    = 0;
static bool     g_active     = false;

void wubu_screensaver_set_timeout_ms(uint32_t ms) {
    g_timeout_ms = (ms > 0) ? ms : SS_DEFAULT_TIMEOUT_MS;
    /* Re-evaluate immediately so lowering the timeout takes effect now. */
    if (g_idle_ms >= g_timeout_ms) g_active = true;
}

uint32_t wubu_screensaver_timeout_ms(void) { return g_timeout_ms; }

void wubu_screensaver_activity(void) {
    g_idle_ms = 0;
    g_active  = false;
}

void wubu_screensaver_tick(uint32_t delta_ms) {
    if (g_active) return;
    g_idle_ms += delta_ms;
    if (g_timeout_ms > 0 && g_idle_ms >= g_timeout_ms) g_active = true;
}

bool wubu_screensaver_active(void) { return g_active; }

void wubu_screensaver_render(uint32_t *fb, int fb_w, int fb_h) {
    if (!g_active) return;

    /* Dim to near-black.  A solid fill keeps this dependency-free. */
    vbe_fill_rect(0, 0, fb_w, fb_h, 0x0A0A0A);

    /* Clock, bottom-right, so the display is still useful. */
    time_t now = time(NULL);
    struct tm tmv;
    localtime_r(&now, &tmv);
    char clock[32];
    snprintf(clock, sizeof(clock), "%02d:%02d", tmv.tm_hour, tmv.tm_min);

    int cw = (int)strlen(clock) * 8;
    vbe_draw_text(fb_w - cw - 16, fb_h - taskbar_height_dynamic() - 24,
                  clock, 0x808080, 1);
}
