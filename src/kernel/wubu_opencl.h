/*
 * wubu_opencl.h -- kernel-owned OpenCL runtime routing.
 */
#ifndef WUBU_OPENCL_H
#define WUBU_OPENCL_H

#include <stddef.h>

void wubu_opencl_probe(void);
int  wubu_opencl_present(void);
int  wubu_opencl_has_rocm(int rocm_available);
int  wubu_opencl_has_cuda(int cuda_available);
void wubu_opencl_summary(char *out, size_t cap);

#endif
