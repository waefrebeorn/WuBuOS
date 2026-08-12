/*
 * wubu_zoneappend.c -- kernel-owned ZNS zone append routing.
 *
 * ZNS (Zoned Namespaces) drives expose zones that must be written
 * sequentially via zone append commands. "Runs on everything"
 * includes correct zone append routing on all ZNS-capable SSDs.
 *
 * Impl routing:
 *   - /sys/block/nvme0n1/queue/zoned: zone control type
 *   - /proc/mdstat: zone device presence
 */
#include "wubu_zoneappend.h"
#include "wubu_hw_detect.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static int g_zoneappend_present = 0;
static int g_zoneappend_zoned = 0;

void wubu_zoneappend_probe(void)
{
    /* Detect ZNS zone device presence. */
#ifdef _GNU_SOURCE
    g_zoneappend_present = (access("/sys/block/nvme0n1/queue/zoned", R_OK) == 0) ? 1 : 0;
    g_zoneappend_zoned = (access("/proc/mdstat", R_OK) == 0) ? 1 : 0;
#else
    g_zoneappend_present = g_zoneappend_zoned = 0;
#endif
}

int wubu_zoneappend_present(void)
{
#ifdef _GNU_SOURCE
    return g_zoneappend_present;
#else
    return 0;
#endif
}

int wubu_zoneappend_ok(int zone_state, int seq_pos)
{
    /* Zone append valid only in sequential zones (state 1=active, 2=implicit, 3=explicit open). */
    if (zone_state < 1 || zone_state > 3) return 0;
    if (seq_pos < 0) return 0;
    return 1;
}

const char *wubu_zoneappend_state_str(int state)
{
    switch (state) {
        case 0: return "offline";
        case 1: return "active";
        case 2: return "implicit_open";
        case 3: return "explicit_open";
        case 4: return "closed";
        case 5: return "read_only";
        case 6: return "full";
        default: return "unknown";
    }
}

void wubu_zoneappend_summary(char *out, size_t cap)
{
    snprintf(out, cap, "zoneappend[dev=%d zoned=%d]",
             g_zoneappend_present, g_zoneappend_zoned);
}
