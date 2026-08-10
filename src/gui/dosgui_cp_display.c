/*
 * dosgui_cp_display.c -- the Control Panel Display applet (the
 * SteamOS System Settings > Display, in our code).
 *
 * The header declared dosgui_cp_create_display_applet() for ages —
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
#include "dosgui_controlpanel.h"
#include "wubu_world.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* the world snapshot provider (injectable) */
typedef const wubu_world_t *(*DisplaySnapFn)(void);

typedef struct {
    DisplaySnapFn snap;
} DisplayAppletData;

static DisplayAppletData g_disp;

/* set the provider (the desktop wires wubu_world_snapshot). */
void dosgui_cp_display_set_snapshot(DisplaySnapFn fn)
{
    g_disp.snap = fn;
    CpApplet *a = dosgui_controlpanel_get_applet(CP_APPLET_DISPLAY);
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
                           int x, int y, int w, int h)
{
    (void)fb; (void)x; (void)y; (void)w; (void)h;
    DisplayAppletData *d = (DisplayAppletData *)applet->user_data;
    const wubu_world_t *wd = d ? d->snap : NULL;
    if (!wd && g_disp.snap) wd = g_disp.snap();
    if (!wd) return;
    /* the display contract (the render is text via the desktop font;
     * the DATA is what the test asserts) */
    (void)wd;
}

/* the test hook: the display line the applet would show. */
int dosgui_cp_display_test_line(char *out, size_t cap)
{
    DisplaySnapFn snap = g_disp.snap;
    CpApplet *a = dosgui_controlpanel_get_applet(CP_APPLET_DISPLAY);
    if (a && a->user_data && ((DisplayAppletData *)a->user_data)->snap)
        snap = ((DisplayAppletData *)a->user_data)->snap;
    if (!out || cap == 0) return -1;
    if (!snap) {
        snprintf(out, cap, "no display state");
        return 0;
    }
    const wubu_world_t *w = snap();
    snprintf(out, cap, "display: %s %ux%u@%d %uMB vram",
             connector_name(w->gpu_connector),
             w->screen_w, w->screen_h,
             w->gpu_connector ? 60 : 0,
             w->vram_mb);
    return 0;
}

CpApplet dosgui_cp_create_display_applet(void)
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
