/*
 * wubu_nvidia_turing.h -- kernel-owned NVIDIA Turing routing.
 */
#ifndef WUBU_NVIDIA_TURING_H
#define WUBU_NVIDIA_TURING_H

#include <stddef.h>

void wubu_nvidia_turing_probe(void);
int  wubu_nvidia_turing_present(void);
int  wubu_nvidia_turing_has_rt_core(int rt_supported);
int  wubu_nvidia_turing_vulkan(int vulkan_level);
void wubu_nvidia_turing_summary(char *out, size_t cap);

#endif
