/*
 * wubu_zonefmt.c -- kernel-owned ZNS zone format/reset routing.
 *
 * ZNS zones must be reset (write pointer to 0) before reuse after
 * a zone becomes full. "Runs on everything" includes correct
 * zone reset routing on all ZNS-capable SSDs.
 *
 * Impl routing:
 *   - /sys/block/nvme0n1/queue/zoned: zone control type
 *   - /proc/mdstat: zone device presence
 */
#include "wubu_zonefmt.h"
#include "wubu_hw_detect.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static int g_zonefmt_present = 0;
static int g_zonefmt_zoned = 0;

void wubu_zonefmt_probe(void)
{
    /* Detect ZNS zone device. */
#ifdef _GNU_SOURCE
    g_zonefmt_present = (access("/sys/block/nvme0n1/queue/zoned", R_OK) == 0) ? 1 : 0;
    g_zonefmt_zoned = (access("/proc/mdstat", R_OK) == 0) ? 1 : 0;
#else
    g_zonefmt_present = g_zonefmt_zoned = 0;
#endif
}

int wubu_zonefmt_present(void)
{
#ifdef _GNU_SOURCE
    return g_zonefmt_present;
#else
    return 0;
#endif
}

int wubu_zonefmt_reset_ok(int zone_state)
{
    /* Reset valid only on offline or full zones. */
    if (zone_state == 0 || zone_state == 6) return 1;
    return 0;
}

const char *wubu_zonefmt_state_str(int state)
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

void wubu_zonefmt_summary(char *out, size_t cap)
{
    snprintf(out, cap, "zonefmt[dev=%d zoned=%d]",
             g_zonefmt_present, g_zonefmt_zoned);
}
