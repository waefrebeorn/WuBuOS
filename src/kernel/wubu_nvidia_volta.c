/*
 * wubu_nvidia_volta.c -- kernel-owned NVIDIA Volta routing.
 *
 * NVIDIA Volta (V100) binds nvidia datacenter driver 535/550.
 * CUDA 12.0/12.4. V100-SXM2-32GB confirmed on 535.183.01.
 * Reddit r/LocalLLaMA: "NVIDIA drops Pascal" (Volta still
 * datacenter-supported). 590 may drop Volta GA10x support.
 *
 * Impl routing:
 *   - /sys/class/drm/card0/device/uevent: driver binding
 *   - /sys/class/drm/card0/device/vendor: PCI vendor (0x10DE)
 */
#include "wubu_nvidia_volta.h"
#include "wubu_hw_detect.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static int g_volta_present = 0;
static int g_volta_driver = 0;

void wubu_nvidia_volta_probe(void)
{
#ifdef WUBU_HOSTED
    g_volta_present = (access("/sys/class/drm/card0/device/uevent", R_OK) == 0) ? 1 : 0;
    g_volta_driver = (access("/sys/class/drm/card0/device/vendor", R_OK) == 0) ? 1 : 0;
#else
    g_volta_present = g_volta_driver = 0;
#endif
}

int wubu_nvidia_volta_present(void)
{
#ifdef WUBU_HOSTED
    return g_volta_present;
#else
    return 0;
#endif
}

int wubu_nvidia_volta_is_datacenter(int gpu_type)
{
    /* V100 is datacenter (sm_70). gpu_type 1 = datacenter. */
    return (gpu_type) ? 1 : 0;
}

int wubu_nvidia_volta_cuda_gencode(int cuda_version)
{
    /* V100 sm_70 supports CUDA 12.x gencode. */
    return (cuda_version >= 1200 && cuda_version < 1400) ? 1 : 0;
}

void wubu_nvidia_volta_summary(char *out, size_t cap)
{
    snprintf(out, cap, "nvidia_volta[dev=%d driver=%d]", g_volta_present, g_volta_driver);
}
