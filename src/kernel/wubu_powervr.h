/*
 * wubu_powervr.h -- kernel-owned Imagination PowerVR routing.
 */
#ifndef WUBU_POWERVr_H
#define WUBU_POWERVr_H

#include <stddef.h>

void wubu_powervr_probe(void);
int  wubu_powervr_present(void);
int  wubu_powervr_uses_pvrsrvkm(int kernel_616);
int  wubu_powervr_has_vulkan(int mesa_253);
void wubu_powervr_summary(char *out, size_t cap);

#endif
