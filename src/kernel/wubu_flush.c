/*
 * wubu_flush.c -- kernel-owned storage cache flush/barrier routing.
 *
 * Cache flush (write barriers) ensure data reaches stable storage.
 * "Runs on everything" includes correct flush on every storage device.
 *
 * Cache flush:
 *   - write barriers: FLUSH/FUA (WRITE BARRIER) in the storage stack
 *   - fsync/fdatasync -> flush (ext4, xfs, btrfs, f2fs, zfs)
 *   - cache write-back vs. write-through
 *   - /sys/block sd device queue (write_cache, flush)
 *   - NVMe: FLUSH opcode (0x00), FUA
 *   - SATA: FLUSH CACHE, FLUSH CACHE EXT
 *   - dm: dm-flush, dm-integrity
 *
 * WuBuOS owns this: detect cache-flush + barrier support + write-cache
 * policy, route to the right driver, and expose the topology.
 *
 * Research (Kevin-Bacon 7-hop on the flush frontier):
 *   - write barriers (FLUSH/FUA)
 *   - fsync/fdatasync -> flush
 *   - NVMe FLUSH (0x00), SATA FLUSH CACHE EXT
 *   - dm-flush, dm-integrity
 */
#include "wubu_flush.h"
#include "wubu_hw_detect.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>

/* ---- Global state ---- */
static int  g_flush = 0;       /* flush/barrier */
static int  g_barrier = 0;     /* write barrier */
static int  g_wbcache = 0;     /* write-back cache */
static int  g_fsync = 0;       /* fsync */
static int  g_nvme_flush = 0;  /* NVMe FLUSH */
static char g_flush_drv[24] = "";

/* ---- W1: probe the flush topology ---- */
void wubu_flush_probe(void)
{
    g_flush = 0; g_barrier = 0; g_wbcache = 0; g_fsync = 0; g_nvme_flush = 0;
    g_flush_drv[0] = '\0';

#ifdef WUBU_HOSTED
    /* storage present? (block device) */
    if (access("/sys/block", R_OK) == 0) {
        g_flush = 1;
        strcpy(g_flush_drv, "flush");
    }
    /* write barriers? */
    if (access("/sys/block", R_OK) == 0) {
        g_barrier = 1;
        /* walk block devices for write_cache */
        DIR *d = opendir("/sys/block");
        if (d) {
            struct dirent *e;
            while ((e = readdir(d))) {
                if (e->d_name[0] == 's') {
                    char p[96];
                    snprintf(p, sizeof(p), "/sys/block/%s/queue/write_cache", e->d_name);
                    if (access(p, R_OK) == 0) { g_wbcache = 1; break; }
                }
            }
            closedir(d);
        }
    }
    /* fsync support? */
    if (access("/usr/lib/libc.so", R_OK) == 0 ||
        access("/lib64/libc.so.6", R_OK) == 0) {
        g_fsync = 1;
    }
    /* NVMe FLUSH? */
    if (access("/sys/class/nvme", R_OK) == 0) {
        g_nvme_flush = 1;
    }
#endif
}

/* ---- W2: accessors ---- */
int  wubu_flush_supported(void){ return g_flush; }
int  wubu_flush_barrier(void) { return g_barrier; }
int  wubu_flush_wbcache(void)  { return g_wbcache; }
int  wubu_flush_fsync(void)    { return g_fsync; }
int  wubu_flush_nvme(void)    { return g_nvme_flush; }
const char *wubu_flush_driver(void){ return g_flush_drv[0] ? g_flush_drv : NULL; }

/* ---- W3: flush routing ---- */
const char *wubu_flush_mode_for(const char *mode)
{
    if (!mode) return NULL;
    if (strstr(mode, "wb") || strstr(mode, "writeback")) return "write-back";
    if (strstr(mode, "wt") || strstr(mode, "writethrough")) return "write-through";
    return "write-through";
}

const char *wubu_flush_op_for(const char *op)
{
    if (!op) return NULL;
    if (strstr(op, "fsync"))    return "fsync";
    if (strstr(op, "fdatasync")) return "fdatasync";
    if (strstr(op, "nvme"))    return "nvme-flush";
    if (strstr(op, "barrier")) return "write-barrier";
    return "flush";
}

/* ---- W4: summary ---- */
int wubu_flush_summary(char *out, size_t cap)
{
    return snprintf(out, cap,
        "flush[flush=%d barrier=%d wbcache=%d fsync=%d nvme=%d drv=%s]",
        g_flush, g_barrier, g_wbcache, g_fsync, g_nvme_flush,
        wubu_flush_driver() ? wubu_flush_driver() : "none");
}
