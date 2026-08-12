/*
 * wubu_radeon_6000.h -- kernel-owned AMD Radeon HD 6000 (NI) routing.
 */
#ifndef WUBU_RADEON_6000_H
#define WUBU_RADEON_6000_H

#include <stddef.h>

void wubu_radeon_6000_probe(void);
int  wubu_radeon_6000_present(void);
int  wubu_radeon_6000_needs_legacy(int amdgpu_support);
int  wubu_radeon_6000_is_pre_gcn(int family);
void wubu_radeon_6000_summary(char *out, size_t cap);

#endif
