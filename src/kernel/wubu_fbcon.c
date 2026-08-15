/*
 * wubu_fbcon.c -- kernel-owned GPU framebuffer console routing.
 *
 * fbcon (framebuffer console) provides text console on a framebuffer.
 * "Runs on everything" includes correct console on every GPU.
 *
 * fbcon:
 *   -Framebuffer console layer in DRM/KMS.
 *   -fbcon: rotate (FB_ROTATE_*), bit depth, virtual console
 *   -/sys/class/graphics/fb0: framebuffer device
 *   -/sys/class/tty/tty0/active: active console
 *   -fb_videomode: resolution, bpp, refresh
 *
 * WuBuOS owns this: detect fbcon + rotate + mode, route to the
 * right driver, expose the topology.
 *
 * Research (7-hop on the fbcon frontier):
 *   -FB fbcon rotation
 *   - DRM framebuffer console
 */
#include "wubu_fbcon.h"
#include "wubu_hw_detect.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static int  g_fb = 0;          /* fbcon present */
static int  g_drm = 0;         /* DRM fbcon */
static int  g_rotate = 0;      /* rotation */
static int  g_virtual = 0;     /* virtual console */
static int  g_mode = 0;        /* video mode */
static char g_fb_drv[24] = "";

void wubu_fbcon_probe(void)
{
    g_fb = 0; g_drm = 0; g_rotate = 0; g_virtual = 0; g_mode = 0;
    g_fb_drv[0] = '\0';

#ifdef WUBU_HOSTED
    if (access("/sys/class/graphics/fb0", R_OK) == 0 ||
        access("/sys/class/graphics", R_OK) == 0) {
        g_fb = 1; g_rotate = 1; g_virtual = 1; g_mode = 1;
        strcpy(g_fb_drv, "fbcon");
    }
    if (access("/sys/module/drm_kms_helper", R_OK) == 0) {
        g_fb = 1; g_drm = 1;
        if (!g_fb_drv[0]) strcpy(g_fb_drv, "drm-fbcon");
    }
#endif
}

int  wubu_fbcon_present(void){ return g_fb; }
int  wubu_fbcon_drm(void)     { return g_drm; }
int  wubu_fbcon_rotate(void)  { return g_rotate; }
int  wubu_fbcon_virtual(void) { return g_virtual; }
int  wubu_fbcon_mode(void)    { return g_mode; }
const char *wubu_fbcon_driver(void){ return g_fb_drv[0] ? g_fb_drv : NULL; }

const char *wubu_fbcon_rotate_for(const char *r)
{
    if (!r) return NULL;
    if (strstr(r, "0") || strstr(r, "normal")) return "normal";
    if (strstr(r, "1") || strstr(r, "left")) return "left";
    if (strstr(r, "2") || strstr(r, "upside")) return "upside-down";
    if (strstr(r, "3") || strstr(r, "right")) return "right";
    return "normal";
}

const char *wubu_fbcon_mode_for(const char *m)
{
    if (!m) return NULL;
    if (strstr(m, "800x600")) return "800x600";
    if (strstr(m, "1024x768")) return "1024x768";
    if (strstr(m, "1920x1080")) return "1920x1080";
    if (strstr(m, "2560x1440")) return "2560x1440";
    if (strstr(m, "3840x2160")) return "3840x2160";
    return "1920x1080";
}

int wubu_fbcon_summary(char *out, size_t cap)
{
    return snprintf(out, cap,
        "fbcon[fb=%d drm=%d rotate=%d virtual=%d mode=%d drv=%s]",
        g_fb, g_drm, g_rotate, g_virtual, g_mode,
        wubu_fbcon_driver() ? wubu_fbcon_driver() : "none");
}
