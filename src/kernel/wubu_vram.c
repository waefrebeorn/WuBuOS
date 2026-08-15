/*
 * wubu_vram.c -- kernel-owned GPU VRAM + framebuffer memory routing.
 *
 * VRAM is dedicated graphics memory. Framebuffer memory holds the scanout
 * buffer. "Runs on everything" includes correct GPU memory on every gpu.
 *
 * VRAM:
 *   - DRM: drm_mm (DR M memory manager), VRAM allocation (TTM, amdgpu)
 *   - /sys/class/drm card mem_info_vram
 *   - TTM: ttm_bo (bo = buffer object), GPU memory pool
 *   - framebuffer: fb_fix, fb_var (fbmem, scanout buffer)
 *   - stolen: Intel stolen memory (for integrated GPU)
 *   - VRAM staging: CPU to GPU bounce buffer
 *
 * WuBuOS owns this: detect VRAM type + framebuffer + size + stolen,
 * route to the right driver, and expose the topology.
 *
 * Research (Kevin-Bacon 7-hop on the VRAM frontier):
 *   - drm_mm memory manager
 *   - TTM buffer objects (ttm_bo)
 *   - /sys/class/drm mem_info_vram
 *   - Intel stolen memory
 *   - framebuffer (fbmem)
 */
#include "wubu_vram.h"
#include "wubu_hw_detect.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>

/* ---- Global state ---- */
static int  g_vram = 0;        /* VRAM present */
static int  g_fb = 0;          /* framebuffer */
static int  g_stolen = 0;      /* Intel stolen */
static int  g_ttm = 0;         /* TTM pool */
static int  g_drm_mm = 0;      /* drm_mm */
static char g_vram_drv[24] = "";

/* ---- W1: probe the VRAM topology ---- */
void wubu_vram_probe(void)
{
    g_vram = 0; g_fb = 0; g_stolen = 0; g_ttm = 0; g_drm_mm = 0;
    g_vram_drv[0] = '\0';

#ifdef WUBU_HOSTED
    /* DRM card present? */
    if (access("/sys/class/drm", R_OK) == 0) {
        g_vram = 1; g_drm_mm = 1;
        strcpy(g_vram_drv, "drm-vram");
        /* framebuffer? */
        if (access("/dev/fb0", R_OK) == 0) {
            g_fb = 1;
        }
        /* TTM? */
        if (access("/sys/module/ttm", R_OK) == 0) {
            g_ttm = 1;
        }
        /* stolen? */
        if (access("/sys/class/drm/card0/device/stolen", R_OK) == 0 ||
            access("/sys/class/drm/card0/device/stolen_fp", R_OK) == 0) {
            g_stolen = 1;
        }
    }
    /* /dev/dgx (WSL GPU paravirt)? */
    if (access("/dev/dgx", R_OK) == 0) {
        g_vram = 1; g_fb = 1;
        if (!g_vram_drv[0]) strcpy(g_vram_drv, "dxg-vram");
    }
#endif
}

/* ---- W2: accessors ---- */
int  wubu_vram_present(void){ return g_vram; }
int  wubu_vram_fb(void)      { return g_fb; }
int  wubu_vram_stolen(void)  { return g_stolen; }
int  wubu_vram_ttm(void)     { return g_ttm; }
int  wubu_vram_drm_mm(void)  { return g_drm_mm; }
const char *wubu_vram_driver(void){ return g_vram_drv[0] ? g_vram_drv : NULL; }

/* ---- W3: VRAM routing ---- */
const char *wubu_vram_pool_for(const char *pool)
{
    if (!pool) return NULL;
    if (strstr(pool, "stolen"))  return "stolen";
    if (strstr(pool, "ttm"))     return "ttm";
    if (strstr(pool, "vram"))    return "vram";
    if (strstr(pool, "fb"))      return "framebuffer";
    return "vram";
}

const char *wubu_vram_alloc_for(const char *hint)
{
    if (!hint) return NULL;
    if (strstr(hint, "gpu"))    return "gpu-domain";
    if (strstr(hint, "cpu"))    return "cpu-domain";
    if (strstr(hint, "wc"))     return "write-combining";
    if (strstr(hint, "uc"))     return "uncached";
    if (strstr(hint, "wb"))     return "write-back";
    return "vram";
}

/* ---- W4: summary ---- */
int wubu_vram_summary(char *out, size_t cap)
{
    return snprintf(out, cap,
        "vram[vram=%d fb=%d stolen=%d ttm=%d drm_mm=%d drv=%s]",
        g_vram, g_fb, g_stolen, g_ttm, g_drm_mm,
        wubu_vram_driver() ? wubu_vram_driver() : "none");
}