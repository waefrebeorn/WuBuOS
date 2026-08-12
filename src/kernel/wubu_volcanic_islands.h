/*
 * wubu_volcanic_islands.h -- kernel-owned AMD GCN3 Volcanic routing.
 */
#ifndef WUBU_VOLCANIC_ISLANDS_H
#define WUBU_VOLCANIC_ISLANDS_H

#include <stddef.h>

void wubu_volcanic_islands_probe(void);
int  wubu_volcanic_islands_present(void);
int  wubu_volcanic_islands_uses_amdgpu(int amdgpu_folded);
int  wubu_volcanic_islands_radv(int vulkan_available);
void wubu_volcanic_islands_summary(char *out, size_t cap);

#endif
