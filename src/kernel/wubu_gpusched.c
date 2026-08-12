/*
 * wubu_gpusched.c -- kernel-owned GPU compute scheduler routing.
 *
 * The GPU scheduler manages command submission priority + fairness between
 * contexts (3D, compute, video). "Runs on everything" includes correct
 * GPU scheduling on every GPU.
 *
 * GPU scheduler:
 *   - DRM scheduler (drm-sched): job submission + priority
 *   - i915 GuC: firmware command submission + preemption
 *   - amdgpu: GPU scheduler + CP (command processor)
 *   - nvkm: NVIDIA scheduler (prio bands: high/normal/low)
 *   - priority: high > normal > low; compute vs. 3D fairness
 *   - /sys/class/drm card device/ip/ (power + scheduler)
 *
 * WuBuOS owns this: detect the GPU scheduler + priority bands, route to the
 * right driver, and expose the topology.
 *
 * Research (Kevin-Bacon 7-hop on the GPU-sched frontier):
 *   - drm-sched: job submission + priority
 *   - i915 GuC: firmware submission + preemption
 *   - amdgpu scheduler + CP
 *   - nvkm priority bands (high/normal/low)
 */
#include "wubu_gpusched.h"
#include "wubu_hw_detect.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

/* ---- Global state ---- */
static int  g_sched = 0;       /* GPU scheduler */
static int  g_guc = 0;         /* i915 GuC */
static int  g_prio = 0;        /* priority bands */
static int  g_preempt = 0;     /* preemption */
static int  g_fair = 0;        /* fairness */
static char g_gpusched_drv[24] = "";

/* ---- W1: probe the GPU-sched topology ---- */
void wubu_gpusched_probe(void)
{
    g_sched = 0; g_guc = 0; g_prio = 0; g_preempt = 0; g_fair = 0;
    g_gpusched_drv[0] = '\0';

#ifdef _GNU_SOURCE
    /* i915? */
    if (access("/sys/module/i915", R_OK) == 0) {
        g_sched = 1; g_guc = 1; g_prio = 1; g_preempt = 1;
        strcpy(g_gpusched_drv, "i915-guc");
    }
    /* amdgpu? */
    if (access("/sys/module/amdgpu", R_OK) == 0 && !g_sched) {
        g_sched = 1; g_prio = 1;
        strcpy(g_gpusched_drv, "amdgpu-sched");
    }
    /* nvkm? */
    if (access("/sys/module/nouveau", R_OK) == 0 && !g_sched) {
        g_sched = 1; g_prio = 1;
        strcpy(g_gpusched_drv, "nvkm-sched");
    }
    if (g_sched) g_fair = 1;
#endif
}

/* ---- W2: accessors ---- */
int  wubu_gpusched_present(void){ return g_sched; }
int  wubu_gpusched_guc(void)  { return g_guc; }
int  wubu_gpusched_prio(void) { return g_prio; }
int  wubu_gpusched_preempt(void){ return g_preempt; }
int  wubu_gpusched_fair(void){ return g_fair; }
const char *wubu_gpusched_driver(void){ return g_gpusched_drv[0] ? g_gpusched_drv : NULL; }

/* ---- W3: GPU-sched routing ---- */
const char *wubu_gpusched_prio_for(const char *prio)
{
    if (!prio) return NULL;
    if (strstr(prio, "high"))   return "high";
    if (strstr(prio, "normal")) return "normal";
    if (strstr(prio, "low"))    return "low";
    return "normal";
}

const char *wubu_gpusched_class_for(const char *cls)
{
    if (!cls) return NULL;
    if (strstr(cls, "3d"))     return "3d";
    if (strstr(cls, "compute")) return "compute";
    if (strstr(cls, "video"))  return "video";
    if (strstr(cls, "copy"))   return "copy";
    return "3d";
}

/* ---- W4: summary ---- */
int wubu_gpusched_summary(char *out, size_t cap)
{
    return snprintf(out, cap,
        "gpusched[sched=%d guc=%d prio=%d preempt=%d fair=%d drv=%s]",
        g_sched, g_guc, g_prio, g_preempt, g_fair,
        wubu_gpusched_driver() ? wubu_gpusched_driver() : "none");
}
