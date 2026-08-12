/*
 * wubu_intelgpu.h -- kernel-owned Intel GPU routing.
 */
#ifndef WUBU_intelgpu_H
#define WUBU_intelgpu_H

#include <stddef.h>

void wubu_intelgpu_probe(void);
int  wubu_intelgpu_present(void);
int  wubu_intelgpu_driver(int gen);
int  wubu_intelgpu_needs_firmware(int gen);
void wubu_intelgpu_summary(char *out, size_t cap);

#endif
