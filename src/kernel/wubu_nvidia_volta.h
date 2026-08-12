/*
 * wubu_nvidia_volta.h -- kernel-owned NVIDIA Volta routing.
 */
#ifndef WUBU_NVIDIA_VOLTA_H
#define WUBU_NVIDIA_VOLTA_H

#include <stddef.h>

void wubu_nvidia_volta_probe(void);
int  wubu_nvidia_volta_present(void);
int  wubu_nvidia_volta_is_datacenter(int gpu_type);
int  wubu_nvidia_volta_cuda_gencode(int cuda_version);
void wubu_nvidia_volta_summary(char *out, size_t cap);

#endif
