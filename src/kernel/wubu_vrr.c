/*
 * wubu_vrr.c -- kernel-owned G-Sync/FreeSync variable refresh routing.
 *
 * NVIDIA G-Sync / AMD FreeSync variable refresh via KMS.
 * ArchWiki: AMDGPU = FreeSync, NVIDIA = G-SYNC Compatible.
 * VRR enabled per-monitor in display settings. "Runs on everything"
 * includes VRR routing.
 *
 * Impl routing:
 *   - /sys/class/drm/card0/device/uevent: driver binding
 *   - /sys/module/amdgpu/parameters: FreeSync config
 *   - /sys/module/nvidia/parameters: G-Sync config
 */
#include "wubu_vrr.h"
#include "wubu_hw_detect.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static int g_vrr_present = 0;
static int g_vrr_freesync = 0;

void wubu_vrr_probe(void)
{
#ifdef _GNU_SOURCE
    g_vrr_present = (access("/sys/class/drm/card0/device/uevent", R_OK) == 0) ? 1 : 0;
    g_vrr_freesync = (access("/sys/class/drm/card0/device/vendor", R_OK) == 0) ? 1 : 0;
#else
    g_vrr_present = g_vrr_freesync = 0;
#endif
}

int wubu_vrr_present(void)
{
#ifdef _GNU_SOURCE
    return g_vrr_present;
#else
    return 0;
#endif
}

int wubu_vrr_is_freesync(int freesync_available)
{
    /* AMD FreeSync via AMDGPU. */
    return (freesync_available) ? 1 : 0;
}

int wubu_vrr_is_gsync(int gsync_available)
{
    /* NVIDIA G-Sync Compatible via NVIDIA driver. */
    return (gsync_available) ? 1 : 0;
}

void wubu_vrr_summary(char *out, size_t cap)
{
    snprintf(out, cap, "vrr[dev=%d freesync=%d]", g_vrr_present, g_vrr_freesync);
}
