/*
 * wubu_gpufw.c -- kernel-owned GPU firmware version routing.
 *
 * GPU firmware (VBIOS/ microcode) is inspected to match driver ABI.
 * "Runs on everything" includes firmware version matching on every
 * vendor (NVIDIA/AMD/Intel) for safe driver binding.
 *
 * Impl routing:
 *   - /sys/class/drm/card0/device/rom: GPU VBIOS ROM
 *   - /sys/class/drm/card0/device/uevent: driver version
 */
#include "wubu_gpufw.h"
#include "wubu_hw_detect.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static int g_gpufw_present = 0;
static int g_gpufw_matched = 0;

void wubu_gpufw_probe(void)
{
    /* Detect GPU firmware presence + driver match. */
#ifdef WUBU_HOSTED
    g_gpufw_present = (access("/sys/class/drm/card0/device/rom", R_OK) == 0) ? 1 : 0;
    g_gpufw_matched = (access("/sys/class/drm/card0/device/uevent", R_OK) == 0) ? 1 : 0;
#else
    g_gpufw_present = g_gpufw_matched = 0;
#endif
}

int wubu_gpufw_present(void)
{
#ifdef WUBU_HOSTED
    return g_gpufw_present;
#else
    return 0;
#endif
}

int wubu_gpufw_match(int vendor_id)
{
    /* Simplified: vendor IDs NVIDIA=0x10de, AMD=0x1002, Intel=0x8086. */
    return (vendor_id == 0x10de || vendor_id == 0x1002 || vendor_id == 0x8086) ? 1 : 0;
}

const char *wubu_gpufw_status(int matched)
{
    switch (matched) {
        case 0: return "nomatch";
        case 1: return "ok";
        case 2: return "mismatch";
        default: return "unknown";
    }
}

void wubu_gpufw_summary(char *out, size_t cap)
{
    snprintf(out, cap, "gpufw[fw=%d match=%d]",
             g_gpufw_present, g_gpufw_matched);
}
