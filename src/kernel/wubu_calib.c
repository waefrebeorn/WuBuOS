/*
 * wubu_calib.c -- kernel-owned display brightness/gamma calibration routing.
 *
 * Calibration adjusts the display's brightness curve + gamma + color LUT
 * so what you see matches intent. "Runs on everything" includes correct
 * display calibration.
 *
 * Calibration:
 *   - drm_color_mgmt: DRM color management (CTM, gamma LUT, degamma)
 *   - gamma/brightness/contrast: software adjustment (xgamma, ddcutil)
 *   - backlight curve: /sys/class/backlight brightness
 *   - icc profile: color calibration profile (colord)
 *   - DDC/CI: monitor OSD control (ddcutil, i2c)
 *
 * WuBuOS owns this: detect the calibration capability (DRM color mgmt,
 * DDC/CI, gamma), route to the right driver, and expose the topology.
 *
 * Research (Kevin-Bacon 7-hop on the calibration frontier):
 *   - drm_color_mgmt: CRTC gamma/degamma/CTM LUTs
 *   - ddcutil: DDC/CI monitor control (brightness via i2c)
 *   - gamma: xgamma/icc, colord color profiles
 *   - backlight: sysfs brightness curve
 */
#include "wubu_calib.h"
#include "wubu_hw_detect.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

/* ---- Global state ---- */
static int  g_drm_color = 0;   /* DRM color management */
static int  g_ddc = 0;         /* DDC/CI monitor control */
static int  g_gamma = 0;       /* gamma/LUT */
static int  g_colord = 0;      /* color profile daemon */
static int  g_icc = 0;         /* ICC profile */
static char g_calib_drv[24] = "";

/* ---- W1: probe the calibration topology ---- */
void wubu_calib_probe(void)
{
    g_drm_color = 0; g_ddc = 0; g_gamma = 0; g_colord = 0; g_icc = 0;
    g_calib_drv[0] = '\0';

#ifdef _GNU_SOURCE
    /* DRM color management (gamma LUT)? */
    if (access("/sys/kernel/debug/dri", R_OK) == 0 ||
        access("/sys/module/drm_kms_helper", R_OK) == 0) {
        g_drm_color = 1;
        strcpy(g_calib_drv, "drm-color");
    }
    /* DDC/CI monitor control (i2c)? */
    if (access("/usr/bin/ddcutil", R_OK) == 0 ||
        access("/usr/lib/udev", R_OK) == 0) {
        g_ddc = 1;
        if (!g_calib_drv[0]) strcpy(g_calib_drv, "ddc-ci");
    }
    /* Gamma (xgamma / colord)? */
    if (access("/usr/bin/xgamma", R_OK) == 0 ||
        access("/usr/libexec/colord", R_OK) == 0 ||
        access("/usr/lib/colord", R_OK) == 0) {
        g_gamma = 1;
        if (!g_calib_drv[0]) strcpy(g_calib_drv, "gamma");
    }
    /* colord daemon (ICC profiles)? */
    if (access("/usr/libexec/colord", R_OK) == 0 ||
        access("/usr/lib/colord", R_OK) == 0 ||
        access("/run/colord", R_OK) == 0) {
        g_colord = 1;
    }
    /* ICC profile present? */
    if (access("/usr/share/color/icc", R_OK) == 0) {
        g_icc = 1;
    }
#endif
}

/* ---- W2: accessors ---- */
int  wubu_calib_drm_color(void){ return g_drm_color; }
int  wubu_calib_ddc(void)      { return g_ddc; }
int  wubu_calib_gamma(void)    { return g_gamma; }
int  wubu_calib_colord(void)   { return g_colord; }
int  wubu_calib_icc(void)      { return g_icc; }
const char *wubu_calib_driver(void){ return g_calib_drv[0] ? g_calib_drv : NULL; }

/* ---- W3: calibration driver routing ---- */
const char *wubu_calib_driver_for(const char *calib)
{
    if (!calib) return NULL;
    if (strstr(calib, "drm") || strstr(calib, "lut")) return "drm-color";
    if (strstr(calib, "ddc") || strstr(calib, "ci"))  return "ddc-ci";
    if (strstr(calib, "gamma")) return "gamma-lut";
    if (strstr(calib, "icc"))   return "icc-profile";
    if (strstr(calib, "colord"))return "colord";
    return "calib-core";
}

/* ---- W4: summary ---- */
int wubu_calib_summary(char *out, size_t cap)
{
    return snprintf(out, cap,
        "calib[drm=%d ddc=%d gamma=%d colord=%d icc=%d drv=%s]",
        g_drm_color, g_ddc, g_gamma, g_colord, g_icc,
        wubu_calib_driver() ? wubu_calib_driver() : "none");
}
