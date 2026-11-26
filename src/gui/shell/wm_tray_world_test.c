/*
 * wm_tray_world_test.c -- the live hardware indicators, and the two bugs they had.
 *
 * The module was compiled into the product but in NO build list's caller set: nothing ever
 * called tray_world_wire(), so the battery/network/heat icons were never registered and the
 * tray showed only the daemon dots. And once wired, the network dot read wifi_link ALONE, so
 * a machine on ETHERNET showed the same grey dot as an unplugged one.
 *
 * Neither failure is visible in a screenshot of a wired-idle box, so this drives the module
 * with a fake world snapshot: wired-only, wireless-only, both, neither -- plus the click
 * handler that must open the network surface.
 *
 * The systray registry is this test's own (the module only calls add/update/colour), so the
 * assertions are about the colours the module CHOSE, not about the renderer.
 *
 * C11.
 */
#include <stdio.h>
#include <string.h>

#include "../wm.h"
#include "wm_tray_world.h"
#include "../../kernel/wubu_world.h"

static int g_pass, g_fail;

#define CHECK(cond, msg)                                                      \
    do {                                                                      \
        if (cond) { g_pass++; }                                               \
        else { g_fail++; fprintf(stderr, "  FAIL: %s\n", (msg)); }            \
    } while (0)

/* ---- the systray registry the module talks to --------------------------- */
#define TEST_MAX_ICONS 8
typedef struct { char name[32]; uint32_t color; void (*on_click)(void); int live; } Icon;
static Icon g_icons[TEST_MAX_ICONS];
static int  g_icon_count;
static int  g_quick_settings_toggles;

int systray_add(const char *name, uint32_t color, void (*on_click)(void), void (*on_right)(void))
{
    (void)on_right;
    if (g_icon_count >= TEST_MAX_ICONS) return -1;
    Icon *i = &g_icons[g_icon_count];
    memset(i, 0, sizeof *i);
    strncpy(i->name, name, sizeof(i->name) - 1);
    i->color = color;
    i->on_click = on_click;
    i->live = 1;
    return g_icon_count++;
}

int systray_update_color(const char *name, uint32_t color)
{
    for (int i = 0; i < g_icon_count; i++)
        if (strcmp(g_icons[i].name, name) == 0) { g_icons[i].color = color; return 0; }
    return -1;
}

uint32_t systray_color(const char *name)
{
    for (int i = 0; i < g_icon_count; i++)
        if (strcmp(g_icons[i].name, name) == 0) return g_icons[i].color;
    return 0;
}

void systray_remove(const char *name) { (void)name; }

/* The network icon's click must reach the panel that already shows the rows. */
void quick_settings_toggle(void) { g_quick_settings_toggles++; }

/* ---- the fake world ---------------------------------------------------- */
static wubu_world_t g_world;
static const wubu_world_t *fake_snapshot(void) { return &g_world; }

static Icon *find(const char *name)
{
    for (int i = 0; i < g_icon_count; i++)
        if (strcmp(g_icons[i].name, name) == 0) return &g_icons[i];
    return NULL;
}

int main(void)
{
    printf("=== wm_tray_world_test (live indicators actually register and reflect state) ===\n");

    tray_world_set_snapshot(fake_snapshot);
    tray_world_wire();

    /* 1. THE REGISTRATION GAP: all three indicators must exist after wire(). */
    CHECK(find("tray-battery") != NULL, "the battery indicator is registered");
    CHECK(find("tray-network") != NULL, "the network indicator is registered");
    CHECK(find("tray-heat") != NULL, "the heat indicator is registered");
    CHECK(tray_world_get(&(tray_world_view_t){0}) == 0, "the view is readable once wired");

    /* 2. The network colour must follow ANY link, not wifi alone. */
    memset(&g_world, 0, sizeof g_world);
    g_world.battery_pct = 80;
    tray_world_refresh();
    uint32_t down = systray_color("tray-network");

    g_world.wifi_link = 0; g_world.eth_link = 1;          /* WIRED ONLY: used to render "down" */
    tray_world_refresh();
    uint32_t wired = systray_color("tray-network");
    CHECK(wired != down, "an ethernet link is not drawn as disconnected");
    CHECK(wired == 0xFF4080D0u || wired == down, "the up colour is the link colour");

    g_world.wifi_link = 1; g_world.eth_link = 0;          /* wireless only */
    tray_world_refresh();
    CHECK(systray_color("tray-network") == wired, "wifi and ethernet draw the same 'connected' dot");

    g_world.wifi_link = 0; g_world.eth_link = 0;          /* nothing */
    tray_world_refresh();
    CHECK(systray_color("tray-network") == down, "no link at all is drawn as disconnected");

    /* 3. The battery indicator follows the charge state, not a constant. */
    g_world.battery_pct = 5;  g_world.battery_charging = 0;
    tray_world_refresh();
    uint32_t low = systray_color("tray-battery");
    g_world.battery_pct = 90; g_world.battery_charging = 1;
    tray_world_refresh();
    uint32_t charging = systray_color("tray-battery");
    CHECK(low != charging, "the battery dot changes between low and charging");

    /* 4. The heat indicator follows the throttle flag. */
    g_world.cpu_temp = 40; g_world.throttled = 0;
    tray_world_refresh();
    uint32_t cool = systray_color("tray-heat");
    g_world.throttled = 1;
    tray_world_refresh();
    CHECK(systray_color("tray-heat") != cool, "the heat dot changes when throttled");

    /* 5. Clicking the network icon opens the network surface (it did nothing before). */
    Icon *net = find("tray-network");
    CHECK(net && net->on_click != NULL, "the network icon has a click handler");
    if (net && net->on_click) {
        int before = g_quick_settings_toggles;
        net->on_click();
        CHECK(g_quick_settings_toggles == before + 1,
              "clicking the network icon opens the quick-settings panel");
    }

    /* 6. wire() is idempotent -- a second boot must not add a second set of icons. */
    int count_before = g_icon_count;
    tray_world_wire();
    CHECK(g_icon_count == count_before, "wiring twice does not duplicate the indicators");

    printf("\n Results: %d/%d passed, %d failed\n", g_pass, g_pass + g_fail, g_fail);
    return g_fail ? 1 : 0;
}
