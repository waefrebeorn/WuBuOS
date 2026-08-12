/*
 * wubu_smart.h -- kernel-owned storage SMART routing.
 */
#ifndef WUBU_SMART_H
#define WUBU_SMART_H

#include <stddef.h>

void wubu_smart_probe(void);
int  wubu_smart_present(void);
int  wubu_smart_ata(void);
int  wubu_smart_nvme(void);
int  wubu_smart_health(void);
int  wubu_smart_temp(void);
const char *wubu_smart_driver(void);
const char *wubu_smart_attr_for(const char *a);
const char *wubu_smart_status_for(const char *s);
int wubu_smart_summary(char *out, size_t cap);

#endif
