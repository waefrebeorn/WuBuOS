/*
 * wubu_touch.c -- kernel-owned touchscreen/trackpad driver routing.
 *
 * Touch is how laptops/tablets/all-in-ones take input. "Runs on
 * everything" includes every touchscreen + trackpad.
 *
 * Touch/trackpad drivers:
 *   - Synaptics: rmi4 (RMI4 I2C), synaptics_i2c, hid-rmi, psmouse synaptics
 *   - Elan: elan_i2c (the most common trackpad/touchscreen)
 *   - ALPS: alps (psmouse), hid-alps
 *   - Wacom: wacom (tablets), hid-wacom
 *   - Goodix: goodix_ts (touchscreen), i2c-hid goodix
 *   - Multitouch: hid-multitouch (universal HID MT), hid-mt
 *   - Cypress: cypress-sf (Surface)
 *   - Silead: silead (generic i2c touchscreen)
 *
 * WuBuOS owns this: detect the touch/trackpad (HID/i2c), route to the
 * right driver, and expose the touch topology.
 *
 * Research (Kevin-Bacon 7-hop on the touch frontier):
 *   - elan_i2c: Elan touchpads (most common on Windows laptops)
 *   - rmi4: Synaptics RMI4 (I2C), the other big trackpad family
 *   - hid-multitouch: universal HID multitouch (Windows precision)
 *   - wacom: tablets, goodix_ts: touchscreens
 *   - alps: ALPS trackpads (psmouse)
 */
#include "wubu_touch.h"
#include "wubu_hw_detect.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

/* ---- Global state ---- */
static int  g_touch = 0;
static int  g_elan = 0;
static int  g_synaptics = 0;
static int  g_multitouch = 0;
static int  g_wacom = 0;
static char g_touch_drv[24] = "";

/* ---- W1: probe the touch topology ---- */
void wubu_touch_probe(void)
{
    g_touch = 0; g_elan = 0; g_synaptics = 0; g_multitouch = 0; g_wacom = 0;
    g_touch_drv[0] = '\0';

#ifdef _GNU_SOURCE
    /* Elan trackpad/touchscreen present? */
    if (access("/sys/bus/i2c/drivers/elan_i2c", R_OK) == 0) {
        g_elan = 1; g_touch = 1;
        strcpy(g_touch_drv, "elan_i2c");
    }
    /* Synaptics RMI4 present? */
    if (access("/sys/bus/i2c/drivers/rmi4", R_OK) == 0 ||
        access("/sys/bus/hid/drivers/hid-rmi", R_OK) == 0) {
        g_synaptics = 1; g_touch = 1;
        if (!g_touch_drv[0]) strcpy(g_touch_drv, "rmi4");
    }
    /* hid-multitouch present? */
    if (access("/sys/bus/hid/drivers/hid-multitouch", R_OK) == 0) {
        g_multitouch = 1; g_touch = 1;
        if (!g_touch_drv[0]) strcpy(g_touch_drv, "hid-multitouch");
    }
    /* Wacom present? */
    if (access("/sys/bus/hid/drivers/wacom", R_OK) == 0 ||
        access("/sys/bus/usb/drivers/wacom", R_OK) == 0) {
        g_wacom = 1; g_touch = 1;
    }
    /* Goodix touchscreen? */
    if (access("/sys/bus/i2c/drivers/goodix_ts", R_OK) == 0) {
        g_touch = 1;
        if (!g_touch_drv[0]) strcpy(g_touch_drv, "goodix_ts");
    }
#endif
}

/* ---- W2: accessors ---- */
int  wubu_touch_present(void)   { return g_touch; }
int  wubu_touch_elan(void)      { return g_elan; }
int  wubu_touch_synaptics(void) { return g_synaptics; }
int  wubu_touch_multitouch(void){ return g_multitouch; }
int  wubu_touch_wacom(void)     { return g_wacom; }
const char *wubu_touch_driver(void){ return g_touch_drv[0] ? g_touch_drv : NULL; }

/* ---- W3: touch driver routing ---- */
const char *wubu_touch_driver_for(const char *dev)
{
    if (!dev) return NULL;
    if (strstr(dev, "elan"))    return "elan_i2c";
    if (strstr(dev, "synaptics") || strstr(dev, "rmi")) return "rmi4";
    if (strstr(dev, "alps"))    return "alps";
    if (strstr(dev, "wacom"))   return "wacom";
    if (strstr(dev, "goodix"))  return "goodix_ts";
    if (strstr(dev, "cypress")) return "cypress-sf";
    if (strstr(dev, "silead"))  return "silead";
    return "hid-multitouch";
}

/* ---- W4: summary ---- */
int wubu_touch_summary(char *out, size_t cap)
{
    return snprintf(out, cap,
        "touch[touch=%d elan=%d synaptics=%d mt=%d wacom=%d drv=%s]",
        g_touch, g_elan, g_synaptics, g_multitouch, g_wacom,
        wubu_touch_driver() ? wubu_touch_driver() : "none");
}
