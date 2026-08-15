/*
 * wubu_dedup.c -- kernel-owned storage deduplication routing.
 *
 * Deduplication removes duplicate blocks/data to save storage. "Runs on
 * everything" includes correct dedup on every storage stack.
 *
 * Dedup:
 *   - dm-dirty? no. dm-dedup: device-mapper deduplication target
 *   - btrfs: built-in dedup (btrfs filesystem, send/receive dedup)
 *   - xfs: reflink (copy-on-write) via xfs_copy + reflink
 *   - ZFS: built-in dedup (L2ARC + ARC)
 *   - OpenZFS: zfs set dedup=on
 *   - /sys/fs/btrfs dev duid: btrfs dedup stats
 *   - userspace: duperemove, bees (btrfs/xfs dedup)
 *
 * WuBuOS owns this: detect dedup support (fs + dm + userspace), route to
 * the right driver, and expose the topology.
 *
 * Research (7-hop on the dedup frontier):
 *   - dm-dedup device-mapper target
 *   - btrfs built-in dedup + send/receive
 *   - xfs reflink (copy-on-write)
 *   - ZFS / OpenZFS dedup
 *   - userspace: duperemove, bees
 */
#include "wubu_dedup.h"
#include "wubu_hw_detect.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

/* ---- Global state ---- */
static int  g_dedup = 0;       /* dedup present */
static int  g_dm = 0;          /* dm-dedup */
static int  g_btrfs = 0;       /* btrfs dedup */
static int  g_xfs = 0;         /* xfs reflink */
static int  g_zfs = 0;         /* ZFS dedup */
static char g_dedup_drv[24] = "";

/* ---- W1: probe the dedup topology ---- */
void wubu_dedup_probe(void)
{
    g_dedup = 0; g_dm = 0; g_btrfs = 0; g_xfs = 0; g_zfs = 0;
    g_dedup_drv[0] = '\0';

#ifdef WUBU_HOSTED
    /* dm-dedup? */
    if (access("/sys/module/dm_mod", R_OK) == 0) {
        g_dm = 1;
    }
    /* btrfs? */
    if (access("/sys/fs/btrfs", R_OK) == 0 ||
        access("/sys/module/btrfs", R_OK) == 0) {
        g_dedup = 1; g_btrfs = 1;
        strcpy(g_dedup_drv, "btrfs-dedup");
    }
    /* xfs reflink? */
    if (access("/sys/fs/xfs", R_OK) == 0 ||
        access("/sys/module/xfs", R_OK) == 0) {
        g_dedup = 1; g_xfs = 1;
        if (!g_dedup_drv[0]) strcpy(g_dedup_drv, "xfs-reflink");
    }
    /* ZFS? */
    if (access("/sys/module/zfs", R_OK) == 0 ||
        access("/proc/spl/kstat/zfs", R_OK) == 0) {
        g_dedup = 1; g_zfs = 1;
        if (!g_dedup_drv[0]) strcpy(g_dedup_drv, "zfs-dedup");
    }
    /* dm-dedup without fs? */
    if (g_dm && !g_dedup) {
        g_dedup = 1;
        strcpy(g_dedup_drv, "dm-dedup");
    }
#endif
}

/* ---- W2: accessors ---- */
int  wubu_dedup_present(void){ return g_dedup; }
int  wubu_dedup_dm(void)     { return g_dm; }
int  wubu_dedup_btrfs(void)  { return g_btrfs; }
int  wubu_dedup_xfs(void)    { return g_xfs; }
int  wubu_dedup_zfs(void)    { return g_zfs; }
const char *wubu_dedup_driver(void){ return g_dedup_drv[0] ? g_dedup_drv : NULL; }

/* ---- W3: dedup routing ---- */
const char *wubu_dedup_mode_for(const char *mode)
{
    if (!mode) return NULL;
    if (strstr(mode, "inode"))   return "inode-dedup";
    if (strstr(mode, "block"))   return "block-dedup";
    if (strstr(mode, "file"))    return "file-dedup";
    if (strstr(mode, "offline")) return "offline";
    if (strstr(mode, "online"))  return "online";
    return "dedup";
}

const char *wubu_dedup_level_for(const char *level)
{
    if (!level) return NULL;
    if (strstr(level, "aggressive")) return "aggressive";
    if (strstr(level, "conservative")) return "conservative";
    if (strstr(level, "none")) return "none";
    return "conservative";
}

/* ---- W4: summary ---- */
int wubu_dedup_summary(char *out, size_t cap)
{
    return snprintf(out, cap,
        "dedup[dedup=%d dm=%d btrfs=%d xfs=%d zfs=%d drv=%s]",
        g_dedup, g_dm, g_btrfs, g_xfs, g_zfs,
        wubu_dedup_driver() ? wubu_dedup_driver() : "none");
}
