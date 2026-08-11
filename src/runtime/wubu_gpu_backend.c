/*
 * wubu_gpu_backend.c -- the GPU BACKEND DISPATCHER (the magic glue).
 *
 * "It should work on bare metal or not bare metal because we are a magical
 *  operating system."
 *
 * This module abstracts the GPU device so the rest of the OS doesn't care:
 *   - WSL2: /dev/dxg + gfxstream Vulkan ICD (DirectX->Vulkan translation)
 *   - Bare metal NVIDIA: /dev/nvidia0 + real Vulkan ICD
 *   - Bare metal AMD/Intel: /dev/dri/card0 + Mesa Vulkan ICD
 *
 * It picks the path automatically via wubu_hw_detect() and exposes a
 * single interface to the compositor / BearRL / Vulkan compute:
 *
 *   wubu_gpu_open()      -> device fd (or -1)
 *   wubu_gpu_device_name() -> "nvidia:///dev/dxg" etc
 *   wubu_gpu_vram_mb()   -> reported VRAM
 *
 * The Vulkan ICD is discovered from /usr/share/vulkan/icd.d/*.json — no
 * config file, no user choice. The OS is magical; it just works.
 *
 * C11. Uses stdio for JSON parsing + device ops.
 */
#include "../kernel/wubu_hw_detect.h"
#include "../kernel/wubu_drv_gpu.h"
#include "wubu_gpu_backend.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>

/* the resolved backend (set by wubu_gpu_init()) */
static int   g_fd           = -1;
static int   g_vram_mb      = 0;
static char  g_device_name[128] = "";

/* ---- B1: pick the ICD json matching our GPU vendor ---- */
static const char *find_icd_for_vendor(const char *vendor)
{
    DIR *d = opendir("/usr/share/vulkan/icd.d");
    if (!d) return NULL;

    struct dirent *ent;
    static char path[256];
    while ((ent = readdir(d))) {
        if (!strstr(ent->d_name, ".json")) continue;
        snprintf(path, sizeof(path), "/usr/share/vulkan/icd.d/%s", ent->d_name);

        FILE *f = fopen(path, "r");
        if (!f) continue;
        char line[512];
        int is_nvidia = 0, is_amd = 0, is_intel = 0, is_dxg = 0;
        while (fgets(line, sizeof(line), f)) {
            if (strstr(line, "nvidia")) is_nvidia = 1;
            if (strstr(line, "amd") || strstr(line, "radeon")) is_amd = 1;
            if (strstr(line, "intel")) is_intel = 1;
            if (strstr(line, "dxgkrnl") || strstr(line, "gfxstream")) is_dxg = 1;
        }
        fclose(f);

        if ((strcmp(vendor, "nvidia") == 0 && is_nvidia) ||
            (strcmp(vendor, "amd") == 0 && is_amd && !is_nvidia) ||
            (strcmp(vendor, "intel") == 0 && is_intel && !is_nvidia) ||
            (strcmp(vendor, "dxg") == 0 && is_dxg))
            return path;
    }
    closedir(d);
    return NULL;
}

/* ---- B2: auto-init (the magic) ---- */
int wubu_gpu_init(void)
{
    /* ask the hardware detector what we are */
    wubu_hw_detect();

    g_fd = -1;
    g_vram_mb = 0;
    g_device_name[0] = '\0';

    const char *platform = wubu_hw_platform();
    const char *gpu_path = wubu_hw_gpu_path();

    if (!gpu_path) {
        /* No GPU at all — headless AGI. Still report so the Brain knows. */
        snprintf(g_device_name, sizeof(g_device_name), "none");
        return -1;
    }

    /* Open the GPU device */
    g_fd = open(gpu_path, O_RDWR | O_CLOEXEC);
    if (g_fd < 0) g_fd = open(gpu_path, O_RDONLY | O_CLOEXEC);

    /* Determine the vendor + ICD */
    const char *icd = NULL;
    if (strstr(platform, "wsl") || access("/dev/dgx", R_OK) == 0 || gpu_path) {
        /* On WSL2, the path is /dev/dxg and the ICD is gfxstream */
        if (g_fd >= 0) {
            icd = find_icd_for_vendor("dxg");
            if (!icd) icd = find_icd_for_vendor("nvidia");  /* fallback */
        }
    }
    if (!icd && g_fd >= 0) {
        icd = find_icd_for_vendor("nvidia");
        if (!icd) icd = find_icd_for_vendor("amd");
        if (!icd) icd = find_icd_for_vendor("intel");
    }

    /* Report VRAM + device name */
    if (wubu_gpu_present_wsl()) {
        g_vram_mb = (uint32_t)wubu_gpu_vram_mb();
    } else {
        g_vram_mb = 2048;  /* conservative bare-metal default — backend probes */
    }

    if (g_fd >= 0) {
        snprintf(g_device_name, sizeof(g_device_name),
                 "%s%s%s%s%s",
                 platform,
                 gpu_path,
                 icd ? " icd=" : "",
                 icd ? icd : "",
                 "");
    } else {
        snprintf(g_device_name, sizeof(g_device_name), "%s", platform);
    }

    return g_fd >= 0 ? 0 : -1;
}

/* ---- B3: accessors ---- */
int          wubu_gpu_backend_fd(void)        { return g_fd; }
uint32_t     wubu_gpu_backend_vram_mb(void)    { return g_vram_mb; }
const char  *wubu_gpu_backend_device_name(void){ return g_device_name; }
