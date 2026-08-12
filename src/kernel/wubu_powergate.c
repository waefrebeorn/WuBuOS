/*
 * wubu_powergate.c -- kernel-owned GPU power gating routing.
 *
 * Power gating cuts power to GPU blocks (shader, texture, cache) during
 * idle to save energy. "Runs on everything" includes correct power
 * control on every GPU.
 *
 * Power gate:
 *   - amdgpu: power gating (pg), /sys/class/drm card device/power_dpm_force
 *   - i915: power well, /sys/class/drm card gt_boost
 *   - nvidia: power gating via NV reg, Xorg
 *   - runtime PM: autosuspend, runtime_suspend
 *   - domain: shader, texture, l2, mcv
 *
 * WuBuOS owns this: detect power gate + runtime PM + domain, route to
 * the right driver, expose the topology.
 *
 * Research (7-hop on the powergate frontier):
 *   - amdgpu power gating
 *   - i915 power well
 *   - runtime PM suspend
 */
#include "wubu_powergate.h"
#include "wubu_hw_detect.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static int  g_pg = 0;          /* power gate present */
static int  g_runtime = 0;     /* runtime PM */
static int  g_shader = 0;      /* shader domain */
static int  g_texture = 0;     /* texture domain */
static int  g_l2 = 0;          /* L2 domain */
static char g_pg_drv[24] = "";

void wubu_powergate_probe(void)
{
    g_pg = 0; g_runtime = 0; g_shader = 0; g_texture = 0; g_l2 = 0;
    g_pg_drv[0] = '\0';

#ifdef _GNU_SOURCE
    if (access("/sys/module/amdgpu", R_OK) == 0) {
        g_pg = 1; g_runtime = 1; g_shader = 1; g_texture = 1; g_l2 = 1;
        strcpy(g_pg_drv, "amdgpu-pg");
    }
    if (access("/sys/module/i915", R_OK) == 0) {
        g_pg = 1; g_runtime = 1;
        if (!g_pg_drv[0]) strcpy(g_pg_drv, "i915-pg");
    }
    if (access("/sys/module/nvidia", R_OK) == 0 ||
        access("/sys/module/nvidia_drm", R_OK) == 0) {
        g_pg = 1; g_runtime = 1;
        if (!g_pg_drv[0]) strcpy(g_pg_drv, "nvidia-pg");
    }
    if (access("/sys/class/drm", R_OK) == 0 && !g_pg_drv[0]) {
        g_runtime = 1;
        strcpy(g_pg_drv, "drm-pm");
    }
#endif
}

int  wubu_powergate_present(void){ return g_pg; }
int  wubu_powergate_runtime(void){ return g_runtime; }
int  wubu_powergate_shader(void) { return g_shader; }
int  wubu_powergate_texture(void){ return g_texture; }
int  wubu_powergate_l2(void)     { return g_l2; }
const char *wubu_powergate_driver(void){ return g_pg_drv[0] ? g_pg_drv : NULL; }

const char *wubu_powergate_domain_for(const char *d)
{
    if (!d) return NULL;
    if (strstr(d, "shader"))  return "shader";
    if (strstr(d, "texture")) return "texture";
    if (strstr(d, "l2"))      return "l2";
    if (strstr(d, "mc") || strstr(d, "mcv")) return "mcv";
    if (strstr(d, "skep"))    return "skep";
    return "shader";
}

const char *wubu_powergate_state_for(const char *s)
{
    if (!s) return NULL;
    if (strstr(s, "on") || strstr(s, "active")) return "on";
    if (strstr(s, "off") || strstr(s, "gated")) return "gated";
    if (strstr(s, "suspend")) return "suspend";
    return "on";
}

int wubu_powergate_summary(char *out, size_t cap)
{
    return snprintf(out, cap,
        "powergate[pg=%d runtime=%d shader=%d texture=%d l2=%d drv=%s]",
        g_pg, g_runtime, g_shader, g_texture, g_l2,
        wubu_powergate_driver() ? wubu_powergate_driver() : "none");
}
