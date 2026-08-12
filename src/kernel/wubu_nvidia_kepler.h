/*
 * wubu_nvidia_kepler.h -- kernel-owned NVIDIA Kepler legacy routing.
 */
#ifndef WUBU_NVIDIA_KEPLER_H
#define WUBU_NVIDIA_KEPLER_H

#include <stddef.h>

void wubu_nvidia_kepler_probe(void);
int  wubu_nvidia_kepler_present(void);
int  wubu_nvidia_kepler_needs_legacy(int kepler);
int  wubu_nvidia_kepler_nvk_support(int gen);
void wubu_nvidia_kepler_summary(char *out, size_t cap);

#endif
