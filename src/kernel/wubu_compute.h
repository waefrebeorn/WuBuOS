/*
 * wubu_compute.h -- kernel-owned graphics compute (OpenCL/Vulkan) routing.
 */
#ifndef WUBU_COMPUTE_H
#define WUBU_COMPUTE_H

#include <stddef.h>

/* W1: probe the compute topology. */
void wubu_compute_probe(void);

/* W2: accessors */
int  wubu_compute_opencl(void);
int  wubu_compute_vulkan(void);
int  wubu_compute_cuda(void);
int  wubu_compute_rusticl(void);
int  wubu_compute_present(void);
const char *wubu_compute_driver(void);
const char *wubu_compute_vendor(void);

/* W3: compute driver routing. */
const char *wubu_compute_driver_for(const char *gpu);

/* W4: summary fragment. */
int wubu_compute_summary(char *out, size_t cap);

#endif /* WUBU_COMPUTE_H */
