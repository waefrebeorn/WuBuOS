/*
 * wubu_gamepad.c -- kernel-owned game controller + display DSC routing.
 *
 * Two capabilities:
 *   - Game controllers: steering wheels, arcade sticks, racing pedals,
 *     generic gamepads. "Runs on everything" includes every controller.
 *   - DSC (Display Stream Compression): compresses eDP/DP 1.4 links so
 *     high-res/high-refresh panels fit. "Runs on everything" includes
 *     DSC-enabled panels.
 *
 * Game controller drivers:
 *   - xpad (Xbox USB/wireless), hid-playstation (DualSense/DualShock),
 *     hid-nintendo (Switch), hid-steam (Steam Controller),
 *     uinput (generic), input_ff (force feedback)
 *   - Wheels: G27/G29 (g29_ff), Thrustmaster (hid-tmff), Logitech wheel
 *   - Arcade: xpad (fight pads), hid-gaff (generic arcade)
 *   - Pedals: wheel combined with pedals via input devices
 *
 * DSC (Display Stream Compression):
 *   - DSC 1.2: eDP/DP 1.4, 4K@120Hz, 8K@60Hz, VESA DSC
 *   - i915/amdgpu/nouveau DSC support, drm_dsc.c
 *
 * WuBuOS owns this: detect the game controller + DSC capability, route to
 * the right driver, and expose the topology.
 *
 * Research (Kevin-Bacon 7-hop on the controller/DSC frontier):
 *   - xpad, hid-playstation, hid-nintendo, hid-steam
 *   - g29_ff, hid-tmff: racing wheels + force feedback
 *   - DSC: drm_dsc, i915 DSC, amdgpu DSC (eDP/DP 1.4)
 */
#include "wubu_gamepad.h"
#include "wubu_hw_detect.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

/* ---- Global state ---- */
static int  g_gamepad = 0;
static int  g_wheel = 0;
static int  g_arcade = 0;
static int  g_dsc = 0;
static char g_pad_drv[24] = "";
static char g_dsc_drv[24] = "";

/* ---- W1: probe the gamepad/DSC topology ---- */
void wubu_gamepad_probe(void)
{
    g_gamepad = 0; g_wheel = 0; g_arcade = 0; g_dsc = 0;
    g_pad_drv[0] = '\0'; g_dsc_drv[0] = '\0';

#ifdef _GNU_SOURCE
    /* Xbox controller (xpad) present? */
    if (access("/sys/bus/usb/drivers/xpad", R_OK) == 0) {
        g_gamepad = 1;
        strcpy(g_pad_drv, "xpad");
    }
    /* PlayStation (hid-playstation) present? */
    if (access("/sys/bus/hid/drivers/hid-playstation", R_OK) == 0) {
        g_gamepad = 1;
        if (!g_pad_drv[0]) strcpy(g_pad_drv, "hid-playstation");
    }
    /* Nintendo (hid-nintendo) present? */
    if (access("/sys/bus/hid/drivers/hid-nintendo", R_OK) == 0) {
        g_gamepad = 1;
        if (!g_pad_drv[0]) strcpy(g_pad_drv, "hid-nintendo");
    }
    /* Racing wheel (g29_ff / hid-tmff) present? */
    if (access("/sys/bus/usb/drivers/g29_ff", R_OK) == 0 ||
        access("/sys/bus/usb/drivers/hid-tmff", R_OK) == 0) {
        g_wheel = 1; g_gamepad = 1;
        if (!g_pad_drv[0]) strcpy(g_pad_drv, "g29_ff");
    }
    /* Arcade (generic) present? */
    if (access("/sys/bus/usb/drivers/hid-gaff", R_OK) == 0) {
        g_arcade = 1; g_gamepad = 1;
    }
    /* DSC: i915/amdgpu/nouveau DSC support. */
    if (access("/sys/bus/pci/drivers/i915", R_OK) == 0) {
        g_dsc = 1;
        strcpy(g_dsc_drv, "i915-dsc");
    } else if (access("/sys/bus/pci/drivers/amdgpu", R_OK) == 0) {
        g_dsc = 1;
        strcpy(g_dsc_drv, "amdgpu-dsc");
    } else if (access("/sys/bus/pci/drivers/nouveau", R_OK) == 0) {
        g_dsc = 1;
        strcpy(g_dsc_drv, "nouveau-dsc");
    }
#endif
}

/* ---- W2: accessors ---- */
int  wubu_gamepad_present(void) { return g_gamepad; }
int  wubu_gamepad_wheel(void)   { return g_wheel; }
int  wubu_gamepad_arcade(void)  { return g_arcade; }
int  wubu_gamepad_dsc(void)     { return g_dsc; }
const char *wubu_gamepad_driver(void){ return g_pad_drv[0] ? g_pad_drv : NULL; }
const char *wubu_gamepad_dsc_driver(void){ return g_dsc_drv[0] ? g_dsc_drv : NULL; }

/* ---- W3: routing ---- */
const char *wubu_gamepad_controller_for(const char *dev)
{
    if (!dev) return NULL;
    if (strstr(dev, "xbox") || strstr(dev, "xpad")) return "xpad";
    if (strstr(dev, "playstation") || strstr(dev, "dual")) return "hid-playstation";
    if (strstr(dev, "nintendo") || strstr(dev, "switch")) return "hid-nintendo";
    if (strstr(dev, "steam"))   return "hid-steam";
    if (strstr(dev, "g29") || strstr(dev, "g27") || strstr(dev, "logitech")) return "g29_ff";
    if (strstr(dev, "thrustmaster")) return "hid-tmff";
    return "uinput";
}

const char *wubu_gamepad_dsc_for(const char *gpu)
{
    if (!gpu) return NULL;
    if (strstr(gpu, "i915") || strstr(gpu, "intel")) return "i915-dsc";
    if (strstr(gpu, "amdgpu") || strstr(gpu, "amd")) return "amdgpu-dsc";
    if (strstr(gpu, "nouveau") || strstr(gpu, "nvidia")) return "nouveau-dsc";
    return "drm-dsc";
}

/* ---- W4: summary ---- */
int wubu_gamepad_summary(char *out, size_t cap)
{
    return snprintf(out, cap,
        "gamepad[pad=%d wheel=%d arcade=%d(%s) dsc=%d(%s)]",
        g_gamepad, g_wheel, g_arcade,
        wubu_gamepad_driver() ? wubu_gamepad_driver() : "none",
        g_dsc, wubu_gamepad_dsc_driver() ? wubu_gamepad_dsc_driver() : "none");
}
