/*
 * wubu_drmx.c -- kernel-owned DRM writeback/overlay + HDR/color routing.
 *
 * Advanced display features beyond the base KMS driver:
 *   - DRM writeback: capture composed frames to a buffer (screen capture,
 *     virtual display, vkms)
 *   - Overlays/planes: hardware planes (video, cursor, scaled)
 *   - HDR (HDR10/HLG): output metadata (SDR luminance, PQ transfer)
 *   - Color management: gamma/LUT, color pipeline, CTM (color transform)
 *
 * WuBuOS owns this: detect the DRM writeback connector + HDR/color
 * capability, route to the right driver, and expose the display-advanced
 * topology.
 *
 * Research (Kevin-Bacon 7-hop on the DRM-advanced frontier):
 *   - DRM writeback: drm_writeback.c, /dev/dri writeback connectors
 *   - vkms: virtual kernel modesetting (writeback + testing)
 *   - DRM color: drm_color_mgmt (gamma LUT, CTM), HDR_OUTPUT_METADATA
 *   - HDR10: PQ transfer, SDR luminance, master display metadata
 *   - DRM planes: drm_plane, OVERLAY/CURSOR planes, zpos
 */
#include "wubu_drmx.h"
#include "wubu_hw_detect.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

/* ---- Global state ---- */
static int  g_writeback = 0;
static int  g_overlay = 0;
static int  g_hdr = 0;
static int  g_color_mgmt = 0;
static int  g_vkms = 0;
static char g_drmx_drv[24] = "";

/* ---- W1: probe the DRM-advanced topology ---- */
void wubu_drmx_probe(void)
{
    g_writeback = 0; g_overlay = 0; g_hdr = 0; g_color_mgmt = 0; g_vkms = 0;
    g_drmx_drv[0] = '\0';

#ifdef WUBU_HOSTED
    /* DRM present at all? */
    if (access("/dev/dri/card0", R_OK) == 0 ||
        access("/sys/class/drm", R_OK) == 0) {
        /* writeback connectors (drm_writeback) */
        if (access("/sys/class/drm/card0-Writeback-1", R_OK) == 0 ||
            access("/sys/class/drm/writeback", R_OK) == 0) {
            g_writeback = 1;
        }
        /* vkms (virtual modesetting, has writeback) */
        if (access("/sys/bus/platform/drivers/vkms", R_OK) == 0) {
            g_vkms = 1; g_writeback = 1;
            strcpy(g_drmx_drv, "vkms");
        }
        /* overlay/primary planes (always present on real DRM) */
        if (access("/sys/class/drm/card0", R_OK) == 0) {
            g_overlay = 1;
        }
        /* color mgmt: gamma_lut + ctm present in most drivers */
        if (access("/sys/kernel/debug/dri/0/state", R_OK) == 0) {
            g_color_mgmt = 1;
        }
        if (!g_drmx_drv[0]) strcpy(g_drmx_drv, "drm-kms");
    }
    /* HDR: drm_hdr (metadata exposed via KMS) - most modern GPUs */
    if (access("/sys/bus/pci/drivers/amdgpu", R_OK) == 0 ||
        access("/sys/bus/pci/drivers/i915", R_OK) == 0) {
        g_hdr = 1;   /* modern amdgpu/i915 support HDR metadata */
    }
#endif
}

/* ---- W2: accessors ---- */
int  wubu_drmx_writeback(void)   { return g_writeback; }
int  wubu_drmx_overlay(void)     { return g_overlay; }
int  wubu_drmx_hdr(void)         { return g_hdr; }
int  wubu_drmx_color_mgmt(void)  { return g_color_mgmt; }
int  wubu_drmx_vkms(void)        { return g_vkms; }
const char *wubu_drmx_driver(void){ return g_drmx_drv[0] ? g_drmx_drv : NULL; }

/* ---- W3: driver routing ---- */
const char *wubu_drmx_writeback_driver(const char *gpu)
{
    if (!gpu) return NULL;
    if (strstr(gpu, "vkms"))       return "vkms";
    if (strstr(gpu, "amdgpu"))     return "amdgpu-writeback";
    if (strstr(gpu, "i915"))       return "i915-writeback";
    if (strstr(gpu, "msm") || strstr(gpu, "vc4")) return "drm-writeback";
    return "drm-writeback";
}

const char *wubu_drmx_hdr_mode(const char *mode)
{
    if (!mode) return NULL;
    if (strstr(mode, "hdr10"))  return "HDR10";
    if (strstr(mode, "hlg"))    return "HLG";
    if (strstr(mode, "pq"))     return "PQ";
    if (strstr(mode, "bt2020")) return "BT.2020";
    return "SDR";
}

/* ---- W4: summary ---- */
int wubu_drmx_summary(char *out, size_t cap)
{
    return snprintf(out, cap,
        "drmx[writeback=%d overlay=%d hdr=%d color=%d vkms=%d drv=%s]",
        g_writeback, g_overlay, g_hdr, g_color_mgmt, g_vkms,
        wubu_drmx_driver() ? wubu_drmx_driver() : "none");
}
