/*
 * wubu_navi10.h -- kernel-owned AMD Navi10 RDNA1 routing.
 */
#ifndef WUBU_NAVI10_H
#define WUBU_NAVI10_H

#include <stddef.h>

void wubu_navi10_probe(void);
int  wubu_navi10_present(void);
int  wubu_navi10_uses_radv(int vulkan_available);
int  wubu_navi10_kernel_min(int kernel_version);
void wubu_navi10_summary(char *out, size_t cap);

#endif
