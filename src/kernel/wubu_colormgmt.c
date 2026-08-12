/*
 * wubu_colormgmt.c -- kernel-owned display color management routing.
 *
 * Color management sets CTM/GAMMA/DEGAMMA/LUT + colorspace so colors match.
 * "Runs on everything" includes correct display color on every screen.
 *
 * Color management:
 *   - DRM CTM (color transform matrix)
 *   - GAMMA / DEGAMMA LUT (1-D lookup tables)
 *   - 3-D LUT / CSC (colorspace conversion)
 *   - /sys/class/drm card CTM, GAMMA_LUT, DEGAMMA_LUT
 *   - amdgpu: dm, color, ctm
 *   - i915: cdclk, pipemc
 *
 * WuBuOS owns this: detect color mgmt caps + LUT support, route to the
 * right driver, and expose the topology.
 *
 * Research (Kevin-Bacon 7-hop on the colormgmt frontier):
 *   - DRM CTM/GAMMA/DEGAMMA LUT
 *   - 3-LUT / CSC colorspace
 *   - amdgpu dm color, i915 cdclk
 */
#include "wubu_colormgmt.h"
#include "wubu_hw_detect.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>

/* ---- Global state ---- */
static int  g_ctm = 0;         /* color transform matrix */
static int  g_gamma = 0;       /* gamma LUT */
static int  g_degamma = 0;     /* degamma LUT */
static int  g_csc = 0;         /* colorspace conversion */
static int  g_3dlut = 0;       /* 3-D LUT */
static char g_cm_drv[24] = "";

/* ---- W1: probe the color-mgmt topology ---- */
void wubu_colormgmt_probe(void)
{
    g_ctm = 0; g_gamma = 0; g_degamma = 0; g_csc = 0; g_3dlut = 0;
    g_cm_drv[0] = '\0';

#ifdef _GNU_SOURCE
    /* DRM color mgmt? */
    if (access("/sys/class/drm", R_OK) == 0) {
        DIR *d = opendir("/sys/class/drm");
        if (d) {
            struct dirent *e;
            while ((e = readdir(d))) {
                char p[96];
                /* CTM LUT? */
                snprintf(p, sizeof(p), "/sys/class/drm/%s/CTM", e->d_name);
                if (access(p, R_OK) == 0) { g_ctm = 1; }
                /* gamma LUT? */
                snprintf(p, sizeof(p), "/sys/class/drm/%s/GAMMA_LUT", e->d_name);
                if (access(p, R_OK) == 0) { g_gamma = 1; }
                /* degamma LUT? */
                snprintf(p, sizeof(p), "/sys/class/drm/%s/DEGAMMA_LUT", e->d_name);
                if (access(p, R_OK) == 0) { g_degamma = 1; }
                /* colorspace? */
                snprintf(p, sizeof(p), "/sys/class/drm/%s/colorspace", e->d_name);
                if (access(p, R_OK) == 0) { g_csc = 1; }
                if (g_ctm && g_gamma && g_degamma) break;
            }
            closedir(d);
        }
        if (g_ctm || g_gamma || g_degamma) {
            strcpy(g_cm_drv, "drm-color");
        }
    }
    /* 3-D LUT / CSC (amdgpu/i915)? */
    if (access("/sys/module/amdgpu", R_OK) == 0 ||
        access("/sys/module/i915", R_OK) == 0) {
        g_3dlut = 1; g_csc = 1;
        if (!g_cm_drv[0]) strcpy(g_cm_drv, "amdgpu-color");
    }
#endif
}

/* ---- W2: accessors ---- */
int  wubu_colormgmt_ctm(void)    { return g_ctm; }
int  wubu_colormgmt_gamma(void)  { return g_gamma; }
int  wubu_colormgmt_degamma(void){ return g_degamma; }
int  wubu_colormgmt_csc(void)    { return g_csc; }
int  wubu_colormgmt_3dlut(void)  { return g_3dlut; }
const char *wubu_colormgmt_driver(void){ return g_cm_drv[0] ? g_cm_drv : NULL; }

/* ---- W3: color-mgmt routing ---- */
const char *wubu_colormgmt_lut_for(const char *lut)
{
    if (!lut) return NULL;
    if (strstr(lut, "degamma"))return "degamma-lut";
    if (strstr(lut, "gamma"))  return "gamma-lut";
    if (strstr(lut, "3d"))     return "3d-lut";
    if (strstr(lut, "ctm"))    return "ctm";
    return "lut";
}

const char *wubu_colormgmt_csc_for(const char *cs)
{
    if (!cs) return NULL;
    if (strstr(cs, "bt709") || strstr(cs, "bt.709")) return "bt709";
    if (strstr(cs, "bt2020") || strstr(cs, "bt.2020")) return "bt2020";
    if (strstr(cs, "rgb")) return "rgb";
    if (strstr(cs, "ycbcr")) return "ycbcr";
    return "csc";
}

/* ---- W4: summary ---- */
int wubu_colormgmt_summary(char *out, size_t cap)
{
    return snprintf(out, cap,
        "colormgmt[ctm=%d gamma=%d degamma=%d csc=%d lut3d=%d drv=%s]",
        g_ctm, g_gamma, g_degamma, g_csc, g_3dlut,
        wubu_colormgmt_driver() ? wubu_colormgmt_driver() : "none");
}
