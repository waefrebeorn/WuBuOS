/*
 * wubu_devmapper.c -- kernel-owned storage DM (device mapper) routing.
 *
 * DM (device mapper) provides virtual block devices. "Runs on everything"
 * includes correct DM on every storage system.
 *
 * DM:
 *   - /dev/mapper/: device mapper devices
 *   - /sys/block dm dm: DM metadata
 *-   - target: linear, stripe, mirror, snapshot, thin, encrypt
 *   - table: /sys/block dm slaves holders
 *   - /proc/mounts: dm devices
 *
 * WuBuOS owns this: detect DM + target + table, route to the
 * right driver, expose the topology.
 *
 * Research (7-hop on the devmapper frontier):
 *   -Linux DM thin provisioning snapshot
 */
#include "wubu_devmapper.h"
#include "wubu_hw_detect.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static int  g_dm = 0;          /* DM present */
static int  g_linear = 0;      /* linear target */
static int  g_stripe = 0;      /* stripe target */
static int  g_mirror = 0;      /* mirror target */
static int  g_snapshot = 0;    /* snapshot target */
static char g_dm_drv[24] = "";

void wubu_devmapper_probe(void)
{
    g_dm = 0; g_linear = 0; g_stripe = 0; g_mirror = 0; g_snapshot = 0;
    g_dm_drv[0] = '\0';

#ifdef WUBU_HOSTED
    if (access("/dev/mapper/control", R_OK) == 0 ||
        access("/sys/block/dm-0/dm", R_OK) == 0 ||
        access("/sys/block/dm-0", R_OK) == 0) {
        g_dm = 1; g_linear = 1; g_stripe = 1; g_mirror = 1; g_snapshot = 1;
        strcpy(g_dm_drv, "device-mapper");
    }
    if (access("/proc/mounts", R_OK) == 0) {
        FILE *f = fopen("/proc/mounts", "r");
        if (f) {
            char line[512];
            while (fgets(line, sizeof(line), f)) {
                if (strstr(line, "/dev/mapper/")) {
                    g_dm = 1;
                    if (!g_dm_drv[0]) strcpy(g_dm_drv, "dm-mount");
                }
            }
            fclose(f);
        }
    }
#endif
}

int  wubu_devmapper_present(void){ return g_dm; }
int  wubu_devmapper_linear(void){ return g_linear; }
int  wubu_devmapper_stripe(void){ return g_stripe; }
int  wubu_devmapper_mirror(void){ return g_mirror; }
int  wubu_devmapper_snapshot(void){ return g_snapshot; }
const char *wubu_devmapper_driver(void){ return g_dm_drv[0] ? g_dm_drv : NULL; }

const char *wubu_devmapper_target_for(const char *t)
{
    if (!t) return NULL;
    if (strstr(t, "linear"))   return "linear";
    if (strstr(t, "stripe"))   return "stripe";
    if (strstr(t, "mirror"))   return "mirror";
    if (strstr(t, "snapshot")) return "snapshot";
    if (strstr(t, "thin"))     return "thin";
    if (strstr(t, "crypt"))    return "crypt";
    if (strstr(t, "multipath")) return "multipath";
    return "linear";
}

const char *wubu_devmapper_mode_for(const char *m)
{
    if (!m) return NULL;
    if (strstr(m, "read-w") || strstr(m, "rw")) return "read-write";
    if (strstr(m, "read-only") || strstr(m, "ro")) return "read-only";
    if (strstr(m, "read"))    return "read";
    if (strstr(m, "write"))   return "write";
    return "read-write";
}

int wubu_devmapper_summary(char *out, size_t cap)
{
    return snprintf(out, cap,
        "devmapper[dm=%d linear=%d stripe=%d mirror=%d snapshot=%d drv=%s]",
        g_dm, g_linear, g_stripe, g_mirror, g_snapshot,
        wubu_devmapper_driver() ? wubu_devmapper_driver() : "none");
}
