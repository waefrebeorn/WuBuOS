/*
 * wubu_raidcache.c -- kernel-owned storage RAID cache (dm-cache) routing.
 *
 * SSD caching of spinning disks (or fast tier of slower) speeds up
 * storage. "Runs on everything" includes smart cache tiers.
 *
 * RAID cache:
 *   - dm-cache: device-mapper cache (SSD + HDD, writeback/writethrough)
 *   - bcache: the other SSD cache (writeback/writethrough/writearound)
 *   - md raid cache: RAID4/5/6 journal + cache (raid5-cache)
 *   - zram: compressed RAM swap (fast tmpfs-backed)
 *   - lvm-cache: LVM cache volumes
 *
 * WuBuOS owns this: detect the cache tier (dm-cache/bcache/zram), route to
 * the right driver, and expose the cache topology.
 *
 * Research (Kevin-Bacon 7-hop on the RAID-cache frontier):
 *   - dm-cache: device-mapper, /dev/mapper cache
 *   - bcache: /sys/fs/bcache, writeback caching
 *   - raid5-cache: md RAID journal
 *   - zram: compressed RAM swap
 */
#include "wubu_raidcache.h"
#include "wubu_hw_detect.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

/* ---- Global state ---- */
static int  g_cache = 0;
static int  g_dm_cache = 0;
static int  g_bcache = 0;
static int  g_zram = 0;
static int  g_raid_journal = 0;
static char g_cache_drv[24] = "";

/* ---- W1: probe the cache topology ---- */
void wubu_raidcache_probe(void)
{
    g_cache = 0; g_dm_cache = 0; g_bcache = 0; g_zram = 0; g_raid_journal = 0;
    g_cache_drv[0] = '\0';

#ifdef WUBU_HOSTED
    /* dm-cache present? */
    if (access("/sys/module/dm_cache", R_OK) == 0 ||
        access("/sys/module/dm_cache_writeback", R_OK) == 0) {
        g_dm_cache = 1; g_cache = 1;
        strcpy(g_cache_drv, "dm-cache");
    }
    /* bcache present? */
    if (access("/sys/fs/bcache", R_OK) == 0 ||
        access("/sys/module/bcache", R_OK) == 0) {
        g_bcache = 1; g_cache = 1;
        if (!g_cache_drv[0]) strcpy(g_cache_drv, "bcache");
    }
    /* zram present? */
    if (access("/sys/block/zram0", R_OK) == 0 ||
        access("/sys/module/zram", R_OK) == 0) {
        g_zram = 1;
        if (!g_cache_drv[0]) strcpy(g_cache_drv, "zram");
    }
    /* md RAID journal (raid5-cache)? */
    if (access("/sys/module/raid5", R_OK) == 0 ||
        access("/proc/mdstat", R_OK) == 0) {
        g_raid_journal = 1;
    }
#endif
}

/* ---- W2: accessors ---- */
int  wubu_raidcache_present(void){ return g_cache; }
int  wubu_raidcache_dm_cache(void){ return g_dm_cache; }
int  wubu_raidcache_bcache(void) { return g_bcache; }
int  wubu_raidcache_zram(void)   { return g_zram; }
int  wubu_raidcache_raid_journal(void){ return g_raid_journal; }
const char *wubu_raidcache_driver(void){ return g_cache_drv[0] ? g_cache_drv : NULL; }

/* ---- W3: cache driver routing ---- */
const char *wubu_raidcache_driver_for(const char *cache)
{
    if (!cache) return NULL;
    if (strstr(cache, "dm-cache")) return "dm-cache";
    if (strstr(cache, "bcache"))   return "bcache";
    if (strstr(cache, "zram"))     return "zram";
    if (strstr(cache, "raid5") || strstr(cache, "journal")) return "raid5-cache";
    if (strstr(cache, "lvm"))      return "lvm-cache";
    return "cache-core";
}

/* ---- W4: summary ---- */
int wubu_raidcache_summary(char *out, size_t cap)
{
    return snprintf(out, cap,
        "raidcache[cache=%d dm-cache=%d bcache=%d zram=%d raid-jnl=%d drv=%s]",
        g_cache, g_dm_cache, g_bcache, g_zram, g_raid_journal,
        wubu_raidcache_driver() ? wubu_raidcache_driver() : "none");
}
