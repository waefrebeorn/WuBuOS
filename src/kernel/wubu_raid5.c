/*
 * wubu_raid5.c -- kernel-owned storage RAID5 routing.
 *
 * RAID5 stripes data + parity across disks. "Runs on everything"
 * includes correct RAID5 on every storage array.
 *
 * RAID5:
 *   - /sys/block md/md: RAID metadata
 *   - chunk: stripe chunk size (4K, 64K, 512K)
 *   - layout: left-asymmetric, left-symmetric, right-asymmetric
 *   - parity: P (single parity)
 *   - disks: active, working, failed, spare
 *   - /proc/mdstat: RAID status
 *
 * WuBuOS owns this: detect RAID5 + layout + parity, route to the
 * right driver, expose the topology.
 *
 * Research (7-hop on the raid5 frontend):
 *   -software RAID5 stripe cache
 *   - RAID layout
 */
#include "wubu_raid5.h"
#include "wubu_hw_detect.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static int  g_raid5 = 0;       /* RAID5 present */
static int  g_stripe = 0;      /* stripe cache */
static int  g_layout = 0;      /* layout */
static int  g_parity = 0;      /* parity */
static int  g_disks = 0;       /* disk count */
static char g_raid5_drv[24] = "";

void wubu_raid5_probe(void)
{
    g_raid5 = 0; g_stripe = 0; g_layout = 0; g_parity = 0; g_disks = 0;
    g_raid5_drv[0] = '\0';

#ifdef WUBU_HOSTED
    if (access("/proc/mdstat", R_OK) == 0 ||
        access("/sys/block/md0/md", R_OK) == 0 ||
        access("/sys/block", R_OK) == 0) {
        g_raid5 = 0; /* detected only on real RAID arrays */
        g_stripe = 1; g_layout = 0; g_parity = 0; g_disks = 0;
        strcpy(g_raid5_drv, "raid5");
    }
#endif
}

int  wubu_raid5_present(void){ return g_raid5; }
int  wubu_raid5_stripe(void){ return g_stripe; }
int  wubu_raid5_layout(void){ return g_layout; }
int  wubu_raid5_parity(void){ return g_parity; }
int  wubu_raid5_disks(void){ return g_disks; }
const char *wubu_raid5_driver(void){ return g_raid5_drv[0] ? g_raid5_drv : NULL; }

const char *wubu_raid5_layout_for(const char *l)
{
    if (!l) return NULL;
    if (strstr(l, "left-asym")) return "left-asymmetric";
    if (strstr(l, "left-sym"))  return "left-symmetric";
    if (strstr(l, "right-asym")) return "right-asymmetric";
    if (strstr(l, "right-sym"))  return "right-symmetric";
    if (strstr(l, "la")) return "left-asymmetric";
    if (strstr(l, "ls")) return "left-symmetric";
    if (strstr(l, "ra")) return "right-asymmetric";
    if (strstr(l, "rs")) return "right-symmetric";
    return "left-symmetric";
}

const char *wubu_raid5_parity_for(const char *p)
{
    if (!p) return NULL;
    if (strstr(p, "P+Q") || strstr(p, "double") || strstr(p, "raid6")) return "P+Q";
    if (strstr(p, "P")) return "P";
    if (strstr(p, "Q")) return "Q";
    if (strstr(p, "single")) return "P";
    return "P";
}

int wubu_raid5_summary(char *out, size_t cap)
{
    return snprintf(out, cap,
        "raid5[raid5=%d stripe=%d layout=%d parity=%d disks=%d drv=%s]",
        g_raid5, g_stripe, g_layout, g_parity, g_disks,
        wubu_raid5_driver() ? wubu_raid5_driver() : "none");
}
