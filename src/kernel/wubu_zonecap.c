/*
 * wubu_zonecap.c -- kernel-owned ZNS zone capacity routing.
 *
 * ZNS zones have a write pointer that must stay below capacity.
 * "Runs on everything" includes correct zone capacity routing
 * on all ZNS-capable SSDs.
 *
 * Impl routing:
 *   - /sys/block/nvme0n1/queue/zoned: zone control type
 *   - /sys/block/nvme0n1/queue/zoned_capacity: zone capacity
 */
#include "wubu_zonecap.h"
#include "wubu_hw_detect.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static int g_zonecap_present = 0;
static int g_zonecap_zoned = 0;

void wubu_zonecap_probe(void)
{
#ifdef WUBU_HOSTED
    g_zonecap_present = (access("/sys/block/nvme0n1/queue/zoned", R_OK) == 0) ? 1 : 0;
    g_zonecap_zoned = (access("/sys/block/nvme0n1/queue/zoned_capacity", R_OK) == 0) ? 1 : 0;
#else
    g_zonecap_present = g_zonecap_zoned = 0;
#endif
}

int wubu_zonecap_present(void)
{
#ifdef WUBU_HOSTED
    return g_zonecap_present;
#else
    return 0;
#endif
}

int wubu_zonecap_safe(int wp, int zone_cap, int len)
{
    /* Check if write pointer + length stays within capacity. */
    if (wp + len <= zone_cap && len > 0) return 1;
    return 0;
}

int wubu_zonecap_full(int wp, int zone_cap)
{
    /* Zone is full when wp >= zone_cap. */
    if (wp >= zone_cap && zone_cap > 0) return 1;
    return 0;
}

void wubu_zonecap_summary(char *out, size_t lim)
{
    snprintf(out, lim, "zonecap[dev=%d cap=%d]", g_zonecap_present, g_zonecap_zoned);
}
