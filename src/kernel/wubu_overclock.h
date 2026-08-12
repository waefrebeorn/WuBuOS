/*
 * wubu_overclock.h -- kernel-owned GPU overclocking routing.
 */
#ifndef WUBU_OVERCLOCK_H
#define WUBU_OVERCLOCK_H

#include <stddef.h>

void wubu_overclock_probe(void);
int  wubu_overclock_present(void);
int  wubu_overclock_od(void);
int  wubu_overclock_sysfs(void);
int  wubu_overclock_core(void);
int  wubu_overclock_mem(void);
const char *wubu_overclock_driver(void);
const char *wubu_overclock_clk_for(const char *c);
const char *wubu_overclock_state_for(const char *s);
int wubu_overclock_summary(char *out, size_t cap);

#endif
