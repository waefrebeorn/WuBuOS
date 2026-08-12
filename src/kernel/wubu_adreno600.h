/*
 * wubu_adreno600.h -- kernel-owned Qualcomm Adreno 600 GPU routing.
 */
#ifndef WUBU_ADRENO600_H
#define WUBU_ADRENO600_H

#include <stddef.h>

void wubu_adreno600_probe(void);
int  wubu_adreno600_present(void);
int  wubu_adreno600_has_vulkan(int vulkan_available);
int  wubu_adreno600_is_a6xx(int a6xx);
void wubu_adreno600_summary(char *out, size_t cap);

#endif
