/*
 * wubu_storagesched.h -- kernel-owned storage scheduler routing.
 */
#ifndef WUBU_STORAGESCHED_H
#define WUBU_STORAGESCHED_H

#include <stddef.h>

void wubu_storagesched_probe(void);
int  wubu_storagesched_present(void);
int  wubu_storagesched_mq(void);
int  wubu_storagesched_bfq(void);
int  wubu_storagesched_deadline(void);
int  wubu_storagesched_none(void);
const char *wubu_storagesched_driver(void);
const char *wubu_storagesched_type_for(const char *t);
const char *wubu_storagesched_mode_for(const char *m);
int wubu_storagesched_summary(char *out, size_t cap);

#endif
