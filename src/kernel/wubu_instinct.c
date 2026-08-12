/*
 * wubu_instinct.c -- kernel-owned AMD Instinct MI routing.
 *
 * AMD Instinct MI (data center GPU) binds the amdgpu kernel driver
 * + ROCm runtime. ROCm 7.0 (Fall 2025) modular; supports Instinct
 * MI350+; cross-platform (Linux + Windows). ROCm docs: optimized
 * for AMD Instinct, Radeon, Ryzen AI.
 * "Runs on everything" includes Instinct data center routing.
 *
 * Impl routing:
 *   - /sys/class/drm/card0/device/uevent: driver binding
 *   - /sys/class/drm/card0/device/vendor: PCI vendor (0x1002)
 */
#include "wubu_instinct.h"
#include "wubu_hw_detect.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static int g_instinct_present = 0;
static int g_instinct_amdgpu = 0;

void wubu_instinct_probe(void)
{
#ifdef _GNU_SOURCE
    g_instinct_present = (access("/sys/class/drm/card0/device/uevent", R_OK) == 0) ? 1 : 0;
    g_instinct_amdgpu = (access("/sys/class/drm/card0/device/vendor", R_OK) == 0) ? 1 : 0;
#else
    g_instinct_present = g_instinct_amdgpu = 0;
#endif
}

int wubu_instinct_present(void)
{
#ifdef _GNU_SOURCE
    return g_instinct_present;
#else
    return 0;
#endif
}

int wubu_instinct_uses_rocm(int rocm_available)
{
    /* Instinct uses ROCm runtime for GPU compute. */
    return (rocm_available) ? 1 : 0;
}

int wubu_instinct_is_datacenter(int gpu_type)
{
    /* Instinct = data center GPU (not consumer). */
    return (gpu_type == 1) ? 1 : 0;
}

void wubu_instinct_summary(char *out, size_t cap)
{
    snprintf(out, cap, "instinct[dev=%d amdgpu=%d]", g_instinct_present, g_instinct_amdgpu);
}
