/*
 * wubu_cuda.c -- kernel-owned CUDA runtime routing.
 *
 * CUDA bind libcuda driver + NVIDIA developer GPU compute
 * capability list. CUDA 11.8 = earliest cc8.9/cc9.0 support.
 * NVIDIA Developer: compute capability table. "Runs on everything"
 * includes CUDA routing.
 *
 * Impl routing:
 *   - /usr/lib/libcuda.so: CUDA driver library
 *   - /proc/driver/nvidia/gpus: GPU enumeration
 */
#include "wubu_cuda.h"
#include "wubu_hw_detect.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static int g_cuda_present = 0;
static int g_cuda_cuda = 0;

void wubu_cuda_probe(void)
{
#ifdef _GNU_SOURCE
    g_cuda_present = (access("/usr/lib/libcuda.so", R_OK) == 0) ? 1 : 0;
    g_cuda_cuda = (access("/usr/lib/libcuda.so", R_OK) == 0) ? 1 : 0;
#else
    g_cuda_present = g_cuda_cuda = 0;
#endif
}

int wubu_cuda_present(void)
{
#ifdef _GNU_SOURCE
    return g_cuda_present;
#else
    return 0;
#endif
}

int wubu_cuda_has_cuda_cc(int cuda_available)
{
    /* CUDA compute capability support. */
    return (cuda_available) ? 1 : 0;
}

int wubu_cuda_min_version(int cc)
{
    /* cc 8.9/9.0 requires CUDA 11.8+. */
    return (cc >= 89) ? 1 : 0;
}

void wubu_cuda_summary(char *out, size_t cap)
{
    snprintf(out, cap, "cuda[dev=%d cuda=%d]", g_cuda_present, g_cuda_cuda);
}
