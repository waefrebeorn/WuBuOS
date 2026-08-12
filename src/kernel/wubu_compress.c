/*
 * wubu_compress.c -- kernel-owned storage transparent compression routing.
 *
 * Transparent compression compresses data at write time, decompresses
 * at read time. "Runs on everything" includes correct compression.
 *
 * Storage compression:
 *   - btrfs: transparent compression (zlib, zstd, lzo)
 *   - ZFS: transparent (lz4, gzip, zle, zstd)
 *   - dm-integrity: no-compress; dm-compress (dm-zcrypt)
 *   - /sys/fs/btrfs dev compression: btrfs compression mode
 *   - algorithms: zlib, zstd, lzo, lz4
 *   - levels: 1-9 for zlib/zstd
 *
 * WuBuOS owns this: detect transparent compression (fs + dm) + algorithm,
 * route to the right driver, and expose the topology.
 *
 * Research (7-hop on the compression frontier):
 *   - btrfs transparent compression (zlib, zstd, lzo)
 *   - ZFS lz4/gzip/zstd
 *   - dm-integrity, dm-zcrypt
 */
#include "wubu_compress.h"
#include "wubu_hw_detect.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>

static int  g_compress = 0;    /* compression present */
static int  g_btrfs = 0;       /* btrfs */
static int  g_zfs = 0;         /* ZFS */
static int  g_zstd = 0;        /* zstd algo */
static int  g_lz4 = 0;         /* lz4 algo */
static char g_compress_drv[24] = "";

void wubu_compress_probe(void)
{
    g_compress = 0; g_btrfs = 0; g_zfs = 0; g_zstd = 0; g_lz4 = 0;
    g_compress_drv[0] = '\0';

#ifdef _GNU_SOURCE
    if (access("/sys/fs/btrfs", R_OK) == 0 ||
        access("/sys/module/btrfs", R_OK) == 0) {
        g_compress = 1; g_btrfs = 1; g_zstd = 1; g_lz4 = 1;
        strcpy(g_compress_drv, "btrfs-compress");
    }
    if (access("/sys/module/zfs", R_OK) == 0 ||
        access("/proc/spl/kstat/zfs", R_OK) == 0) {
        g_compress = 1; g_zfs = 1; g_lz4 = 1;
        if (!g_compress_drv[0]) strcpy(g_compress_drv, "zfs-compress");
    }
#endif
}

int  wubu_compress_present(void){ return g_compress; }
int  wubu_compress_btrfs(void)  { return g_btrfs; }
int  wubu_compress_zfs(void)    { return g_zfs; }
int  wubu_compress_zstd(void)   { return g_zstd; }
int  wubu_compress_lz4(void)    { return g_lz4; }
const char *wubu_compress_driver(void){ return g_compress_drv[0] ? g_compress_drv : NULL; }

const char *wubu_compress_algo_for(const char *algo)
{
    if (!algo) return NULL;
    if (strstr(algo, "zstd")) return "zstd";
    if (strstr(algo, "lz4"))  return "lz4";
    if (strstr(algo, "lzo"))  return "lzo";
    if (strstr(algo, "zlib")) return "zlib";
    if (strstr(algo, "gzip")) return "gzip";
    return "none";
}

const char *wubu_compress_mode_for(const char *mode)
{
    if (!mode) return NULL;
    if (strstr(mode, "transparent")) return "transparent";
    if (strstr(mode, "force"))      return "force";
    if (strstr(mode, "zlib"))      return "zlib";
    return "auto";
}

int wubu_compress_summary(char *out, size_t cap)
{
    return snprintf(out, cap,
        "compress[compress=%d btrfs=%d zfs=%d zstd=%d lz4=%d drv=%s]",
        g_compress, g_btrfs, g_zfs, g_zstd, g_lz4,
        wubu_compress_driver() ? wubu_compress_driver() : "none");
}