/*
 * wubu_powergate.h -- kernel-owned GPU power gating routing.
 */
#ifndef WUBU_POWERGATE_H
#define WUBU_POWERGATE_H

#include <stddef.h>

void wubu_powergate_probe(void);
int  wubu_powergate_present(void);
int  wubu_powergate_runtime(void);
int  wubu_powergate_shader(void);
int  wubu_powergate_texture(void);
int  wubu_powergate_l2(void);
const char *wubu_powergate_driver(void);
const char *wubu_powergate_domain_for(const char *d);
const char *wubu_powergate_state_for(const char *s);
int wubu_powergate_summary(char *out, size_t cap);

#endif
