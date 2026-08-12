/*
 * wubu_raidcache.h -- kernel-owned storage RAID cache routing.
 */
#ifndef WUBU_RAIDCACHE_H
#define WUBU_RAIDCACHE_H

#include <stddef.h>

/* W1: probe the cache topology. */
void wubu_raidcache_probe(void);

/* W2: accessors */
int  wubu_raidcache_present(void);
int  wubu_raidcache_dm_cache(void);
int  wubu_raidcache_bcache(void);
int  wubu_raidcache_zram(void);
int  wubu_raidcache_raid_journal(void);
const char *wubu_raidcache_driver(void);

/* W3: cache driver routing. */
const char *wubu_raidcache_driver_for(const char *cache);

/* W4: summary fragment. */
int wubu_raidcache_summary(char *out, size_t cap);

#endif /* WUBU_RAIDCACHE_H */
