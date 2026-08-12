/*
 * wubu_gpurst.h -- kernel-owned GPU reset + recovery routing.
 */
#ifndef WUBU_GPURST_H
#define WUBU_GPURST_H

#include <stddef.h>

void wubu_gpurst_probe(void);
int  wubu_gpurst_present(void);
int  wubu_gpurst_ring(void);
int  wubu_gpurst_hb(void);
int  wubu_gpurst_timeout(void);
int  wubu_gpurst_recover(void);
const char *wubu_gpurst_driver(void);
const char *wubu_gpurst_stage_for(const char *s);
const char *wubu_gpurst_ring_for(const char *r);
int wubu_gpurst_summary(char *out, size_t cap);

#endif
