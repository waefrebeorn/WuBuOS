/*
 * dosgui_wm_tray_world.c -- the WORLD-STATE tray indicators (the
 * SteamOS desktop's tray: battery, network, heat — in our code).
 *
 * The systray already hosts the daemon icons (archd/holyd). The
 * missing SteamOS-parity surface is the LIVE hardware tray: the
 * battery percent, the wifi link, the CPU temp. This module wires the
 * world-state (wubu_world) into three tray icons:
 *
 *   battery  — the percent + the charging state (green charging /
 *              red low / amber otherwise)
 *   network  — wifi link on/off (blue on / grey off)
 *   heat     — the cpu temp + the fan duty (the icon color shifts
 *              with the throttle flag)
 *
 * The world-state provider is injectable (the desktop wires the real
 * wubu_world_snapshot; the test injects a fake).
 * C11.
 */
#include "dosgui_wm.h"
#include "wubu_world.h"

#include <stdio.h>
#include <string.h>

/* the world snapshot provider (injectable) */
typedef const wubu_world_t *(*WorldSnapFn)(void);

static WorldSnapFn g_snap;
static int g_wired = 0;

/* the tray icon names (registered once) */
#define TRAY_BATTERY "tray-battery"
#define TRAY_NETWORK "tray-network"
#define TRAY_HEAT    "tray-heat"

/* TW1: the provider override (the tests). */
void dosgui_tray_world_set_snapshot(WorldSnapFn fn)
{
    g_snap = fn;
}

/* TW2: wire the three indicators into the systray (idempotent). */
void dosgui_tray_world_wire(void)
{
    if (g_wired) return;
    dosgui_systray_add(TRAY_BATTERY, 0xFF40B040, NULL, NULL);
    dosgui_systray_add(TRAY_NETWORK, 0xFF4080D0, NULL, NULL);
    dosgui_systray_add(TRAY_HEAT,    0xFFD08040, NULL, NULL);
    g_wired = 1;
}

/* TW3: refresh the indicators from the world state. */
void dosgui_tray_world_refresh(void)
{
    const wubu_world_t *w = g_snap ? g_snap() : NULL;
    if (!w) return;
    /* the battery: green charging / red low / amber otherwise */
    uint32_t bat_color;
    if (w->battery_charging)      bat_color = 0xFF40B040;
    else if (w->battery_pct < 20) bat_color = 0xFFD04040;
    else                          bat_color = 0xFFD0A040;
    dosgui_systray_update_color(TRAY_BATTERY, bat_color);
    /* the network: blue link up / grey down */
    dosgui_systray_update_color(TRAY_NETWORK,
                                w->wifi_link ? 0xFF4080D0 : 0xFF808080);
    /* the heat: red throttled / amber hot / green cool */
    uint32_t heat_color;
    if (w->throttled)             heat_color = 0xFFD03030;
    else if (w->cpu_temp >= 80)   heat_color = 0xFFD0A030;
    else                          heat_color = 0xFF30C060;
    dosgui_systray_update_color(TRAY_HEAT, heat_color);
}

/* TW4: the test hooks */
typedef struct {
    int wired;
    uint32_t battery_color;
    uint32_t network_color;
    uint32_t heat_color;
} dosgui_tray_world_view_t;

int dosgui_tray_world_get(dosgui_tray_world_view_t *out)
{
    if (!out) return -1;
    out->wired = g_wired;
    out->battery_color = dosgui_systray_color(TRAY_BATTERY);
    out->network_color = dosgui_systray_color(TRAY_NETWORK);
    out->heat_color = dosgui_systray_color(TRAY_HEAT);
    return 0;
}
