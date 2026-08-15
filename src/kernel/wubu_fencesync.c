/*
 * wubu_fencesync.c -- kernel-owned GPU sync/fence routing.
 *
 * Sync fences (DMA fence, timeline) coordinate GPU-CPU. "Runs on
 * everything" includes correct GPU sync on every device.
 *
 * Fence sync:
 *   - DMA fence: dma_fence, fence signaling
 *   - timeline: fence timeline, seqno
 *   - wait: dma_fence_wait, timeout
 *   - /sys/class/drm/card*: sync fd, timeline
 *   - amdgpu: fence driver, SDMA
 *   - i915: timeline, seqno
 *
 * WuBuOS owns this: detect fence + timeline + wait, route to the
 * right driver, expose the topology.
 *
 * Research (7-hop on the fencesync frontier):
 *   - DMA fence synchronization
 *   - GPU fence wait timeout
 */
#include "wubu_fencesync.h"
#include "wubu_hw_detect.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static int  g_fence = 0;       /* fence present */
static int  g_timeline = 0;    /* timeline */
static int  g_wait = 0;        /* fence wait */
static int  g_timeout = 0;     /* fence timeout */
static int  g_signal = 0;      /* fence signal */
static char g_fence_drv[24] = "";

void wubu_fencesync_probe(void)
{
    g_fence = 0; g_timeline = 0; g_wait = 0; g_timeout = 0; g_signal = 0;
    g_fence_drv[0] = '\0';

#ifdef WUBU_HOSTED
    if (access("/sys/module/amdgpu", R_OK) == 0) {
        g_fence = 1; g_timeline = 1; g_wait = 1; g_timeout = 1; g_signal = 1;
        strcpy(g_fence_drv, "amdgpufence");
    }
    if (access("/sys/module/i915", R_OK) == 0) {
        g_fence = 1; g_timeline = 1; g_wait = 1; g_signal = 1;
        if (!g_fence_drv[0]) strcpy(g_fence_drv, "i915fence");
    }
    if (access("/sys/class/drm/card0/device/sync", R_OK) == 0 ||
        access("/sys/module/drm_vblank", R_OK) == 0) {
        g_fence = 1; g_signal = 1;
        if (!g_fence_drv[0]) strcpy(g_fence_drv, "drm-sync");
    }
#endif
}

int  wubu_fencesync_present(void){ return g_fence; }
int  wubu_fencesync_timeline(void){ return g_timeline; }
int  wubu_fencesync_wait(void)   { return g_wait; }
int  wubu_fencesync_timeout(void){ return g_timeout; }
int  wubu_fencesync_signal(void) { return g_signal; }
const char *wubu_fencesync_driver(void){ return g_fence_drv[0] ? g_fence_drv : NULL; }

const char *wubu_fencesync_op_for(const char *o)
{
    if (!o) return NULL;
    if (strstr(o, "wait")) return "wait";
    if (strstr(o, "signal")) return "signal";
    if (strstr(o, "timeline")) return "timeline";
    if (strstr(o, "timeout")) return "timeout";
    return "wait";
}

const char *wubu_fencesync_type_for(const char *t)
{
    if (!t) return NULL;
    if (strstr(t, "sdma")) return "sdma-fence";
    if (strstr(t, "seqno")) return "seqno";
    if (strstr(t, "sync") || strstr(t, "fd")) return "sync-fd";
    if (strstr(t, "dma")) return "dma-fence";
    return "dma-fence";
}

int wubu_fencesync_summary(char *out, size_t cap)
{
    return snprintf(out, cap,
        "fencesync[fence=%d timeline=%d wait=%d timeout=%d signal=%d drv=%s]",
        g_fence, g_timeline, g_wait, g_timeout, g_signal,
        wubu_fencesync_driver() ? wubu_fencesync_driver() : "none");
}
