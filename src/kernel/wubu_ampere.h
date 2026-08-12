/*
 * wubu_ampere.h -- kernel-owned NVIDIA Ampere routing.
 */
#ifndef WUBU_AMPERE_H
#define WUBU_AMPERE_H

#include <stddef.h>

void wubu_ampere_probe(void);
int  wubu_ampere_present(void);
int  wubu_ampere_has_raytracing(int rt_cores);
int  wubu_ampere_vulkan(int vulkan_level);
void wubu_ampere_summary(char *out, size_t cap);

#endif
