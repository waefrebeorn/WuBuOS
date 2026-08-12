/*
 * wubu_intel_icelake.h -- kernel-owned Intel Gen11 Ice Lake routing.
 */
#ifndef WUBU_INTEL_ICELAKE_H
#define WUBU_INTEL_ICELAKE_H

#include <stddef.h>

void wubu_intel_icelake_probe(void);
int  wubu_intel_icelake_present(void);
int  wubu_intel_icelake_uses_iris(int gl_available);
int  wubu_intel_icelake_has_anv(int anv_available);
void wubu_intel_icelake_summary(char *out, size_t cap);

#endif
