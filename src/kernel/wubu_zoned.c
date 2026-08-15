/*
 * wubu_zoned.c -- kernel-owned SMR/Zoned storage driver routing.
 *
 * Zoned storage (SMR HDDs, ZNS NVMe) writes sequentially per zone.
 * "Runs on everything" includes the modern high-capacity drives. The
 * kernel must route the zoned device to the right driver + expose zones.
 *
 * Zoned storage:
 *   - ZBC (SMR HDD): /sys/block/sdX/queue/zoned, libata ZBC
 *   - ZNS NVMe: nvme zns, /sys/block/nvmeXnY/queue/zoned
 *   - zonefs: the zone filesystem (/sys/fs/zonefs)
 *   - Zoned block device API: blk-zoned, zone_info, zone_append
 *
 * WuBuOS owns this: detect the zoned device + zone capacity, route to the
 * right driver, and expose the zone topology.
 *
 * Research (Kevin-Bacon 7-hop on the zoned frontier):
 *   - blk-zoned: the zoned block device core
 *   - ZBC SMR: shingled magnetic recording HDDs
 *   - ZNS NVMe: zoned namespace (high-density flash)
 *   - zonefs: zone filesystem for direct zone access
 */
#include "wubu_zoned.h"
#include "wubu_hw_detect.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>

/* ---- Global state ---- */
static int  g_zoned = 0;
static int  g_smr = 0;          /* SMR HDD (ZBC) */
static int  g_zns = 0;          /* ZNS NVMe */
static int  g_zonefs = 0;
static int  g_zones = 0;
static char g_zoned_drv[24] = "";

/* ---- W1: probe the zoned topology ---- */
void wubu_zoned_probe(void)
{
    g_zoned = 0; g_smr = 0; g_zns = 0; g_zonefs = 0; g_zones = 0;
    g_zoned_drv[0] = '\0';

#ifdef WUBU_HOSTED
    /* Zoned block devices: check queue/zoned on sdX/nvme */
    struct dirent **e;
    int n = scandir("/sys/block", &e, NULL, alphasort);
    for (int i = 0; i < n; i++) {
        char p[128];
        /* zoned = host-managed (1) or host-aware (2) */
        snprintf(p, sizeof(p), "/sys/block/%s/queue/zoned", e[i]->d_name);
        FILE *f = fopen(p, "r");
        if (f) {
            char m[32] = "";
            if (fgets(m, sizeof(m), f)) {
                m[strcspn(m, "\n")] = '\0';
                if (strcmp(m, "none") != 0) {
                    g_zoned = 1;
                    if (strncmp(e[i]->d_name, "sd", 2) == 0) {
                        g_smr = 1; strcpy(g_zoned_drv, "zbc");
                    } else if (strncmp(e[i]->d_name, "nvme", 4) == 0) {
                        g_zns = 1; strcpy(g_zoned_drv, "nvme-zns");
                    }
                    /* count zones */
                    snprintf(p, sizeof(p), "/sys/block/%s/queue/nr_zones", e[i]->d_name);
                    FILE *z = fopen(p, "r");
                    if (z) { if (fscanf(z, "%d", &g_zones) != 1) g_zones = 0; fclose(z); }
                }
            }
            fclose(f);
        }
    }
    /* zonefs mounted? */
    if (access("/sys/fs/zonefs", R_OK) == 0) {
        g_zonefs = 1;
    }
#endif
}

/* ---- W2: accessors ---- */
int  wubu_zoned_present(void)  { return g_zoned; }
int  wubu_zoned_smr(void)      { return g_smr; }
int  wubu_zoned_zns(void)      { return g_zns; }
int  wubu_zoned_zonefs(void)   { return g_zonefs; }
int  wubu_zoned_zones(void)    { return g_zones; }
const char *wubu_zoned_driver(void){ return g_zoned_drv[0] ? g_zoned_drv : NULL; }

/* ---- W3: zoned driver routing ---- */
const char *wubu_zoned_driver_for(const char *dev)
{
    if (!dev) return NULL;
    if (strstr(dev, "nvme"))  return "nvme-zns";
    if (strstr(dev, "sd") || strstr(dev, "sata") || strstr(dev, "zbc")) return "zbc";
    if (strstr(dev, "zonefs")) return "zonefs";
    return "blk-zoned";
}

/* ---- W4: summary ---- */
int wubu_zoned_summary(char *out, size_t cap)
{
    return snprintf(out, cap,
        "zoned[zoned=%d smr=%d zns=%d zonefs=%d zones=%d drv=%s]",
        g_zoned, g_smr, g_zns, g_zonefs, g_zones,
        wubu_zoned_driver() ? wubu_zoned_driver() : "none");
}
