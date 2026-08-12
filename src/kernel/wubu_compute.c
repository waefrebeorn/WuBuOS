/*
 * wubu_compute.c -- kernel-owned graphics compute (OpenCL/Vulkan) routing.
 *
 * GPU compute is the AGI's workhorse: OpenCL, Vulkan compute, CUDA/ZLUDA,
 * and the mesa rusticl driver. "Runs on everything" includes GPGPU
 * acceleration. The kernel must route the compute stack to the right
 * driver + ICD and expose the compute topology.
 *
 * Compute stacks (by GPU vendor):
 *   - AMD: rusticl (mesa OpenCL on radv), ROCm, Vulkan radv compute
 *   - Intel: oneAPI (level-zero), rusticl, Vulkan ANV compute
 *   - NVIDIA: CUDA (proprietary), ZLUDA (CUDA-on-Vulkan), vulkan compute
 *   - Software: llvmpipe/lavapipe (vulkan sw), pocl (opencl sw)
 *   - Mesa: rusticl = the unified OpenCL-on-Vulkan driver
 *
 * WuBuOS owns this: detect the compute stack (OpenCL/Vulkan ICDs), route
 * to the right driver, and expose the compute topology.
 *
 * Research (Kevin-Bacon 7-hop on the graphics-compute frontier):
 *   - OpenCL: mesa rusticl (OpenCL-on-Vulkan), ROCm (AMD), CUDA (NVIDIA)
 *   - Vulkan compute: radv (AMD), ANV (Intel), lavapipe (sw)
 *   - ZLUDA: NVIDIA CUDA translated to Vulkan
 *   - pocl: portable OpenCL (CPU + accelerator)
 */
#include "wubu_compute.h"
#include "wubu_hw_detect.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

/* ---- Global state ---- */
static int  g_opencl = 0;
static int  g_vulkan_compute = 0;
static int  g_cuda = 0;
static int  g_rusticl = 0;
static char g_compute_drv[32] = "";
static char g_compute_vendor[24] = "";

/* ---- W1: probe the compute topology ---- */
void wubu_compute_probe(void)
{
    g_opencl = 0; g_vulkan_compute = 0; g_cuda = 0; g_rusticl = 0;
    g_compute_drv[0] = '\0'; g_compute_vendor[0] = '\0';

#ifdef _GNU_SOURCE
    /* rusticl (mesa OpenCL-on-Vulkan) present? */
    if (access("/usr/lib/x86_64-linux-gnu/libRusticlOpenCL.so", R_OK) == 0 ||
        access("/usr/lib/x86_64-linux-gnu/mesa/libRusticlOpenCL.so", R_OK) == 0) {
        g_rusticl = 1; g_opencl = 1;
        strcpy(g_compute_drv, "rusticl");
    }
    /* CUDA (NVIDIA) present? */
    if (access("/usr/local/cuda", R_OK) == 0 ||
        access("/usr/lib/x86_64-linux-gnu/libcuda.so", R_OK) == 0) {
        g_cuda = 1;
        strcpy(g_compute_vendor, "NVIDIA");
        if (!g_compute_drv[0]) strcpy(g_compute_drv, "cuda");
    }
    /* Vulkan compute ICDs (radv/ANV/lavapipe). */
    if (access("/usr/share/vulkan/icd.d/radv_icd.x86_64.json", R_OK) == 0 ||
        access("/usr/share/vulkan/icd.d/intel_icd.x86_64.json", R_OK) == 0 ||
        access("/usr/share/vulkan/icd.d/lvp_icd.x86_64.json", R_OK) == 0) {
        g_vulkan_compute = 1;
        if (!g_compute_drv[0]) strcpy(g_compute_drv, "vulkan-compute");
    }
    /* OpenCL via any ICD (pocl, ROCm). */
    if (access("/etc/OpenCL/vendors", R_OK) == 0) {
        if (!g_opencl) g_opencl = 1;
        if (!g_compute_drv[0]) strcpy(g_compute_drv, "opencl");
    }
#endif
}

/* ---- W2: accessors ---- */
int  wubu_compute_opencl(void)     { return g_opencl; }
int  wubu_compute_vulkan(void)     { return g_vulkan_compute; }
int  wubu_compute_cuda(void)       { return g_cuda; }
int  wubu_compute_rusticl(void)    { return g_rusticl; }
int  wubu_compute_present(void)    { return (g_opencl || g_vulkan_compute || g_cuda); }
const char *wubu_compute_driver(void){ return g_compute_drv[0] ? g_compute_drv : NULL; }
const char *wubu_compute_vendor(void){ return g_compute_vendor[0] ? g_compute_vendor : NULL; }

/* ---- W3: compute driver routing ---- */
const char *wubu_compute_driver_for(const char *gpu)
{
    if (!gpu) return NULL;
    if (strstr(gpu, "amd"))    return "rusticl";
    if (strstr(gpu, "intel"))  return "rusticl";
    if (strstr(gpu, "nvidia")) return "cuda";
    if (strstr(gpu, "zlu"))    return "zlu";
    return "pocl";
}

/* ---- W4: summary ---- */
int wubu_compute_summary(char *out, size_t cap)
{
    return snprintf(out, cap,
        "compute[opencl=%d vulkan=%d cuda=%d rusticl=%d drv=%s vendor=%s]",
        g_opencl, g_vulkan_compute, g_cuda, g_rusticl,
        wubu_compute_driver() ? wubu_compute_driver() : "none",
        wubu_compute_vendor() ? wubu_compute_vendor() : "-");
}
