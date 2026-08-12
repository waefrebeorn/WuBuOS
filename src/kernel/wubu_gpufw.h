/*
 * wubu_gpufw.h -- kernel-owned GPU firmware version routing.
 */
#ifndef WUBU_GPUFW_H
#define WUBU_GPUFW_H

#include <stddef.h>

void wubu_gpufw_probe(void);
int  wubu_gpufw_present(void);
int  wubu_gpufw_match(int vendor_id);
const char *wubu_gpufw_status(int matched);
void wubu_gpufw_summary(char *out, size_t cap);

#endif
