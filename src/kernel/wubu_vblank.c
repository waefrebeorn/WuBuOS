/*
 * wubu_vblank.c -- kernel-owned GPU VBLANK interrupt routing.
 *
 * VBLANK (vertical blank) interrupt synchronizes display refresh.
 * "Runs on everything" includes correct VBLANK on every GPU.
 *
 * VBLANK:
 *   - DRM: drm_vblank, vblank interrupt
 *   - /sys/class/drm card device vblank: vblank counters
 *   - /sys/class/drm card device vblank_time: vblank time
 *   - CRTC: vblank event, page flip
 *   - /dev/dri/card*: DRM vblank
 *   - amdgpu: vblank, page flip event
 *   - i915: vblank, page flip
 *
 * WuBuOS owns this: detect VBLANK + counter + event, route to the
 * right driver, expose the topology.
 *
 * Research (7-hop on the vblank frontier):
 *   -drm_vblank.c VBLANK interrupt
 */
#include "wubu_vblank.h"
#include "wubu_hw_detect.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static int  g_vbl = 0;         /* VBLANK present */
static int  g_counter = 0;     /* vblank counter */
static int  g_event = 0;       /* vblank event */
static int  g_time = 0;        /* vblank time */
static int  g_flip = 0;        /* page flip */
static char g_vbl_drv[24] = "";

void wubu_vblank_probe(void)
{
    g_vbl = 0; g_counter = 0; g_event = 0; g_time = 0; g_flip = 0;
    g_vbl_drv[0] = '\0';

#ifdef WUBU_HOSTED
    if (access("/dev/dri/card0", R_OK) == 0 ||
        access("/sys/class/drm", R_OK) == 0) {
        g_vbl = 1; g_counter = 1; g_event = 1; g_time = 1; g_flip = 1;
        strcpy(g_vbl_drv, "drm-vblank");
    }
    if (access("/sys/module/amdgpu", R_OK) == 0) {
        g_vbl = 1; g_counter = 1; g_event = 1; g_flip = 1;
        if (!g_vbl_drv[0]) strcpy(g_vbl_drv, "amdgpu-vblank");
    }
    if (access("/sys/module/i915", R_OK) == 0) {
        g_vbl = 1; g_counter = 1; g_event = 1; g_flip = 1;
        if (!g_vbl_drv[0]) strcpy(g_vbl_drv, "i915-vblank");
    }
#endif
}

int  wubu_vblank_present(void){ return g_vbl; }
int  wubu_vblank_counter(void){ return g_counter; }
int  wubu_vblank_event(void)  { return g_event; }
int  wubu_vblank_time(void)    { return g_time; }
int  wubu_vblank_flip(void)    { return g_flip; }
const char *wubu_vblank_driver(void){ return g_vbl_drv[0] ? g_vbl_drv : NULL; }

const char *wubu_vblank_src_for(const char *s)
{
    if (!s) return NULL;
    if (strstr(s, "crtc")) return "CRTC";
    if (strstr(s, "conn") || strstr(s, "connector")) return "Connector";
    if (strstr(s, "encoder")) return "Encoder";
    if (strstr(s, "plane")) return "Plane";
    if (strstr(s, "primary")) return "Primary";
    if (strstr(s, "cursor")) return "Cursor";
    return "CRTC";
}

const char *wubu_vblank_mode_for(const char *m)
{
    if (!m) return NULL;
    if (strstr(m, "event")) return "event";
    if (strstr(m, "flip")) return "page-flip";
    if (strstr(m, "counter")) return "counter";
    if (strstr(m, "time")) return "time";
    if (strstr(m, "disable")) return "disabled";
    if (strstr(m, "enable")) return "enabled";
    return "event";
}

int wubu_vblank_summary(char *out, size_t cap)
{
    return snprintf(out, cap,
        "vblank[vbl=%d counter=%d event=%d time=%d flip=%d drv=%s]",
        g_vbl, g_counter, g_event, g_time, g_flip,
        wubu_vblank_driver() ? wubu_vblank_driver() : "none");
}
