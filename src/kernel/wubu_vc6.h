/*
 * wubu_vc6.h -- kernel-owned Broadcom VideoCore VI routing.
 */
#ifndef WUBU_VC6_H
#define WUBU_VC6_H

#include <stddef.h>

void wubu_vc6_probe(void);
int  wubu_vc6_present(void);
int  wubu_vc6_uses_v3d(int v3d_available);
int  wubu_vc6_has_vulkan(int vulkan_available);
void wubu_vc6_summary(char *out, size_t cap);

#endif
