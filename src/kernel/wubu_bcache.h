/*
 * wubu_bcache.h -- kernel-owned storage bcache routing.
 */
#ifndef WUBU_BCACHE_H
#define WUBU_BCACHE_H

#include <stddef.h>

void wubu_bcache_probe(void);
int  wubu_bcache_present(void);
const char *wubu_bcache_mode_for(const char *s);
int  wubu_bcache_hit_pct(int hits, int misses);
void wubu_bcache_summary(char *out, size_t cap);

#endif
