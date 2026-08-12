/*
 * wubu_voltagectl.h -- kernel-owned GPU voltage control routing.
 */
#ifndef WUBU_VOLTAGECTL_H
#define WUUB_VOLTAGECTL_H

#include <stddef.h>

void wubu_voltagectl_probe(void);
int  wubu_voltagectl_present(void);
const char *wubu_voltagectl_state_str(int mV);
int  wubu_voltagectl_mv_to_uv(int mV);
void wubu_voltagectl_summary(char *out, size_t cap);

#endif
