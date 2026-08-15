/*
 * wubu_nvidia_pascal.c -- kernel-owned NVIDIA Pascal routing.
 *
 * NVIDIA Pascal (GTX 10xx) binds nvidia 535/550/590 driver.
 * CUDA 12.0, OpenGL 4.6, Vulkan. Reddit: "NVIDIA drops Pascal
 * on Linux" (gradually phasing out). 535 supports Pascal with
 * CUDA 12.0. 470.xx also supports Pascal but is legacy.
 *
 * Impl routing:
 *   - /sys/class/drm/card0/device/uevent: driver binding
 *   - /sys/class/drm/card0/device/vendor: PCI vendor (0x10DE)
 */
#include "wubu_nvidia_pascal.h"
#include "wubu_hw_detect.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static int g_pascal_present = 0;
static int g_pascal_driver = 0;

void wubu_nvidia_pascal_probe(void)
{
#ifdef WUBU_HOSTED
    g_pascal_present = (access("/sys/class/drm/card0/device/uevent", R_OK) == 0) ? 1 : 0;
    g_pascal_driver = (access("/sys/class/drm/card0/device/vendor", R_OK) == 0) ? 1 : 0;
#else
    g_pascal_present = g_pascal_driver = 0;
#endif
}

int wubu_nvidia_pascal_present(void)
{
#ifdef WUBU_HOSTED
    return g_pascal_present;
#else
    return 0;
#endif
}

int wubu_nvidia_pascal_needs_535(int cuda_version)
{
    /* CUDA 12.0+ requires nvidia 535 or newer. */
    return (cuda_version >= 1200) ? 1 : 0;
}

int wubu_nvidia_pascal_max_cuda(int driver_major)
{
    /* 470 = legacy (no CUDA 12), 535 = CUDA 12, 550 = CUDA 12.x, 590+ = CUDA 13.x. */
    if (driver_major >= 590) return 13;
    if (driver_major >= 535) return 12;
    return 0;
}

void wubu_nvidia_pascal_summary(char *out, size_t cap)
{
    snprintf(out, cap, "nvidia_pascal[dev=%d driver=%d]", g_pascal_present, g_pascal_driver);
}
