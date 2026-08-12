/*
 * wubu_dedup.h -- kernel-owned storage deduplication routing.
 */
#ifndef WUBU_DEDUP_H
#define WUBU_DEDUP_H

#include <stddef.h>

/* W1: probe the dedup topology. */
void wubu_dedup_probe(void);

/* W2: accessors */
int  wubu_dedup_present(void);
int  wubu_dedup_dm(void);
int  wubu_dedup_btrfs(void);
int  wubu_dedup_xfs(void);
int  wubu_dedup_zfs(void);
const char *wubu_dedup_driver(void);

/* W3: dedup routing. */
const char *wubu_dedup_mode_for(const char *mode);
const char *wubu_dedup_level_for(const char *level);

/* W4: summary fragment. */
int wubu_dedup_summary(char *out, size_t cap);

#endif /* WUBU_DEDUP_H */
