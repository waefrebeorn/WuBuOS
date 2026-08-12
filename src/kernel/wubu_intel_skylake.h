/*
 * wubu_intel_skylake.h -- kernel-owned Intel Gen9 Skylake routing.
 */
#ifndef WUBU_INTEL_SKYLAKE_H
#define WUBU_INTEL_SKYLAKE_H

#include <stddef.h>

void wubu_intel_skylake_probe(void);
int  wubu_intel_skylake_present(void);
int  wubu_intel_skylake_uses_iris(int gl_available);
int  wubu_intel_skylake_has_anv(int anv_available);
void wubu_intel_skylake_summary(char *out, size_t cap);

#endif
