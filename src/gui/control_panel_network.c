/*
 * control_panel_network.c -- the Control Panel Network applet (the
 * SteamOS System Settings > Network, in our code).
 *
 * The header declared cp_create_network_applet() for ages;
 * the implementation never existed. This is it: the applet reads the
 * WORLD state (the driver registry's live network) and shows the
 * network contract:
 *
 *   - the wifi link (up/down) + the eth link
 *   - the presence of the NICs
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
typedef const wubu_world_t *(*NetworkSnapFn)(void);

typedef struct {
    NetworkSnapFn snap;
} NetworkAppletData;

static NetworkAppletData g_net;
static char g_last_drawn[256];   /* the rows this page last drew */

/* set the provider (the desktop wires wubu_world_snapshot). */
void cp_network_set_snapshot(NetworkSnapFn fn)
{
    g_net.snap = fn;
    CpApplet *a = controlpanel_get_applet(CP_APPLET_NETWORK);
    if (a && a->user_data)
        ((NetworkAppletData *)a->user_data)->snap = fn;
}

static void network_init(CpApplet *applet)
{
    if (!applet->user_data)
        applet->user_data = (void *)calloc(1, sizeof(NetworkAppletData));
}

static void network_render(CpApplet *applet, uint32_t *fb,
                           int fb_w, int fb_h, int x, int y, int w, int h)
{
    NetworkAppletData *d = (NetworkAppletData *)applet->user_data;
    const wubu_world_t *wd = d ? d->snap : NULL;
    if (!wd && g_net.snap) wd = g_net.snap();
    /* No product code installs a provider any more: resolve the live world directly, and treat the
     * injected provider as a test override (an unowned pointer gave one live session a world that was not
     * the world). */
    {
        extern const wubu_world_t *wubu_world_snapshot(void) __attribute__((weak));
        if (!wd && wubu_world_snapshot) wd = wubu_world_snapshot();
    }

    /* Colours come from the theme: this page used to paint a near-white card in a dark-only OS,
     * which is exactly how one panel read as bootleg next to every other. */
    const WubuThemeColors *c = wubu_theme_colors();
    cp_fill(fb, fb_w, fb_h, x, y, w, h, c->win_face);
    cp_draw_text(fb, fb_w, fb_h, x + 14, y + 12, applet->name, c->select_bg, 2);
    if (!wd) {
        cp_draw_text(fb, fb_w, fb_h, x + 14, y + 44,
                     "no network snapshot", c->body_text_ina, 1);
        return;
    }
    /* Rows, like the Display page: one sentence and a large empty card reads as unfinished. Every value
     * is either measured or says it is not -- and a link is only claimed when the world says so. */
    const char *rows[3][2];
    char v0[80], v1[80], v2[80];
    snprintf(v0, sizeof v0, "%s", wd->has_wifi ? (wd->wifi_link ? "present, link up" : "present, no link")
                                               : "not reported");
    snprintf(v1, sizeof v1, "%s", wd->has_eth ? (wd->eth_link ? "present, link up" : "present, no link")
                                              : "not reported");
    /* "down" is a measurement; "we cannot see any interface" is not. Say which one it is. */
    if ((wd->has_wifi && wd->wifi_link) || (wd->has_eth && wd->eth_link))
        snprintf(v2, sizeof v2, "up");
    else if (wd->has_wifi || wd->has_eth)
        snprintf(v2, sizeof v2, "down");
    else
        snprintf(v2, sizeof v2, "not reported");
    rows[0][0] = "Wi-Fi";   rows[0][1] = v0;
    rows[1][0] = "Ethernet"; rows[1][1] = v1;
    rows[2][0] = "Link";    rows[2][1] = v2;

    size_t off = 0;
    g_last_drawn[0] = 0;
    for (int i = 0; i < 3; i++) {
        int n = snprintf(g_last_drawn + off, sizeof g_last_drawn - off, "%s%s: %s",
                         i ? "; " : "", rows[i][0], rows[i][1]);
        if (n < 0) break;
        off += (size_t)n;
        int ry = y + 44 + i * 26;
        if (ry + 10 > y + h) break;
        cp_draw_text(fb, fb_w, fb_h, x + 14, ry, rows[i][0], c->body_text_ina, 1);
        cp_draw_text(fb, fb_w, fb_h, x + 150, ry, rows[i][1], c->body_text, 1);
    }
}

/* the test hook: the network line the applet would show. */
int cp_network_test_line(char *out, size_t cap)
{
    NetworkSnapFn snap = g_net.snap;
    CpApplet *a = controlpanel_get_applet(CP_APPLET_NETWORK);
    if (a && a->user_data && ((NetworkAppletData *)a->user_data)->snap)
        snap = ((NetworkAppletData *)a->user_data)->snap;
    if (!out || cap == 0) return -1;
    if (!snap) {
        snprintf(out, cap, "no network state");
        return 0;
    }
    const wubu_world_t *w = snap();
    snprintf(out, cap, "network: wifi %s (%s) eth %s",
             w->wifi_link ? "UP" : "down",
             w->has_wifi ? "present" : "absent",
             w->eth_link ? "UP" : "down");
    return 0;
}

CpApplet cp_create_network_applet(void)
{
    CpApplet a;
    memset(&a, 0, sizeof(a));
    a.id = CP_APPLET_NETWORK;
    strncpy(a.name, "Network", sizeof(a.name) - 1);
    strncpy(a.desc, "The network the AGI is connected to (the world state)",
            sizeof(a.desc) - 1);
    a.icon_color = 0xFF30A0D0;
    a.init = network_init;
    a.render = network_render;
    a.mouse = NULL;
    a.key = NULL;
    a.cleanup = NULL;
    a.user_data = NULL;
    a.initialized = false;
    return a;
}

/* Test hook: the rows the Network page last drew. */
const char *cp_network_last_drawn(void) { return g_last_drawn; }
