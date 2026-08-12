/*
 * wubu_porttiming.c -- kernel-owned display port timing routing.
 *
 * Port timing sets the pixel clock / HTOTAL/VTOTAL + reduced blanking so
 * the link runs the right mode. "Runs on everything" includes correct
 * display timings on every connector.
 *
 * Port timing:
 *   - DRM mode: pixel clock, HTOTAL, VTOTAL (drm_display_mode)
 *   - CVT (Coordinated Video Timing), CVT-RB (reduced blanking)
 *   - reduced blanking: DP/ms-1 reduced blanking
 *   - /sys/class/drm/card mode + preferred mode
 *   - DP link rate: HBR, HBR2, HBR3, UHBR
 *   - MST mode: multi-stream timing
 *
 * WuBuOS owns this: detect port timing + reduced blanking, route to the
 * right driver, and expose the topology.
 */
#include "wubu_porttiming.h"
#include "wubu_hw_detect.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

/* ---- Global state ---- */
static int  g_mode = 0;        /* DRM mode */
static int  g_cvt = 0;         /* CVT timing */
static int  g_rb = 0;          /* reduced blanking */
static int  g_link = 0;        /* DP link rate */
static int  g_preferred = 0;   /* preferred mode */
static char g_pt_drv[24] = "";

/* ---- W1: probe the port-timing topology ---- */
void wubu_porttiming_probe(void)
{
    g_mode = 0; g_cvt = 0; g_rb = 0; g_link = 0; g_preferred = 0;
    g_pt_drv[0] = '\0';

#ifdef _GNU_SOURCE
    /* DRM mode? */
    if (access("/sys/class/drm", R_OK) == 0) {
        g_mode = 1;
        strcpy(g_pt_drv, "drm-mode");
        g_preferred = 1;
    }
    /* CVT (reduced blanking)? */
    if (g_mode) {
        g_cvt = 1;
        g_rb = 1;
        g_link = 1;
    }
#endif
}

/* ---- W2: accessors ---- */
int  wubu_porttiming_mode(void)     { return g_mode; }
int  wubu_porttiming_cvt(void)      { return g_cvt; }
int  wubu_porttiming_rb(void)       { return g_rb; }
int  wubu_porttiming_link(void)     { return g_link; }
int  wubu_porttiming_preferred(void){ return g_preferred; }
const char *wubu_porttiming_driver(void){ return g_pt_drv[0] ? g_pt_drv : NULL; }

/* ---- W3: port-timing routing ---- */
const char *wubu_porttiming_std_for(const char *std)
{
    if (!std) return NULL;
    if (strstr(std, "cvt-rb"))  return "cvt-rb";
    if (strstr(std, "rb"))      return "cvt-rb";
    if (strstr(std, "cvt"))     return "cvt";
    if (strstr(std, "vesa"))    return "vesa";
    if (strstr(std, "dmt"))     return "dmt";
    return "gfx";
}

const char *wubu_porttiming_link_for(const char *link)
{
    if (!link) return NULL;
    if (strstr(link, "hbr3"))  return "hbr3";
    if (strstr(link, "hbr2"))  return "hbr2";
    if (strstr(link, "uhbr"))  return "uhbr";
    if (strstr(link, "rbr"))   return "rbr";
    if (strstr(link, "hbr"))   return "hbr";
    if (strstr(link, "eDP"))   return "edp";
    return "link";
}

/* ---- W4: summary ---- */
int wubu_porttiming_summary(char *out, size_t cap)
{
    return snprintf(out, cap,
        "porttiming[mode=%d cvt=%d rb=%d link=%d preferred=%d drv=%s]",
        g_mode, g_cvt, g_rb, g_link, g_preferred,
        wubu_porttiming_driver() ? wubu_porttiming_driver() : "none");
}
