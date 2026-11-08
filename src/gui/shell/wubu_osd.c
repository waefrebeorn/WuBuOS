/*
 * wubu_osd.c -- system volume / brightness state and its overlay.
 *
 * See wubu_osd.h.  The overlay is a single centred card: a label, a bar, and
 * the percentage.  It fades out over ~1.2 s so it reads as a transient
 * notification rather than a dialog.
 */

#include "wubu_osd.h"
#include "../dosgui_wm.h"
#include "../dosgui_wm_internal.h"
#include "../kernel/vbe.h"
#include "../wubu_theme.h"

#include <stdio.h>
#include <string.h>

/* -- State --------------------------------------------------------- */

static float g_volume   = 0.8f;
static bool  g_muted    = false;
static float g_brightness = 1.0f;

/* Overlay: >0 means visible, counts down in frames. */
static int   g_osd_frames = 0;
static char  g_osd_label[32] = "";
static int   g_osd_pct    = 0;

#define OSD_FRAMES  72          /* ~1.2 s at 60 fps */
#define OSD_WIDTH   260
#define OSD_HEIGHT  64
#define OSD_BAR_W   180
#define OSD_BAR_H   10

/* -- Levels -------------------------------------------------------- */

void wubu_osd_set_volume(float v) {
    if (v < 0.0f) v = 0.0f;
    if (v > 1.0f) v = 1.0f;
    g_volume = v;
    if (v > 0.0f && g_muted) g_muted = false;
}

float wubu_osd_volume(void) { return g_volume; }

void wubu_osd_set_muted(bool muted) { g_muted = muted; }
bool wubu_osd_muted(void)           { return g_muted; }

void wubu_osd_set_brightness(float v) {
    if (v < 0.2f) v = 0.2f;
    if (v > 1.0f) v = 1.0f;
    g_brightness = v;
}

float wubu_osd_brightness(void) { return g_brightness; }

/* -- Overlay ------------------------------------------------------- */

void wubu_osd_notify(void) {
    if (g_muted) {
        snprintf(g_osd_label, sizeof(g_osd_label), "Muted");
        g_osd_pct = 0;
    } else {
        snprintf(g_osd_label, sizeof(g_osd_label), "%s",
                 g_brightness != 1.0f ? "Brightness" : "Volume");
        int pct = (int)(g_volume * 100.0f + 0.5f);
        g_osd_pct = pct;
    }
    g_osd_frames = OSD_FRAMES;
}

void wubu_osd_tick(void) {
    if (g_osd_frames > 0) g_osd_frames--;
}

bool wubu_osd_is_visible(void) { return g_osd_frames > 0; }

void wubu_osd_render(uint32_t *fb, int fb_w, int fb_h) {
    (void)fb;
    if (g_osd_frames <= 0) return;

    const WubuThemeColors *c = tc();

    /* Fade over the last third of the lifetime. */
    int alpha = g_osd_frames;
    if (alpha > OSD_FRAMES / 3) alpha = OSD_FRAMES / 3;

    int x = (fb_w - OSD_WIDTH) / 2;
    int y = fb_h - taskbar_height_dynamic() - OSD_HEIGHT - 40;

    vbe_fill_rect_rounded(x, y, OSD_WIDTH, OSD_HEIGHT, 8, c->win_face);
    vbe_rect_rounded(x, y, OSD_WIDTH, OSD_HEIGHT, 8, c->border_light);

    /* Label. */
    vbe_draw_text(x + 16, y + 10, g_osd_label, c->btn_text, 1);

    /* Track + fill. */
    int bx = x + 16;
    int by = y + 34;
    vbe_fill_rect_rounded(bx, by, OSD_BAR_W, OSD_BAR_H, 4, c->scroll_track);
    int fill = (OSD_BAR_W * alpha * g_osd_pct) / (OSD_FRAMES / 3) / 100;
    if (fill > OSD_BAR_W) fill = OSD_BAR_W;
    if (fill > 0)
        vbe_fill_rect_rounded(bx, by, fill, OSD_BAR_H, 4, c->select_bg);

    /* Percentage. */
    char pct[16];
    snprintf(pct, sizeof(pct), "%d%%", g_osd_pct);
    vbe_draw_text(bx + OSD_BAR_W + 12, by - 2, pct, c->btn_text, 1);
}
