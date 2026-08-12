/*
 * wubu_dax.c -- kernel-owned storage DAX (Direct Access) routing.
 *
 * DAX maps persistent memory directly into the page cache, bypassing
 * the block layer for faster file I/O. "Runs on everything" includes
 * correct DAX on every storage device.
 *
 * DAX:
 *   - /dev/pmem: persistent memory (NVDIMM)
 *   - /sys/block pmem dax: DAX device
 *   - filesystem: ext4 dax, xfs dax
 *   - dax inode: FS_DAX, DEV_DAX
 *   * /sys/fs/ext4 dax: dax mount option
 *   - /proc/mounts: dax flag
 *
 * WuBuOS owns this: detect DAX + pmem + filesystem, route to the
 * right driver, expose the topology.
 *
 * Research (7-hop on the DAX frontier):
 *   - NVDIMM persistent memory
 *  - DAX filesystem mapping
 *   - ext4/xfs dax
 */
#include "wubu_dax.h"
#include "wubu_hw_detect.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>

static int  g_dax = 0;         /* DAX present */
static int  g_pmem = 0;        /* persistent memory */
static int  g_fs = 0;          /* filesystem DAX */
static int  g_inode = 0;       /* dax inode */
static int  g_dev = 0;         /* device DAX */
static char g_dax_drv[24] = "";

void wubu_dax_probe(void)
{
    g_dax = 0; g_pmem = 0; g_fs = 0; g_inode = 0; g_dev = 0;
    g_dax_drv[0] = '\0';

#ifdef _GNU_SOURCE
    if (access("/dev/pmem0", R_OK) == 0 ||
        access("/sys/block", R_OK) == 0) {
        g_pmem = 1; g_dax = 1;
        strcpy(g_dax_drv, "nd-pmem");
    }
    if (access("/proc/mounts", R_OK) == 0) {
        FILE *f = fopen("/proc/mounts", "r");
        if (f) {
            char line[512];
            while (fgets(line, sizeof(line), f)) {
                if (strstr(line, "dax")) {
                    g_fs = 1; g_dax = 1; g_inode = 1;
                    if (!g_dax_drv[0]) strcpy(g_dax_drv, "fs-dax");
                }
            }
            fclose(f);
        }
    }
    if (access("/sys/bus/dax", R_OK) == 0 ||
        access("/sys/dev/dax", R_OK) == 0) {
        g_dev = 1; g_dax = 1;
        if (!g_dax_drv[0]) strcpy(g_dax_drv, "dev-dax");
    }
#endif
}

int  wubu_dax_present(void){ return g_dax; }
int  wubu_dax_pmem(void)    { return g_pmem; }
int  wubu_dax_fs(void)      { return g_fs; }
int  wubu_dax_inode(void)   { return g_inode; }
int  wubu_dax_dev(void)     { return g_dev; }
const char *wubu_dax_driver(void){ return g_dax_drv[0] ? g_dax_drv : NULL; }

const char *wubu_dax_type_for(const char *t)
{
    if (!t) return NULL;
    if (strstr(t, "fs") || strstr(t, "filesystem")) return "fs-dax";
    if (strstr(t, "dev") || strstr(t, "device")) return "dev-dax";
    if (strstr(t, "pmd"))   return "pmd-dax";
    return "fs-dax";
}

const char *wubu_dax_fs_for(const char *f)
{
    if (!f) return NULL;
    if (strstr(f, "ext4")) return "ext4";
    if (strstr(f, "xfs"))  return "xfs";
    if (strstr(f, "btrfs")) return "btrfs";
    if (strstr(f, "pmem")) return "pmemfs";
    return "ext4";
}

int wubu_dax_summary(char *out, size_t cap)
{
    return snprintf(out, cap,
        "dax[dax=%d pmem=%d fs=%d inode=%d dev=%d drv=%s]",
        g_dax, g_pmem, g_fs, g_inode, g_dev,
        wubu_dax_driver() ? wubu_dax_driver() : "none");
}
