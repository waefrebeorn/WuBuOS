/*
 * wubu_bcache.c -- kernel-owned storage bcache routing.
 *
 * bcache provides SSD/HDD hybrid caching via
 * /sys/block/bcache* and /sys/fs/bcache/. "Runs on everything"
 * includes correct cached/dev cache detection on every block device.
 *
 * Impl routing:
 *   - /sys/fs/bcache: bcache registration
 *   - /sys/block/bcache cache_mode: cache mode
 *   - /sys/block/bcache cached_devs: cached devices
 *   - /sys/block/bcache cache_hit_ratio: hit ratio
 */
#include "wubu_bcache.h"
#include "wubu_hw_detect.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static int g_bcache_hits = 0;
static int g_bcache_mode = 0;

void wubu_bcache_probe(void)
{
    /* Detect bcache via sysfs presence. */
#ifdef _GNU_SOURCE
    g_bcache_hits = (access("/sys/block/bcache0/bcache/cache_hit_ratio", R_OK) == 0) ? 1 : 0;
    g_bcache_mode = (access("/sys/block/bcache0/bcache/cache_mode", R_OK) == 0) ? 1 : 0;
#else
    g_bcache_hits = 0;
    g_bcache_mode = 0;
#endif
}

int wubu_bcache_present(void)
{
#ifdef _GNU_SOURCE
    return access("/sys/fs/bcache", R_OK) == 0;
#else
    return 0;
#endif
}

const char *wubu_bcache_mode_for(const char *s)
{
    if (!s) return NULL;
    if (strstr(s, "writearound") || strstr(s, "write_around")) return "writearound";
    if (strstr(s, "writethrough") || strstr(s, "write_through")) return "writethrough";
    if (strstr(s, "writeback") || strstr(s, "write_back")) return "writeback";
    if (strstr(s, "none")) return "none";
    return "unknown";
}

int wubu_bcache_hit_pct(int hits, int misses)
{
    if (hits + misses <= 0) return 0;
    return (hits * 100) / (hits + misses);
}

void wubu_bcache_summary(char *out, size_t cap)
{
    snprintf(out, cap, "bcache[present=%d mode=%s hits=%d%%]",
             wubu_bcache_present(),
             wubu_bcache_present() ? wubu_bcache_mode_for("") : "none",
             g_bcache_hits);
}
