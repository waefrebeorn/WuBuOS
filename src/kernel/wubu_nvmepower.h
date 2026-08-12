/*
 * wubu_nvmepower.h -- kernel-owned NVMe power state routing.
 */
#ifndef WUBU_NVMEPOWER_H
#define WUBU_NVMEPOWER_H

#include <stddef.h>

void wubu_nvmepower_probe(void);
int  wubu_nvmepower_present(void);
int  wubu_nvmepower_state(int state);
int  wubu_nvmepower_apst_latency(int us);
void wubu_nvmepower_summary(char *out, size_t cap);

#endif
