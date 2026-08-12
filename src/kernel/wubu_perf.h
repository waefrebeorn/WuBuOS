/*
 * wubu_perf.h -- kernel-owned GPU performance counter routing.
 */
#ifndef WUBU_PERF_H
#define WUBU_PERF_H

#include <stddef.h>

void wubu_perf_probe(void);
int  wubu_perf_present(void);
const char *wubu_perf_engine_for(const char *name);
const char *wubu_perf_freq_str(void);
void wubu_perf_summary(char *out, size_t cap);

#endif
