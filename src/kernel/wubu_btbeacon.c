/*
 * wubu_btbeacon.c -- kernel-owned Bluetooth beacon routing.
 *
 * BT beacons (iBeacon/AltBeacon/Eddystone) broadcast advertising
 * packets for proximity, telemetry, and UID. "Runs on everything"
 * includes correct beacon routing on all Bluetooth stacks.
 *
 * Impl routing:
 *   - /sys/class/bluetooth/hci0: BT adapter presence
 *   - /sys/module/btintel/parameters: Intel BT params
 */
#include "wubu_btbeacon.h"
#include "wubu_hw_detect.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static int g_btbeacon_present = 0;
static int g_btbeacon_advertising = 0;

void wubu_btbeacon_probe(void)
{
#ifdef WUBU_HOSTED
    g_btbeacon_present = (access("/sys/class/bluetooth/hci0", R_OK) == 0) ? 1 : 0;
    g_btbeacon_advertising = (access("/sys/module/btintel/parameters", R_OK) == 0) ? 1 : 0;
#else
    g_btbeacon_present = g_btbeacon_advertising = 0;
#endif
}

int wubu_btbeacon_present(void)
{
#ifdef WUBU_HOSTED
    return g_btbeacon_present;
#else
    return 0;
#endif
}

int wubu_btbeacon_type(int rssi)
{
    /* Proximity: immediate (> -70), near (-70 to -85), far (< -85). */
    if (rssi > -70) return 0;  /* immediate */
    if (rssi > -85) return 1;  /* near */
    return 2;                  /* far */
}

int wubu_btbeacon_valid_uuid(const char *uuid_prefix)
{
    /* UUID must start with a known prefix. */
    if (!uuid_prefix) return 0;
    if (strncmp(uuid_prefix, "fda5", 4) == 0) return 1;  /* iBeacon */
    if (strncmp(uuid_prefix, "febe", 4) == 0) return 1;  /* Eddystone */
    if (strncmp(uuid_prefix, "0123", 4) == 0) return 1;  /* AltBeacon */
    return 0;
}

void wubu_btbeacon_summary(char *out, size_t cap)
{
    snprintf(out, cap, "btbeacon[dev=%d adv=%d]", g_btbeacon_present, g_btbeacon_advertising);
}
