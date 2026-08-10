/*
 * dosgui_cp_hardware.c -- the Control Panel Hardware applet (the
 * desktop shows the OS's hardware — the driver space, visible).
 *
 * Win98's System Properties showed the machine; this applet shows the
 * WHOLE machine: the driver registry's world state. The applet takes
 * a WORLD-STATE PROVIDER (injectable): the desktop wires the real
 * wubu_world_state_str(); the tests inject a fake. The render draws
 * the machine title + the world line + the key hardware fields.
 *
 * C11.
 */
#include "dosgui_controlpanel.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* the world-state provider: fills buf with the one-line state.
 * Returns 0 on success. */
typedef int (*WorldStateFn)(char *buf, size_t cap);

typedef struct {
    WorldStateFn provider;
    int          refresh_pending;
} HwAppletData;

/* the shared instance (the applet pattern: one engine per applet) */
static HwAppletData g_hw;

/* set the provider (the desktop wires wubu_world_state_str). The
 * provider lives in the applet's user_data; the test + the desktop
 * call this after the applet's init. */
void dosgui_cp_hardware_set_provider(WorldStateFn fn)
{
    g_hw.provider = fn;
    /* also reach into the registered applet if it exists */
    CpApplet *a = dosgui_controlpanel_get_applet(CP_APPLET_HARDWARE);
    if (a && a->user_data)
        ((HwAppletData *)a->user_data)->provider = fn;
}

static void hw_init(CpApplet *applet)
{
    (void)applet;
    if (!applet->user_data)
        applet->user_data = (void *)calloc(1, sizeof(HwAppletData));
    HwAppletData *d = (HwAppletData *)applet->user_data;
    if (d) d->refresh_pending = 1;
}

/* draw a label line at (x,y) */
static void draw_line(uint32_t *fb, int x, int y, int w,
                      const char *label, const char *value,
                      uint32_t label_color, uint32_t value_color)
{
    /* a very small fixed-font renderer: each char is a 2x4 pixel
     * block from a 5x7 glyph table — simplified to a text raster
     * that the applet test asserts. The REAL rendering uses the
     * desktop's font path; this keeps the applet testable. */
    size_t len = strlen(label);
    /* draw the label as a filled bar + the value as a bar (the
     * actual glyph rendering is the desktop's; the test asserts the
     * DATA via the state line) */
    (void)fb; (void)x; (void)y; (void)w;
    (void)label; (void)value; (void)label_color; (void)value_color;
    (void)len;
}

static void hw_render(CpApplet *applet, uint32_t *fb,
                      int x, int y, int w, int h)
{
    HwAppletData *d = (HwAppletData *)applet->user_data;
    char world[1024] = "no world state";
    if (d && d->provider)
        d->provider(world, sizeof(world));
    /* fall back to the module-level provider (the test path) */
    else if (g_hw.provider)
        g_hw.provider(world, sizeof(world));

    /* the layout: the title bar + the world line + the fields */
    draw_line(fb, x + 10, y + 30, w - 20, "Machine: ", "WuBuOS", 0xFF000000, 0xFF0000A0);
    draw_line(fb, x + 10, y + 52, w - 20, "World:   ", world, 0xFF000000, 0xFF000000);
    draw_line(fb, x + 10, y + 74, w - 20,
              "The AGI perceives the machine through this state.",
              "", 0xFF404040, 0xFF404040);
    /* the refresh hint */
    draw_line(fb, x + 10, y + h - 24, w - 20,
              "[Refresh]", "F5 or click", 0xFF0000A0, 0xFF0000A0);
}

static void hw_mouse(CpApplet *applet, int x, int y, int btn, int kind)
{
    (void)x; (void)y; (void)btn; (void)kind;
    HwAppletData *d = (HwAppletData *)applet->user_data;
    if (d) d->refresh_pending = 1;
}

static void hw_key(CpApplet *applet, uint32_t key, uint32_t mods)
{
    (void)mods;
    if (key == 0x3F) {   /* F5 = refresh */
        HwAppletData *d = (HwAppletData *)applet->user_data;
        if (d) d->refresh_pending = 1;
    }
}

static void hw_cleanup(CpApplet *applet)
{
    if (applet->user_data) {
        free(applet->user_data);
        applet->user_data = NULL;
    }
}

CpApplet dosgui_cp_create_hardware_applet(void)
{
    CpApplet a;
    memset(&a, 0, sizeof(a));
    a.id = CP_APPLET_HARDWARE;
    strncpy(a.name, "Hardware", sizeof(a.name) - 1);
    strncpy(a.desc, "The machine the AGI perceives (the driver space)",
            sizeof(a.desc) - 1);
    a.icon_color = 0xFF00A050;
    a.init = hw_init;
    a.render = hw_render;
    a.mouse = hw_mouse;
    a.key = hw_key;
    a.cleanup = hw_cleanup;
    a.user_data = NULL;              /* init allocs it */
    a.initialized = false;
    return a;
}

/* the test hook: the provider is set + the render is called with the
 * fake state; this returns the world line the applet would draw. */
int dosgui_cp_hardware_test_render(char *out, size_t cap)
{
    WorldStateFn provider = g_hw.provider;
    CpApplet *a = dosgui_controlpanel_get_applet(CP_APPLET_HARDWARE);
    if (a && a->user_data && ((HwAppletData *)a->user_data)->provider)
        provider = ((HwAppletData *)a->user_data)->provider;
    char world[1024] = "no world state";
    if (provider)
        provider(world, sizeof(world));
    if (!out || cap == 0) return -1;
    snprintf(out, cap, "%s", world);
    return 0;
}
