/*
 * wubu_gpurst.c -- kernel-owned GPU reset + recovery routing.
 *
 * GPU reset recovers from a hung GPU by resetting the device after a
 * timeout, then re-initializing the ring. "Runs on everything"
 * includes correct GPU recovery on every GPU.
 *
 * GPU reset/recovery:
 *   - DRM: drm_sched_resubmit_jobs, drm_sched_fault
 *   - amdgpu: amdgpu_device_gpu_recover, AMD_RESET_MAGIC
 *   - i915: i915_reset_device, GEM reset
 *   - nouveau: nvkm_device_ctor, recovery
 *   - /sys/class/drm card reboot: GPU reset
 *   - ring hang: hws, ring test, heartbeat
 *
 * WuBuOS owns this: detect GPU reset + ring + heartbeat, route to the
 * right driver, expose the topology.
 *
 * Research (7-hop on the gpurst frontier):
 *   - DRM scheduler recovery
 *   - amdgpu device gpu_recover
 *   - i915 reset_device
 *   - ring hang, heartbeat, job timeout
 */
#include "wubu_gpurst.h"
#include "wubu_hw_detect.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static int  g_rst = 0;         /* GPU reset present */
static int  g_ring = 0;        /* ring test */
static int  g_hb = 0;          /* heartbeat */
static int  g_timeout = 0;     /* job timeout */
static int  g_recover = 0;     /* recovery */
static char g_rst_drv[24] = "";

void wubu_gpurst_probe(void)
{
    g_rst = 0; g_ring = 0; g_hb = 0; g_timeout = 0; g_recover = 0;
    g_rst_drv[0] = '\0';

#ifdef WUBU_HOSTED
    if (access("/sys/module/amdgpu", R_OK) == 0) {
        g_rst = 1; g_ring = 1; g_hb = 1; g_timeout = 1; g_recover = 1;
        strcpy(g_rst_drv, "amdgpu-reset");
    }
    if (access("/sys/module/i915", R_OK) == 0) {
        g_rst = 1; g_ring = 1; g_timeout = 1; g_recover = 1;
        if (!g_rst_drv[0]) strcpy(g_rst_drv, "i915-reset");
    }
    if (access("/sys/module/nouveau", R_OK) == 0) {
        if (!g_rst_drv[0]) strcpy(g_rst_drv, "nouveau-reset");
    }
    if (access("/sys/class/drm", R_OK) == 0 && !g_rst_drv[0]) {
        g_recover = 1;
        strcpy(g_rst_drv, "drm-sched-recover");
    }
#endif
}

int  wubu_gpurst_present(void){ return g_rst; }
int  wubu_gpurst_ring(void)    { return g_ring; }
int  wubu_gpurst_hb(void)      { return g_hb; }
int  wubu_gpurst_timeout(void) { return g_timeout; }
int  wubu_gpurst_recover(void) { return g_recover; }
const char *wubu_gpurst_driver(void){ return g_rst_drv[0] ? g_rst_drv : NULL; }

const char *wubu_gpurst_stage_for(const char *s)
{
    if (!s) return NULL;
    if (strstr(s, "pre") || strstr(s, "stop"))   return "pre-reset";
    if (strstr(s, "reset")) return "reset";
    if (strstr(s, "post") || strstr(s, "resume")) return "post-reset";
    if (strstr(s, "fault")) return "fault";
    return "idle";
}

const char *wubu_gpurst_ring_for(const char *r)
{
    if (!r) return NULL;
    if (strstr(r, "gfx") || strstr(r, "3d")) return "gfx";
    if (strstr(r, "compute")) return "compute";
    if (strstr(r, "dma"))   return "dma";
    if (strstr(r, "video")) return "video";
    return "gfx";
}

int wubu_gpurst_summary(char *out, size_t cap)
{
    return snprintf(out, cap,
        "gpurst[reset=%d ring=%d hb=%d timeout=%d recover=%d drv=%s]",
        g_rst, g_ring, g_hb, g_timeout, g_recover,
        wubu_gpurst_driver() ? wubu_gpurst_driver() : "none");
}