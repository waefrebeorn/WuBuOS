/*
 * wubu_intel_gma.h -- kernel-owned Intel GMA legacy GPU routing.
 */
#ifndef WUBU_INTEL_GMA_H
#define WUBU_INTEL_GMA_H

#include <stddef.h>

void wubu_intel_gma_probe(void);
int  wubu_intel_gma_present(void);
int  wubu_intel_gma_uses_i915(int gen);
int  wubu_intel_gma_needs_llvmpipe(int accel_available);
void wubu_intel_gma_summary(char *out, size_t cap);

#endif
