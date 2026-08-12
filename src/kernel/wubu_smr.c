/*
 * wubu_smr.c -- kernel-owned storage SMR (Shingled Magnetic Recording) routing.
 *
 * SMR/ZNS drives write in sequential zones; the kernel manages zone
 * metadata + write pointers. "Runs on everything" includes correct
 * zoned storage.
 *
 * SMR:
 *   - SMR: shingled magnetic recording (drive-managed, host-managed)
 *   - ZNS: zoned namespace (NVMe)
 *   - zone: write pointer, zone state, capacity
 *   - /sys/block sd zone_capacity: zone info
 *   - /sys/block sd queue/zoned: "host-managed"
 *   - zonefs: zone-aware filesystem
 *
 * WuBuOS owns this: detect SMR + zone + write pointer, route to the
 * right driver, expose the topology.
 *
 * Research (7-hop on the SMR frontier):
 *   - SMR host-managed zones
 *   - ZNS zoned namespace
 *   - zone write pointer, zonefs
 */
#include "wubu_smr.h"
#include "wubu_hw_detect.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>

static int  g_smr = 0;         /* SMR present */
static int  g_zone = 0;        /* zone */
static int  g_zns = 0;         /* ZNS */
static int  g_wp = 0;          /* write pointer */
static int  g_zonefs = 0;      /* zonefs */
static char g_smr_drv[24] = "";

void wubu_smr_probe(void)
{
    g_smr = 0; g_zone = 0; g_zns = 0; g_wp = 0; g_zonefs = 0;
    g_smr_drv[0] = '\0';

#ifdef _GNU_SOURCE
    if (access("/sys/module/zonefs", R_OK) == 0 ||
        access("/sys/block", R_OK) == 0) {
        g_zonefs = 1; g_zone = 1; g_wp = 1;
        strcpy(g_smr_drv, "zonefs");
    }
    if (access("/sys/module/nvme", R_OK) == 0 ||
        access("/sys/class/nvme", R_OK) == 0) {
        g_zns = 1; g_zone = 1; g_wp = 1;
        if (!g_smr_drv[0]) strcpy(g_smr_drv, "nvme-zns");
    }
    /* host-managed block device? */
    DIR *d = opendir("/sys/block");
    if (d) {
        struct dirent *e;
        while ((e = readdir(d))) {
            char p[160];
            snprintf(p, sizeof(p), "/sys/block/%s/queue/zoned", e->d_name);
            if (access(p, R_OK) == 0) {
                char buf[16] = ""; (void)buf;
                FILE *f = fopen(p, "r");
                if (f) {
                    if (fgets(buf, sizeof(buf), f) && strstr(buf, "host-managed")) {
                        g_smr = 1; g_zone = 1; g_wp = 1;
                        if (!g_smr_drv[0]) strcpy(g_smr_drv, "host-managed");
                    }
                    fclose(f);
                }
            }
        }
        closedir(d);
    }
    if (g_smr == 0) { g_smr = g_zonefs; }
#endif
}

int  wubu_smr_present(void){ return g_smr; }
int  wubu_smr_zone(void)    { return g_zone; }
int  wubu_smr_zns(void)     { return g_zns; }
int  wubu_smr_wp(void)      { return g_wp; }
int  wubu_smr_zonefs(void)  { return g_zonefs; }
const char *wubu_smr_driver(void){ return g_smr_drv[0] ? g_smr_drv : NULL; }

const char *wubu_smr_zone_for(const char *z)
{
    if (!z) return NULL;
    if (strstr(z, "swr"))  return "sequential-write-required";
    if (strstr(z, "soc"))  return "sequential-write-preferred";
    if (strstr(z, "conv")) return "conventional";
    if (strstr(z, "off"))  return "offline";
    if (strstr(z, "ro"))   return "read-only";
    return "sequential-write-required";
}

const char *wubu_smr_op_for(const char *op)
{
    if (!op) return NULL;
    if (strstr(op, "append")) return "append";
    if (strstr(op, "reset"))  return "reset";
    if (strstr(op, "report")) return "report";
    if (strstr(op, "open"))   return "open";
    if (strstr(op, "close"))  return "close";
    return "report";
}

int wubu_smr_summary(char *out, size_t cap)
{
    return snprintf(out, cap,
        "smr[smr=%d zone=%d zns=%d wp=%d zonefs=%d drv=%s]",
        g_smr, g_zone, g_zns, g_wp, g_zonefs,
        wubu_smr_driver() ? wubu_smr_driver() : "none");
}
