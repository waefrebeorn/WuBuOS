/*
 * wubu_gpucsched.h -- kernel-owned GPU compute scheduler routing.
 */
#ifndef WUBU_GPUCSCHED_H
#define WUBU_GPUCSCHED_H

#include <stddef.h>

void wubu_gpucsched_probe(void);
int  wubu_gpucsched_present(void);
int  wubu_gpucsched_priority(int base);
int  wubu_gpucsched_timeslice_ms(int queue);
void wubu_gpucsched_summary(char *out, size_t cap);

#endif
