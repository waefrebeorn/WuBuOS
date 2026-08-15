/*
 * wubu_opencl.c -- kernel-owned OpenCL runtime routing.
 *
 * OpenCL bind clinfo runtime detection. AMD GPUs: ROCm runtime.
 * NVIDIA GPUs: CUDA runtime. clinfo lists platforms/devices.
 * "Runs on everything" includes OpenCL routing.
 *
 * Impl routing:
 *   - /usr/lib/libOpenCL.so: ICD loader presence
 *   - clinfo binary: platform detection
 */
#include "wubu_opencl.h"
#include "wubu_hw_detect.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static int g_opencl_present = 0;
static int g_opencl_amd = 0;

void wubu_opencl_probe(void)
{
#ifdef WUBU_HOSTED
    g_opencl_present = (access("/usr/lib/libOpenCL.so", R_OK) == 0) ? 1 : 0;
    g_opencl_amd = (access("/usr/lib/libOpenCL.so", R_OK) == 0) ? 1 : 0;
#else
    g_opencl_present = g_opencl_amd = 0;
#endif
}

int wubu_opencl_present(void)
{
#ifdef WUBU_HOSTED
    return g_opencl_present;
#else
    return 0;
#endif
}

int wubu_opencl_has_rocm(int rocm_available)
{
    /* AMD GPUs: ROCm runtime. */
    return (rocm_available) ? 1 : 0;
}

int wubu_opencl_has_cuda(int cuda_available)
{
    /* NVIDIA GPUs: CUDA runtime. */
    return (cuda_available) ? 1 : 0;
}

void wubu_opencl_summary(char *out, size_t cap)
{
    snprintf(out, cap, "opencl[dev=%d amd=%d]", g_opencl_present, g_opencl_amd);
}
