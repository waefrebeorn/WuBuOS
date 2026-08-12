/*
 * wubu_radeon_legacy.h -- kernel-owned AMD Radeon legacy GPU routing.
 */
#ifndef WUBU_RADEON_LEGACY_H
#define WUBU_RADEON_LEGACY_H

#include <stddef.h>

void wubu_radeon_legacy_probe(void);
int  wubu_radeon_legacy_present(void);
int  wubu_radeon_legacy_gen(int family);
int  wubu_radeon_legacy_supported_by_amdgpu(int family);
void wubu_radeon_legacy_summary(char *out, size_t cap);

#endif
