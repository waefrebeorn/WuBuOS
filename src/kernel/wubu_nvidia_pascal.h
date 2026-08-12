/*
 * wubu_nvidia_pascal.h -- kernel-owned NVIDIA Pascal routing.
 */
#ifndef WUBU_NVIDIA_PASCAL_H
#define WUBU_NVIDIA_PASCAL_H

#include <stddef.h>

void wubu_nvidia_pascal_probe(void);
int  wubu_nvidia_pascal_present(void);
int  wubu_nvidia_pascal_needs_535(int cuda_version);
int  wubu_nvidia_pascal_max_cuda(int driver_major);
void wubu_nvidia_pascal_summary(char *out, size_t cap);

#endif
