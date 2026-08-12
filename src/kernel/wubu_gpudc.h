/*
 * wubu_gpudc.h -- kernel-owned GPU display controller routing.
 */
#ifndef WUBU_GPUDC_H
#define WUBU_GPUDC_H

#include <stddef.h>

void wubu_gpudc_probe(void);
int  wubu_gpudc_present(void);
int  wubu_gpudc_type(int outputs);
const char *wubu_gpudc_status_str(int outputs);
void wubu_gpudc_summary(char *out, size_t cap);

#endif
