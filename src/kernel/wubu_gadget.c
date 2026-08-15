/*
 * wubu_gadget.c -- kernel-owned USB gadget mode + NVMe endurance routing.
 *
 * Two capabilities:
 *   - USB gadget: the device acts as a USB peripheral (mass storage,
 *     network RNDIS, serial ACM, HID). configfs: /sys/kernel/config.
 *   - NVMe endurance: SSD health telemetry (SMART percentage used,
 *     media wear, TBW).
 *
 * USB gadget:
 *   - udc: USB device controller (dwc3, cdns3, xhci-rcar)
 *   - configfs: /sys/kernel/config/usb_gadget (functions, configs)
 *   - function: mass_storage, rndis, acm, hid, uvc
 *   - /sys/class/udc: device controllers
 *
 * NVMe endurance:
 *   - SMART/health: nvme smart-log (percentage used, media wear)
 *   - /sys/class/nvme: nvme0, nvme1, health
 *   - telemetry: endurance group (media wear, data units written)
 *
 * WuBuOS owns this: detect USB gadget capability (UDC + configfs) and
 * NVMe endurance, route to the right driver, expose the topology.
 *
 * Research (Kevin-Bacon 7-hop on the gadget/endurance frontier):
 *   - gadget_configfs: USB gadget functions via configfs
 *   - udc: device controllers (dwc3, cdns3)
 *   - nvme smart-log: percentage used, media wear, TBW
 */
#include "wubu_gadget.h"
#include "wubu_hw_detect.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

/* ---- Global state ---- */
static int  g_udc = 0;         /* USB device controller */
static int  g_configfs = 0;    /* gadget configfs */
static int  g_gadget = 0;      /* gadget active */
static int  g_nvme = 0;        /* NVMe present */
static int  g_smart = 0;       /* SMART/health */
static char g_gadget_drv[24] = "";

/* ---- W1: probe the gadget/endurance topology ---- */
void wubu_gadget_probe(void)
{
    g_udc = 0; g_configfs = 0; g_gadget = 0; g_nvme = 0; g_smart = 0;
    g_gadget_drv[0] = '\0';

#ifdef WUBU_HOSTED
    /* USB device controller (UDC)? */
    if (access("/sys/class/udc", R_OK) == 0 &&
        access("/sys/module/dwc3", R_OK) == 0) {
        g_udc = 1;
        strcpy(g_gadget_drv, "dwc3");
    } else if (access("/sys/class/udc", R_OK) == 0 &&
               access("/sys/module/cdns3", R_OK) == 0) {
        g_udc = 1;
        strcpy(g_gadget_drv, "cdns3");
    } else if (access("/sys/class/udc", R_OK) == 0) {
        g_udc = 1;
        strcpy(g_gadget_drv, "udc");
    }
    /* Gadget configfs? */
    if (access("/sys/kernel/config/usb_gadget", R_OK) == 0) {
        g_configfs = 1;
        g_gadget = 1;
        if (!g_gadget_drv[0]) strcpy(g_gadget_drv, "configfs");
    }
    /* NVMe present? */
    if (access("/sys/class/nvme", R_OK) == 0) {
        g_nvme = 1;
        /* SMART/health available? */
        if (access("/usr/sbin/nvme", R_OK) == 0 ||
            access("/usr/bin/nvme", R_OK) == 0) {
            g_smart = 1;
        }
    }
#endif
}

/* ---- W2: accessors ---- */
int  wubu_gadget_udc(void)    { return g_udc; }
int  wubu_gadget_configfs(void){ return g_configfs; }
int  wubu_gadget_active(void) { return g_gadget; }
int  wubu_gadget_nvme(void)   { return g_nvme; }
int  wubu_gadget_smart(void)  { return g_smart; }
const char *wubu_gadget_driver(void){ return g_gadget_drv[0] ? g_gadget_drv : NULL; }

/* ---- W3: routing ---- */
const char *wubu_gadget_function_for(const char *fn)
{
    if (!fn) return NULL;
    if (strstr(fn, "mass"))  return "mass_storage";
    if (strstr(fn, "rndis")) return "rndis";
    if (strstr(fn, "acm") || strstr(fn, "serial")) return "acm";
    if (strstr(fn, "hid"))   return "hid";
    if (strstr(fn, "uvc"))   return "uvc";
    if (strstr(fn, "ether") || strstr(fn, "ecm")) return "ecm";
    return "gadget-core";
}

const char *wubu_gadget_nvme_for(const char *health)
{
    if (!health) return NULL;
    if (strstr(health, "smart"))  return "smart-log";
    if (strstr(health, "wear"))   return "media-wear";
    if (strstr(health, "tbw"))    return "tbw";
    if (strstr(health, "percent") || strstr(health, "used")) return "pct-used";
    return "nvme-health";
}

/* ---- W4: summary ---- */
int wubu_gadget_summary(char *out, size_t cap)
{
    return snprintf(out, cap,
        "gadget[udc=%d cfgfs=%d active=%d nvme=%d smart=%d drv=%s]",
        g_udc, g_configfs, g_gadget, g_nvme, g_smart,
        wubu_gadget_driver() ? wubu_gadget_driver() : "none");
}
