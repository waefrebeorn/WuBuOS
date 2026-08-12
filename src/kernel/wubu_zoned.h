/*
 * wubu_zoned.h -- kernel-owned SMR/Zoned storage driver routing.
 */
#ifndef WUBU_ZONED_H
#define WUBU_ZONED_H

#include <stddef.h>

/* W1: probe the zoned topology. */
void wubu_zoned_probe(void);

/* W2: accessors */
int  wubu_zoned_present(void);
int  wubu_zoned_smr(void);
int  wubu_zoned_zns(void);
int  wubu_zoned_zonefs(void);
int  wubu_zoned_zones(void);
const char *wubu_zoned_driver(void);

/* W3: zoned driver routing. */
const char *wubu_zoned_driver_for(const char *dev);

/* W4: summary fragment. */
int wubu_zoned_summary(char *out, size_t cap);

#endif /* WUBU_ZONED_H */
