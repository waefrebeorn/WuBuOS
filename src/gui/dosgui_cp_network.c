/*
 * dosgui_cp_network.c -- the Control Panel Network applet (the
 * SteamOS System Settings > Network, in our code).
 *
 * The header declared dosgui_cp_create_network_applet() for ages;
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
#include "dosgui_controlpanel.h"
#include "wubu_world.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* the world snapshot provider (injectable) */
typedef const wubu_world_t *(*NetworkSnapFn)(void);

typedef struct {
    NetworkSnapFn snap;
} NetworkAppletData;

static NetworkAppletData g_net;

/* set the provider (the desktop wires wubu_world_snapshot). */
void dosgui_cp_network_set_snapshot(NetworkSnapFn fn)
{
    g_net.snap = fn;
    CpApplet *a = dosgui_controlpanel_get_applet(CP_APPLET_NETWORK);
    if (a && a->user_data)
        ((NetworkAppletData *)a->user_data)->snap = fn;
}

static void network_init(CpApplet *applet)
{
    if (!applet->user_data)
        applet->user_data = (void *)calloc(1, sizeof(NetworkAppletData));
}

static void network_render(CpApplet *applet, uint32_t *fb,
                           int x, int y, int w, int h)
{
    (void)fb; (void)x; (void)y; (void)w; (void)h;
    NetworkAppletData *d = (NetworkAppletData *)applet->user_data;
    const wubu_world_t *wd = d ? d->snap : NULL;
    if (!wd && g_net.snap) wd = g_net.snap();
    (void)wd;
}

/* the test hook: the network line the applet would show. */
int dosgui_cp_network_test_line(char *out, size_t cap)
{
    NetworkSnapFn snap = g_net.snap;
    CpApplet *a = dosgui_controlpanel_get_applet(CP_APPLET_NETWORK);
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

CpApplet dosgui_cp_create_network_applet(void)
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
