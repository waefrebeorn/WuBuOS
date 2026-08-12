/*
 * wubu_spdifstatus.h -- kernel-owned audio SPDIF status routing.
 */
#ifndef WUBU_SPDIFSTATUS_H
#define WUBU_SPDIFSTATUS_H

#include <stddef.h>

void wubu_spdifstatus_probe(void);
int  wubu_spdifstatus_present(void);
int  wubu_spdifstatus_lock(void);
int  wubu_spdifstatus_valid(void);
int  wubu_spdifstatus_aes(void);
int  wubu_spdifstatus_rate(void);
const char *wubu_spdifstatus_driver(void);
const char *wubu_spdifstatus_rate_for(const char *r);
const char *wubu_spdifstatus_lock_for(const char *l);
int wubu_spdifstatus_summary(char *out, size_t cap);

#endif
