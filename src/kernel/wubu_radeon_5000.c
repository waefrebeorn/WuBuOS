/*
 * wubu_radeon_5000.c -- kernel-owned AMD Radeon HD 5000 (Evergreen) routing.
 *
 * Radeon HD 5000 (Evergreen, pre-GCN) binds the legacy radeon
 * driver, folded into amdgpu from Linux 6.19. Correct routing
 * routes 5000-series to the working driver.
 *
 * Impl routing:
 *   - /sys/class/drm/card0/device/uevent: driver binding
 *   - /sys/class/drm/card0/device/vendor: PCI vendor (0x1002)
 */
#include "wubu_radeon_5000.h"
#include "wubu_hw_detect.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static int g_radeon_5000_present = 0;
static int g_radeon_5000_legacy = 0;

void wubu_radeon_5000_probe(void)
{
#ifdef WUBU_HOSTED
    g_radeon_5000_present = (access("/sys/class/drm/card0/device/uevent", R_OK) == 0) ? 1 : 0;
    g_radeon_5000_legacy = (access("/sys/class/drm/card0/device/vendor", R_OK) == 0) ? 1 : 0;
#else
    g_radeon_5000_present = g_radeon_5000_legacy = 0;
#endif
}

int wubu_radeon_5000_present(void)
{
#ifdef WUBU_HOSTED
    return g_radeon_5000_present;
#else
    return 0;
#endif
}

int wubu_radeon_5000_supports_legacy(int legacy_available)
{
    /* Legacy radeon driver supports Evergreen when available. */
    return (legacy_available == 0) ? 0 : 1;
}

int wubu_radeon_5000_is_evergreen(int family)
{
    return (family == 1) ? 1 : 0;
}

void wubu_radeon_5000_summary(char *out, size_t cap)
{
    snprintf(out, cap, "radeon_5000[dev=%d legacy=%d]", g_radeon_5000_present, g_radeon_5000_legacy);
}
