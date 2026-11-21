/*
 * wubu_fantml.h -- kernel-owned GPU fan + thermal routing.
 */
#ifndef WUBU_FANTML_H
#define WUBU_FANTML_H

#include <stddef.h>

void wubu_fantml_probe(void);
void wubu_fantml_probe_root(const char *root);
int  wubu_fantml_present(void);
/* -1 means no sensor; 0 RPM is a real stopped-fan reading. */
int  wubu_fantml_fan_rpm(void);
int  wubu_fantml_fan_pwm(void);
int  wubu_fantml_fan_count(void);
const char *wubu_fantml_source(void);
const char *wubu_fantml_status_str(int temp_c);
int  wubu_fantml_fan_pct(int rpm, int max_rpm);
void wubu_fantml_summary(char *out, size_t cap);

#endif
