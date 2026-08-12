/*
 * wubu_perfmon.h -- kernel-owned GPU perf counters routing.
 */
#ifndef WUBU_PERFMON_H
#define WUBU_PERFMON_H

#include <stddef.h>

void wubu_perfmon_probe(void);
int  wubu_perfmon_present(void);
int  wubu_perfmon_event(void);
int  wubu_perfmon_cycles(void);
int  wubu_perfmon_cache(void);
int  wubu_perfmon_occ(void);
const char *wubu_perfmon_driver(void);
const char *wubu_perfmon_metric_for(const char *m);
const char *wubu_perfmon_api_for(const char *a);
int wubu_perfmon_summary(char *out, size_t cap);

#endif
