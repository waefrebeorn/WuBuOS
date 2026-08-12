/*
 * wubu_fingerprint.c -- kernel-owned fingerprint/biometric driver routing.
 *
 * Fingerprint readers secure login on laptops and phones. "Runs on
 * everything" includes the biometric tier. The kernel must route the
 * reader to the right driver and expose it to libfprint (fprintd, the
 * userspace fingerprint daemon).
 *
 * Fingerprint driver families (via libfprint drivers + kernel drivers):
 *   - Goodix: goodix-fp / goodixmoc (many laptops use Goodix)
 *   - VFS (Validity/Synaptics): vfs0090, vfs5011 (dell, thinkpad)
 *   - EgisTec: egis (many windows laptops)
 *   - AuthenTec: authenc (Apple/Huawei)
 *   - Elan: elan-fp
 *   - FPC (Fingerprint Cards): fpc1020/fpc1022 (Android/embedded)
 *   - Synaptics: synaptics-metalink
 *
 * WuBuOS owns this: detect the fingerprint reader (USB/HID/i2c), route to
 * the right driver, and flag biometric presence for the security layer.
 *
 * Research (Kevin-Bacon 7-hop on the biometric frontier):
 *   - libfprint + fprintd: the Linux fingerprint stack (userspace drivers)
 *   - goodix: Goodix readers (GoodixMoc, goodixmoc-hid)
 *   - vfs: Synaptics/Validity (Dell XPS, ThinkPad)
 *   - egis: EgisTec (Acer, Lenovo)
 *   - authenc: AuthenTec (Apple, Huawei)
 *   - fpc: Fingerprint Cards (Android)
 */
#include "wubu_fingerprint.h"
#include "wubu_hw_detect.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

/* ---- Global state ---- */
static int  g_fingerprint = 0;
static int  g_goodix = 0;
static int  g_vfs = 0;
static int  g_egis = 0;
static int  g_authenc = 0;
static int  g_fpc = 0;
static char g_fp_drv[32] = "";
static char g_fp_vendor[24] = "";

/* ---- W1: probe the fingerprint topology ---- */
void wubu_fingerprint_probe(void)
{
    g_fingerprint = 0; g_goodix = 0; g_vfs = 0; g_egis = 0;
    g_authenc = 0; g_fpc = 0;
    g_fp_drv[0] = '\0'; g_fp_vendor[0] = '\0';

#ifdef _GNU_SOURCE
    /* libfprint drivers present? (biometric USB/HID devices) */
    if (access("/sys/bus/usb/drivers/goodixmoc", R_OK) == 0 ||
        access("/sys/bus/hid/drivers/goodixmoc-hid", R_OK) == 0) {
        g_goodix = 1; g_fingerprint = 1;
        strcpy(g_fp_drv, "goodixmoc"); strcpy(g_fp_vendor, "Goodix");
    }
    if (access("/sys/bus/usb/drivers/vfs0090", R_OK) == 0 ||
        access("/sys/bus/usb/drivers/vfs5011", R_OK) == 0) {
        g_vfs = 1; g_fingerprint = 1;
        if (!g_fp_drv[0]) { strcpy(g_fp_drv, "vfs5011"); strcpy(g_fp_vendor, "Synaptics"); }
    }
    if (access("/sys/bus/usb/drivers/egis", R_OK) == 0) {
        g_egis = 1; g_fingerprint = 1;
        if (!g_fp_drv[0]) { strcpy(g_fp_drv, "egis"); strcpy(g_fp_vendor, "EgisTec"); }
    }
    if (access("/sys/bus/usb/drivers/authenc", R_OK) == 0) {
        g_authenc = 1; g_fingerprint = 1;
        if (!g_fp_drv[0]) { strcpy(g_fp_drv, "authenc"); strcpy(g_fp_vendor, "AuthenTec"); }
    }
    if (access("/sys/bus/spi/drivers/fpc1020", R_OK) == 0) {
        g_fpc = 1; g_fingerprint = 1;
        if (!g_fp_drv[0]) { strcpy(g_fp_drv, "fpc1020"); strcpy(g_fp_vendor, "Fingerprint Cards"); }
    }
#endif
}

/* ---- W2: accessors ---- */
int  wubu_fingerprint_present(void){ return g_fingerprint; }
int  wubu_fingerprint_goodix(void) { return g_goodix; }
int  wubu_fingerprint_vfs(void)    { return g_vfs; }
int  wubu_fingerprint_egis(void)   { return g_egis; }
int  wubu_fingerprint_authenc(void){ return g_authenc; }
int  wubu_fingerprint_fpc(void)    { return g_fpc; }
const char *wubu_fingerprint_driver(void){ return g_fp_drv[0] ? g_fp_drv : NULL; }
const char *wubu_fingerprint_vendor(void){ return g_fp_vendor[0] ? g_fp_vendor : NULL; }

/* ---- W3: fingerprint driver routing ---- */
const char *wubu_fingerprint_vendor_driver(const char *vendor)
{
    if (!vendor) return NULL;
    if (strstr(vendor, "goodix"))  return "goodixmoc";
    if (strstr(vendor, "vfs") || strstr(vendor, "synaptics")) return "vfs5011";
    if (strstr(vendor, "egis"))    return "egis";
    if (strstr(vendor, "authenc")) return "authenc";
    if (strstr(vendor, "fpc"))     return "fpc1020";
    if (strstr(vendor, "elan"))    return "elan-fp";
    return "libfprint";
}

/* ---- W4: summary ---- */
int wubu_fingerprint_summary(char *out, size_t cap)
{
    return snprintf(out, cap,
        "fp[present=%d goodix=%d vfs=%d egis=%d authenc=%d fpc=%d drv=%s]",
        g_fingerprint, g_goodix, g_vfs, g_egis, g_authenc, g_fpc,
        wubu_fingerprint_driver() ? wubu_fingerprint_driver() : "none");
}
