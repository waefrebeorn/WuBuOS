/*
 * control_panel_display.c -- the Control Panel Display applet (the
 * SteamOS System Settings > Display, in our code).
 *
 * The header declared cp_create_display_applet() for ages —
 * the implementation never existed (form-without-function). This is
 * it: the applet reads the WORLD state (the driver registry's live
 * snapshot) and shows the display contract:
 *
 *   - the connector (eDP / MIPI-DSI / HDMI / DP)
 *   - the current mode (WxH @ refresh)
 *   - the VRAM (the framebuffer budget)
 *   - the GPU presence
 *
 * The world provider is injectable (the desktop wires the real
 * wubu_world_snapshot; the tests inject a fake).
 * C11.
 */
#include "control_panel.h"
#include "control_panel_text.h"
#include "wubu_world.h"
#include "wubu_theme.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* the world snapshot provider (injectable) */
typedef const wubu_world_t *(*DisplaySnapFn)(void);

typedef struct {
    DisplaySnapFn snap;
} DisplayAppletData;

static DisplayAppletData g_disp;

/* What the page last DREW for its body ("" when it drew none). A page that renders only its heading
 * looks like a broken app, and the only way to gate that is to record the draw, not to count pixels. */
static char g_last_drawn[256];

/* set the provider (the desktop wires wubu_world_snapshot). */
void cp_display_set_snapshot(DisplaySnapFn fn)
{
    g_disp.snap = fn;
    CpApplet *a = controlpanel_get_applet(CP_APPLET_DISPLAY);
    if (a && a->user_data)
        ((DisplayAppletData *)a->user_data)->snap = fn;
}

static void display_init(CpApplet *applet)
{
    if (!applet->user_data)
        applet->user_data = (void *)calloc(1, sizeof(DisplayAppletData));
}

static const char *connector_name(int c)
{
    switch (c) {
    case 1: return "eDP";
    case 2: return "MIPI-DSI";
    case 3: return "HDMI";
    case 4: return "DisplayPort";
    default: return "unknown";
    }
}

static void display_render(CpApplet *applet, uint32_t *fb,
                           int fb_w, int fb_h, int x, int y, int w, int h)
{
    DisplayAppletData *d = (DisplayAppletData *)applet->user_data;
    const wubu_world_t *wd = d ? d->snap : NULL;
    if (!wd && g_disp.snap) wd = g_disp.snap();

    /* Colours come from the theme: this page used to paint a near-white card in a dark-only OS,
     * which is exactly how one panel read as bootleg next to every other. */
    const WubuThemeColors *c = wubu_theme_colors();
    cp_fill(fb, fb_w, fb_h, x, y, w, h, c->win_face);
    cp_draw_text(fb, fb_w, fb_h, x + 14, y + 12, applet->name, c->select_bg, 2);
    if (!wd) {
        cp_draw_text(fb, fb_w, fb_h, x + 14, y + 44,
                     "no display snapshot", c->body_text_ina, 1);
        return;
    }
    /* the line the test asserts is the line the user reads: one source */
    char line[256];
    /* The line builder returns 0 on success and -1 for bad arguments, so "> 0" was never true and the
     * page drew its heading and NOTHING ELSE on every install. Ask for "not an error" instead. */
    if (cp_display_test_line(line, sizeof line) >= 0) {
        snprintf(g_last_drawn, sizeof g_last_drawn, "%s", line);
        cp_text_wrapped(fb, fb_w, fb_h, x + 14, y + 44, w - 28, line, c->body_text, 1);
    }
}

/* the test hook: the display line the applet would show. */
int cp_display_test_line(char *out, size_t cap)
{
    DisplaySnapFn snap = g_disp.snap;
    CpApplet *a = controlpanel_get_applet(CP_APPLET_DISPLAY);
    if (a && a->user_data && ((DisplayAppletData *)a->user_data)->snap)
        snap = ((DisplayAppletData *)a->user_data)->snap;
    if (!out || cap == 0) return -1;
    if (!snap) {
        snprintf(out, cap, "no display state");
        return 0;
    }
    const wubu_world_t *w = snap();
    /* NO INVENTED NUMBERS. This used to print "@60" whenever a connector was present -- a refresh rate
     * nothing in the world snapshot measures (wubu_world_t has no refresh field), so the page stated a
     * fact the OS never learned. It now reports the mode and says plainly what it does not know. */
    char vram[48];
    if (w->vram_mb) snprintf(vram, sizeof vram, "%uMB vram", w->vram_mb);
    else            snprintf(vram, sizeof vram, "vram not reported");
    snprintf(out, cap, "display: %s %ux%u, refresh not reported, %s",
             connector_name(w->gpu_connector), w->screen_w, w->screen_h, vram);
    return 0;
}

CpApplet cp_create_display_applet(void)
{
    CpApplet a;
    memset(&a, 0, sizeof(a));
    a.id = CP_APPLET_DISPLAY;
    strncpy(a.name, "Display", sizeof(a.name) - 1);
    strncpy(a.desc, "The display the GPU drives (the world state)",
            sizeof(a.desc) - 1);
    a.icon_color = 0xFF4080D0;
    a.init = display_init;
    a.render = display_render;
    a.mouse = NULL;
    a.key = NULL;
    a.cleanup = NULL;
    a.user_data = NULL;
    a.initialized = false;
    return a;
}

/* Test hook: the body line the Display page last drew. */
const char *cp_display_last_drawn(void) { return g_last_drawn; }
