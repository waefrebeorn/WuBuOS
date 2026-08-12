/*
 * wubu_nvidia_maxwell.h -- kernel-owned NVIDIA Maxwell routing.
 */
#ifndef WUBU_NVIDIA_MAXWELL_H
#define WUBU_NVIDIA_MAXWELL_H

#include <stddef.h>

void wubu_nvidia_maxwell_probe(void);
int  wubu_nvidia_maxwell_present(void);
int  wubu_nvidia_maxwell_uses_proprietary(int nvidia_version);
int  wubu_nvidia_maxwell_has_nvk(int nvk_status);
void wubu_nvidia_maxwell_summary(char *out, size_t cap);

#endif
