/*
 * wubu_radeon_legacy.c -- kernel-owned AMD Radeon legacy GPU routing.
 *
 * AMD Radeon HD 5000/6000 (Evergreen/Northern Islands, pre-GCN)
 * bind the legacy radeon driver. Linux 6.19 folds these into
 * amdgpu. "Runs on everything" includes correct legacy Radeon
 * routing on old and new stacks.
 *
 * Impl routing:
 *   - /sys/class/drm/card0/device/uevent: driver binding
 *   - /sys/class/drm/card0/device/vendor: PCI vendor (0x1002)
 */
#include "wubu_radeon_legacy.h"
#include "wubu_hw_detect.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static int g_radeon_legacy_present = 0;
static int g_radeon_legacy_amd = 0;

void wubu_radeon_legacy_probe(void)
{
#ifdef _GNU_SOURCE
    g_radeon_legacy_present = (access("/sys/class/drm/card0/device/uevent", R_OK) == 0) ? 1 : 0;
    g_radeon_legacy_amd = (access("/sys/class/drm/card0/device/vendor", R_OK) == 0) ? 1 : 0;
#else
    g_radeon_legacy_present = g_radeon_legacy_amd = 0;
#endif
}

int wubu_radeon_legacy_present(void)
{
#ifdef _GNU_SOURCE
    return g_radeon_legacy_present;
#else
    return 0;
#endif
}

int wubu_radeon_legacy_gen(int family)
{
    /* Family: 0=unknown, 1=Evergreen(HD5000), 2=NI(HD6000), 3=pre-GCN other. */
    if (family < 1) return 0;
    if (family == 1) return 1;
    if (family == 2) return 2;
    return 3;
}

int wubu_radeon_legacy_supported_by_amdgpu(int family)
{
    /* 6.19 folds HD5000/6000 (families 1-2) into amdgpu. */
    return (family >= 1 && family <= 2) ? 1 : 0;
}

void wubu_radeon_legacy_summary(char *out, size_t cap)
{
    snprintf(out, cap, "radeon_legacy[dev=%d amd=%d]", g_radeon_legacy_present, g_radeon_legacy_amd);
}
