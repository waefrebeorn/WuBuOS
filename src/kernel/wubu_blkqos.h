/*
 * wubu_blkqos.h -- kernel-owned storage blk-QoS routing.
 */
#ifndef WUBU_BLKQOS_H
#define WUBU_BLKQOS_H

#include <stddef.h>

void wubu_blkqos_probe(void);
int  wubu_blkqos_present(void);
int  wubu_blkqos_throttle(void);
int  wubu_blkqos_weight(void);
int  wubu_blkqos_cg(void);
int  wubu_blkqos_limit(void);
const char *wubu_blkqos_driver(void);
const char *wubu_blkqos_mode_for(const char *m);
const char *wubu_blkqos_unit_for(const char *u);
int wubu_blkqos_summary(char *out, size_t cap);

#endif
