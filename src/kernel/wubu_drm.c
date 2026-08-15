/*
 * wubu_drm.c -- kernel-owned GPU DRM routing.
 *
 * DRM (Direct Rendering Manager) manages GPU display + buffer management.
 * "Runs on everything" includes correct DRM on every GPU.
 *
 * DRM:
 *   - /dev/dri/card*: DRM render nodes
 *-   - /sys/class/drm: connector, encoder, CRTC, plane
 *   - KMS: Kernel Mode Setting
 *   - GEM: Graphics Execution Manager (buffer)
 *   - PRIME: buffer sharing between GPUs
 *   - /sys/class/drm/card device: GPU device
 *   - amdgpu: amdgpu driver, DRM
 *   - i915: i915 driver, DRM
 *   - nouveau: nouveau driver, DRM
 *
 * WuBuOS owns this: detect DRM + KMS + GEM + PRIME, route to the
 * right driver, expose the topology.
 *
 * Research (7-hop on the drm frontier):
 *   -Direct Rendering Manager DRM
 */
#include "wubu_drm.h"
#include "wubu_hw_detect.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static int  g_drm = 0;         /* DRM present */
static int  g_kms = 0;         /* KMS */
static int  g_gem = 0;         /* GEM */
static int  g_prime = 0;       /* PRIME */
static int  g_msi = 0;         /* MSI */
static char g_drm_drv[24] = "";

void wubu_drm_probe(void)
{
    g_drm = 0; g_kms = 0; g_gem = 0; g_prime = 0; g_msi = 0;
    g_drm_drv[0] = '\0';

#ifdef WUBU_HOSTED
    if (access("/dev/dri/card0", R_OK) == 0 ||
        access("/sys/class/drm", R_OK) == 0) {
        g_drm = 1; g_kms = 1; g_gem = 1; g_prime = 1;
        strcpy(g_drm_drv, "drm-core");
    }
    if (access("/sys/module/amdgpu", R_OK) == 0) {
        g_drm = 1; g_kms = 1; g_gem = 1;
        if (!g_drm_drv[0]) strcpy(g_drm_drv, "amdgpu-drm");
    }
    if (access("/sys/module/i915", R_OK) == 0) {
        g_drm = 1; g_kms = 1; g_gem = 1;
        if (!g_drm_drv[0]) strcpy(g_drm_drv, "i915-drm");
    }
    if (access("/sys/module/nouveau", R_OK) == 0) {
        g_drm = 1; g_gem = 1;
        if (!g_drm_drv[0]) strcpy(g_drm_drv, "nouveau-drm");
    }
#endif
}

int  wubu_drm_present(void){ return g_drm; }
int  wubu_drm_kms(void)    { return g_kms; }
int  wubu_drm_gem(void)    { return g_gem; }
int  wubu_drm_prime(void)   { return g_prime; }
int  wubu_drm_msi(void)    { return g_msi; }
const char *wubu_drm_driver(void){ return g_drm_drv[0] ? g_drm_drv : NULL; }

const char *wubu_drm_subsys_for(const char *s)
{
    if (!s) return NULL;
    if (strstr(s, "amdgpu") || strstr(s, "amd")) return "amdgpu";
    if (strstr(s, "i915") || strstr(s, "intel")) return "i915";
    if (strstr(s, "nouveau")) return "nouveau";
    if (strstr(s, "nouveau")) return "nouveau";
    if (strstr(s, "mgag200") || strstr(s, "mga")) return "mgag200";
    if (strstr(s, "ast")) return "ast";
    if (strstr(s, " virtio") || strstr(s, "virgl")) return "virtio-gpu";
    if (strstr(s, "bochs")) return "bochs";
    return "amdgpu";
}

const char *wubu_drm_obj_for(const char *o)
{
    if (!o) return NULL;
    if (strstr(o, "crtc") || strstr(o, "crtc-")) return "CRTC";
    if (strstr(o, "conn") || strstr(o, "connector")) return "Connector";
    if (strstr(o, "enc") || strstr(o, "encoder")) return "Encoder";
    if (strstr(o, "plane")) return "Plane";
    if (strstr(o, "fb") || strstr(o, "framebuffer")) return "Framebuffer";
    if (strstr(o, "gem")) return "GEM";
    return "Framebuffer";
}

int wubu_drm_summary(char *out, size_t cap)
{
    return snprintf(out, cap,
        "drm[drm=%d kms=%d gem=%d prime=%d msi=%d drv=%s]",
        g_drm, g_kms, g_gem, g_prime, g_msi,
        wubu_drm_driver() ? wubu_drm_driver() : "none");
}
