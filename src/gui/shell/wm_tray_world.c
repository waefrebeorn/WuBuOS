/*
 * wm_tray_world.c -- the WORLD-STATE tray indicators (the
 * SteamOS desktop's tray: battery, network, heat — in our code).
 *
 * The systray already hosts the daemon icons (archd/holyd). The
 * missing SteamOS-parity surface is the LIVE hardware tray: the
 * battery percent, the wifi link, the CPU temp. This module wires the
 * world-state (wubu_world) into three tray icons:
 *
 *   battery  — the percent + the charging state (green charging /
 *              red low / amber otherwise)
 *   network  — ANY link up/down (blue on / grey off), and a click opens the
 *              network panel
 *   heat     — the cpu temp + the fan duty (the icon color shifts
 *              with the throttle flag)
 *
 * The world-state provider is injectable (the desktop wires the real
 * wubu_world_snapshot; the test injects a fake).
 * C11.
 */
#include "../wm.h"
#include "../../kernel/wubu_world.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

/* the world snapshot provider (injectable) */
typedef const wubu_world_t *(*WorldSnapFn)(void);

static WorldSnapFn g_snap;
static int g_wired = 0;

/* the tray icon names (registered once) */
#define TRAY_BATTERY "tray-battery"
#define TRAY_NETWORK "tray-network"
#define TRAY_HEAT    "tray-heat"

/* The world-state provider: the desktop's real snapshot on the product, absent in
 * a test graph that never links the kernel's world module. Weak so the module can
 * be linked anywhere; tray_world_wire() wires it only when it exists. */
extern const wubu_world_t *wubu_world_snapshot(void) __attribute__((weak));

/* Clicking the network icon opens the network surface. Windows opens a
 * connectivity flyout here; this OS already ships that surface -- the quick
 * settings panel with its WiFi/Ethernet rows -- so the icon opens THAT. A
 * second panel that repeated those two rows would be two bugs per fix. */
static void tray_network_click(void) { quick_settings_toggle(); }

/* TW1: the provider override (the tests). */
void tray_world_set_snapshot(WorldSnapFn fn)
{
    g_snap = fn;
}

/* TW2: wire the three indicators into the systray (idempotent). */
void tray_world_wire(void)
{
    if (g_wired) return;
    /* Wire the REAL world snapshot unless a test overrode it first. */
    if (!g_snap && wubu_world_snapshot) g_snap = wubu_world_snapshot;
    systray_add(TRAY_BATTERY, 0xFF40B040, NULL, NULL);
    systray_add(TRAY_NETWORK, 0xFF4080D0, tray_network_click, NULL);
    systray_add(TRAY_HEAT,    0xFFD08040, NULL, NULL);
    g_wired = 1;
}

/* TW3: refresh the indicators from the world state. */
void tray_world_refresh(void)
{
    const wubu_world_t *w = g_snap ? g_snap() : NULL;
    if (!w) return;
    /* the battery: green charging / red low / amber otherwise */
    uint32_t bat_color;
    if (w->battery_charging)      bat_color = 0xFF40B040;
    else if (w->battery_pct < 20) bat_color = 0xFFD04040;
    else                          bat_color = 0xFFD0A040;
    systray_update_color(TRAY_BATTERY, bat_color);
    /* The network: link up / no link. This read wifi_link ALONE, so a machine on
     * ethernet showed the same grey dot as an unplugged one -- the icon disagreed
     * with the quick-settings row that had the truth. Any link counts. */
    systray_update_color(TRAY_NETWORK,
                         (w->wifi_link || w->eth_link) ? 0xFF4080D0 : 0xFF808080);
    /* the heat: red throttled / amber hot / green cool */
    uint32_t heat_color;
    if (w->throttled)             heat_color = 0xFFD03030;
    else if (w->cpu_temp >= 80)   heat_color = 0xFFD0A030;
    else                          heat_color = 0xFF30C060;
    systray_update_color(TRAY_HEAT, heat_color);
}

/* TW3b: the frame hook. Refreshing on every frame would read the world state (sysfs on
 * hosted) sixty times a second for three indicators that change at human speed, so this
 * is throttled to once a second -- a duration, not a frame count. */
void tray_world_tick(void)
{
    static time_t last;
    time_t now = time(NULL);
    if (now == last) return;
    last = now;
    tray_world_refresh();
}

/* TW4: the test hooks */
typedef struct {
    int wired;
    uint32_t battery_color;
    uint32_t network_color;
    uint32_t heat_color;
} tray_world_view_t;

int tray_world_get(tray_world_view_t *out)
{
    if (!out) return -1;
    out->wired = g_wired;
    out->battery_color = systray_color(TRAY_BATTERY);
    out->network_color = systray_color(TRAY_NETWORK);
    out->heat_color = systray_color(TRAY_HEAT);
    return 0;
}
