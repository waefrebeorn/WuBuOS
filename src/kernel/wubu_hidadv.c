/*
 * wubu_hidadv.c -- kernel-owned USB HID advanced driver routing.
 *
 * HID (human interface device) is how keyboards/mice/gamepads/touchscreens
 * connect. This module owns the *routing* of which HID driver the kernel
 * binds for a given device — the report/usage-class dispatch. It
 * complements wubu_hid.c (the unified HID event layer that feeds input
 * events): that driver feeds events, this one routes the driver.
 *
 * HID components:
 *   - hid-core: the HID bus + report descriptor parsing
 *   - hid-generic: the generic driver (covers most HID devices)
 *   - hid-multitouch: multitouch; hid-ff: force feedback
 *   - Vendor: hid-logitech (HID++), hid-apple, hid-steam, hid-sony
 *
 * WuBuOS owns this: detect the HID device + report/usage class, route to
 * the right HID driver, and expose the HID topology.
 *
 * Research (Kevin-Bacon 7-hop on the HID frontier):
 *   - hid-core, hid-generic: the HID bus + generic driver
 *   - hid-multitouch, hid-ff: multitouch + force feedback
 *   - Vendor: hid-logitech (HID++), hid-apple, hid-sony
 *   - report descriptors: /sys/kernel/debug/hid, HIDIOC userspace
 */
#include "wubu_hidadv.h"
#include "wubu_hw_detect.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

/* ---- Global state ---- */
static int  g_hid = 0;
static int  g_generic = 0;
static int  g_multitouch = 0;
static int  g_ff = 0;
static int  g_vendor = 0;
static char g_hid_drv[24] = "";

/* ---- W1: probe the HID topology ---- */
void wubu_hidadv_probe(void)
{
    g_hid = 0; g_generic = 0; g_multitouch = 0; g_ff = 0; g_vendor = 0;
    g_hid_drv[0] = '\0';

#ifdef WUBU_HOSTED
    /* HID bus present? */
    if (access("/sys/bus/hid", R_OK) == 0) {
        g_hid = 1;
        if (access("/sys/bus/hid/drivers/hid-generic", R_OK) == 0) {
            g_generic = 1;
            strcpy(g_hid_drv, "hid-generic");
        }
        if (access("/sys/bus/hid/drivers/hid-multitouch", R_OK) == 0) {
            g_multitouch = 1;
        }
        if (access("/sys/bus/hid/drivers/hid-ff", R_OK) == 0) {
            g_ff = 1;
        }
        if (access("/sys/bus/hid/drivers/logitech", R_OK) == 0 ||
            access("/sys/bus/hid/drivers/hid-logitech-dj", R_OK) == 0) {
            g_vendor = 1;
            if (!g_hid_drv[0]) strcpy(g_hid_drv, "hid-logitech");
        } else if (access("/sys/bus/hid/drivers/apple", R_OK) == 0) {
            g_vendor = 1;
            if (!g_hid_drv[0]) strcpy(g_hid_drv, "hid-apple");
        } else if (access("/sys/bus/hid/drivers/hid-sony", R_OK) == 0) {
            g_vendor = 1;
            if (!g_hid_drv[0]) strcpy(g_hid_drv, "hid-sony");
        }
    }
#endif
}

/* ---- W2: accessors ---- */
int  wubu_hidadv_present(void)    { return g_hid; }
int  wubu_hidadv_generic(void)    { return g_generic; }
int  wubu_hidadv_multitouch(void) { return g_multitouch; }
int  wubu_hidadv_ff(void)         { return g_ff; }
int  wubu_hidadv_vendor(void)     { return g_vendor; }
const char *wubu_hidadv_driver(void){ return g_hid_drv[0] ? g_hid_drv : NULL; }

/* ---- W3: HID driver routing ---- */
const char *wubu_hidadv_driver_for(const char *vendor)
{
    if (!vendor) return NULL;
    if (strstr(vendor, "logitech")) return "hid-logitech-dj";
    if (strstr(vendor, "apple"))    return "hid-apple";
    if (strstr(vendor, "sony") || strstr(vendor, "playstation")) return "hid-sony";
    if (strstr(vendor, "xbox"))     return "hid-xboxone";
    if (strstr(vendor, "steam"))    return "hid-steam";
    if (strstr(vendor, "multitouch") || strstr(vendor, "mt")) return "hid-multitouch";
    return "hid-generic";
}

/* ---- W4: summary ---- */
int wubu_hidadv_summary(char *out, size_t cap)
{
    return snprintf(out, cap,
        "hidadv[hid=%d generic=%d mt=%d ff=%d vendor=%d drv=%s]",
        g_hid, g_generic, g_multitouch, g_ff, g_vendor,
        wubu_hidadv_driver() ? wubu_hidadv_driver() : "none");
}
