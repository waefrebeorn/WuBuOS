/*
 * wubu_devmapper.h -- kernel-owned storage DM routing.
 */
#ifndef WUBU_DEVMAPPER_H
#define WUBU_DEVMAPPER_H

#include <stddef.h>

void wubu_devmapper_probe(void);
int  wubu_devmapper_present(void);
int  wubu_devmapper_linear(void);
int  wubu_devmapper_stripe(void);
int  wubu_devmapper_mirror(void);
int  wubu_devmapper_snapshot(void);
const char *wubu_devmapper_driver(void);
const char *wubu_devmapper_target_for(const char *t);
const char *wubu_devmapper_mode_for(const char *m);
int wubu_devmapper_summary(char *out, size_t cap);

#endif
