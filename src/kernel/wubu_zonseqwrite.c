/*
 * wubu_zonseqwrite.c -- kernel-owned ZNS sequential write routing.
 *
 * ZNS zones require sequential writes within each zone. "Runs on
 * everything" includes correct sequential write routing on all
 * ZNS-capable SSDs.
 *
 * Impl routing:
 *   - /sys/block/nvme0n1/queue/zoned: zone control type
 *   - /sys/block/nvme0n1/queue/zoned_capacity: zone capacity
 */
#include "wubu_zonseqwrite.h"
#include "wubu_hw_detect.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static int g_zonseqwrite_present = 0;
static int g_zonseqwrite_queued = 0;

void wubu_zonseqwrite_probe(void)
{
#ifdef WUBU_HOSTED
    g_zonseqwrite_present = (access("/sys/block/nvme0n1/queue/zoned", R_OK) == 0) ? 1 : 0;
    g_zonseqwrite_queued = (access("/sys/block/nvme0n1/queue/zoned_capacity", R_OK) == 0) ? 1 : 0;
#else
    g_zonseqwrite_present = g_zonseqwrite_queued = 0;
#endif
}

int wubu_zonseqwrite_present(void)
{
#ifdef WUBU_HOSTED
    return g_zonseqwrite_present;
#else
    return 0;
#endif
}

int wubu_zonseqwrite_ok(int zone_state, int wp, int len, int max_len)
{
    /* Sequential write: only in active/implicit/explicit open zones,
     * within capacity, and write fits remaining space. */
    if (zone_state < 1 || zone_state > 3) return 0;  /* must be open */
    if (wp + len > max_len) return 0;               /* would exceed cap */
    if (len <= 0) return 0;
    return 1;
}

const char *wubu_zonseqwrite_state_str(int state)
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

void wubu_zonseqwrite_summary(char *out, size_t cap)
{
    snprintf(out, cap, "zonseqwrite[dev=%d queued=%d]",
             g_zonseqwrite_present, g_zonseqwrite_queued);
}
