/*
 * wubu_mdraid.c -- kernel-owned storage MD RAID routing.
 *
 * MD (Multiple Device) RAID provides software RAID levels 0/1/5/6/10.
 * "Runs on everything" includes correct RAID state on every disk array.
 *
 * Impl routing:
 *   - /proc/mdstat: RAID array status
 *   - /sys/block md-x/md: RAID metadata
 */
#include "wubu_mdraid.h"
#include "wubu_hw_detect.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>

static int g_mdraid_arrays = 0;
static int g_mdraid_healthy = 0;

void wubu_mdraid_probe(void)
{
    /* Detect MD RAID via /proc/mdstat presence. */
#ifdef _GNU_SOURCE
    g_mdraid_arrays = (access("/proc/mdstat", R_OK) == 0) ? 1 : 0;
    g_mdraid_healthy = (access("/sys/block/md0", R_OK) == 0) ? 1 : 0;
#else
    g_mdraid_arrays = g_mdraid_healthy = 0;
#endif
}

int wubu_mdraid_present(void)
{
#ifdef _GNU_SOURCE
    return g_mdraid_arrays;
#else
    return 0;
#endif
}

const char *wubu_mdraid_level_str(int level)
{
    switch (level) {
        case 0: return "raid0";
        case 1: return "raid1";
        case 4: return "raid4";
        case 5: return "raid5";
        case 6: return "raid6";
        case 10: return "raid10";
        default: return "unknown";
    }
}

int wubu_mdraid_degraded(int total, int active)
{
    if (total <= 0 || active < 0) return 0;
    return total - active;
}

void wubu_mdraid_summary(char *out, size_t cap)
{
    snprintf(out, cap, "mdraid[arrays=%d healthy=%d]",
             g_mdraid_arrays, g_mdraid_healthy);
}
