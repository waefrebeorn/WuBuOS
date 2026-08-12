/*
 * wubu_radeon_5000.h -- kernel-owned AMD Radeon HD 5000 (Evergreen) routing.
 */
#ifndef WUBU_RADEON_5000_H
#define WUBU_RADEON_5000_H

#include <stddef.h>

void wubu_radeon_5000_probe(void);
int  wubu_radeon_5000_present(void);
int  wubu_radeon_5000_supports_legacy(int legacy_available);
int  wubu_radeon_5000_is_evergreen(int family);
void wubu_radeon_5000_summary(char *out, size_t cap);

#endif
