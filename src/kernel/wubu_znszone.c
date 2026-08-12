/*
 * wubu_znszone.c -- kernel-owned ZNS (Zoned Namespaces) routing.
 *
 * ZNS exposes SSDs as zones (sequential write, random read) for
 * application-managed placement. "Runs on everything" includes
 * correct ZNS zone state on every NVMe/ZNS-capable drive.
 *
 * Impl routing:
 *   - /sys/block/nvme0n1/queue/zoned: zone control type
 *   - /proc/mdstat: zone device presence
 */
#include "wubu_znszone.h"
#include "wubu_hw_detect.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static int g_znszone_present = 0;
static int g_znszone_zoned = 0;

void wubu_znszone_probe(void)
{
    /* Detect ZNS zone device presence. */
#ifdef _GNU_SOURCE
    g_znszone_present = (access("/sys/block/nvme0n1/queue/zoned", R_OK) == 0) ? 1 : 0;
    g_znszone_zoned = (access("/proc/mdstat", R_OK) == 0) ? 1 : 0;
#else
    g_znszone_present = g_znszone_zoned = 0;
#endif
}

int wubu_znszone_present(void)
{
#ifdef _GNU_SOURCE
    return g_znszone_present;
#else
    return 0;
#endif
}

int wubu_znszone_active(int total, int used)
{
    if (total <= 0 || used < 0) return 0;
    return total - used;
}

const char *wubu_znszone_state_str(int state)
{
    switch (state) {
        case 0: return "offline";
        case 1: return "implicit_open";
        case 2: return "explicit_open";
        case 3: return "closed";
        case 4: return "read_only";
        case 5: return "full";
        default: return "unknown";
    }
}

void wubu_znszone_summary(char *out, size_t cap)
{
    snprintf(out, cap, "znszone[dev=%d zoned=%d]",
             g_znszone_present, g_znszone_zoned);
}
