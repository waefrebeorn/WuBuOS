/*
 * wubu_cuda.h -- kernel-owned CUDA runtime routing.
 */
#ifndef WUBU_CUDA_H
#define WUBU_CUDA_H

#include <stddef.h>

void wubu_cuda_probe(void);
int  wubu_cuda_present(void);
int  wubu_cuda_has_cuda_cc(int cuda_available);
int  wubu_cuda_min_version(int cc);
void wubu_cuda_summary(char *out, size_t cap);

#endif
