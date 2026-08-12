/*
 * wubu_fantml.h -- kernel-owned GPU fan + thermal routing.
 */
#ifndef WUBU_FANTML_H
#define WUBU_FANTML_H

#include <stddef.h>

void wubu_fantml_probe(void);
int  wubu_fantml_present(void);
const char *wubu_fantml_status_str(int temp_c);
int  wubu_fantml_fan_pct(int rpm, int max_rpm);
void wubu_fantml_summary(char *out, size_t cap);

#endif
