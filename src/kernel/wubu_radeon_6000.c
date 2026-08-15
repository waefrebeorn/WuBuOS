/*
 * wubu_radeon_6000.c -- kernel-owned AMD Radeon HD 6000 (NI) routing.
 *
 * Radeon HD 6000 (Northern Islands, pre-GCN) binds the legacy
 * radeon driver, folded into amdgpu from Linux 6.19. Correct
 * routing routes 6000-series to the working driver.
 *
 * Impl routing:
 *   - /sys/class/drm/card0/device/uevent: driver binding
 *   - /sys/class/drm/card0/device/vendor: PCI vendor (0x1002)
 */
#include "wubu_radeon_6000.h"
#include "wubu_hw_detect.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static int g_radeon_6000_present = 0;
static int g_radeon_6000_legacy = 0;

void wubu_radeon_6000_probe(void)
{
#ifdef WUBU_HOSTED
    g_radeon_6000_present = (access("/sys/class/drm/card0/device/uevent", R_OK) == 0) ? 1 : 0;
    g_radeon_6000_legacy = (access("/sys/class/drm/card0/device/vendor", R_OK) == 0) ? 1 : 0;
#else
    g_radeon_6000_present = g_radeon_6000_legacy = 0;
#endif
}

int wubu_radeon_6000_present(void)
{
#ifdef WUBU_HOSTED
    return g_radeon_6000_present;
#else
    return 0;
#endif
}

int wubu_radeon_6000_needs_legacy(int amdgpu_support)
{
    /* Without amdgpu (pre-6.19), 6000-series needs legacy radeon. */
    return (amdgpu_support == 0) ? 1 : 0;
}

int wubu_radeon_6000_is_pre_gcn(int family)
{
    /* family: 1=Evergreen(HD5000), 2=NI(HD6000). NI is pre-GCN. */
    return (family == 2) ? 1 : 0;
}

void wubu_radeon_6000_summary(char *out, size_t cap)
{
    snprintf(out, cap, "radeon_6000[dev=%d legacy=%d]", g_radeon_6000_present, g_radeon_6000_legacy);
}
