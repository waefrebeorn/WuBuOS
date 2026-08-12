/*
 * wubu_ptp.c -- kernel-owned Ethernet PTP/TSN + input haptics routing.
 *
 * Two capabilities:
 *   - PTP/TSN: precision time (hardware clock timestamping) + time-
 *     sensitive networking for industrial/AV. "Runs on everything"
 *     includes deterministic networking.
 *   - Haptics: input vibration/force feedback (rumble, adaptive triggers).
 *
 * PTP/TSN:
 *   - PTP hardware clock (phc): igb/i210 ptp, ixgbe, ice, mlx5
 *   - 802.1AS gPTP: automotive/AV time sync
 *   - TSN: time-aware shaper, credit-based shaping, frame preemption
 *
 * Haptics:
 *   - input force-feedback (FF_RUMBLE, FF_PERIODIC): ff-memless
 *   - gamepad rumble: xpad/playstation/sony (adaptive triggers)
 *   - haptics: input_ff, evdev force feedback events
 *
 * WuBuOS owns this: detect the PTP hardware clock + TSN support + the
 * haptic/force-feedback capability, route to the right driver.
 *
 * Research (Kevin-Bacon 7-hop on the PTP/TSN + haptics frontier):
 *   - PTP: phc drivers (igb, ixgbe, ice, i210), /dev/ptp0, phc2sys
 *   - TSN: taprio (time-aware shaper), etf (earliest tx), mqprio
 *   - haptics: ff-memless, FF_RUMBLE, xpad force feedback
 */
#include "wubu_ptp.h"
#include "wubu_hw_detect.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

/* ---- Global state ---- */
static int  g_ptp = 0;          /* PTP hardware clock */
static int  g_tsn = 0;          /* TSN support */
static int  g_haptic = 0;       /* force-feedback/haptics */
static int  g_phc_clocks = 0;
static char g_ptp_drv[24] = "";
static char g_haptic_drv[24] = "";

/* ---- W1: probe the PTP/TSN + haptics topology ---- */
void wubu_ptp_probe(void)
{
    g_ptp = 0; g_tsn = 0; g_haptic = 0; g_phc_clocks = 0;
    g_ptp_drv[0] = '\0'; g_haptic_drv[0] = '\0';

#ifdef _GNU_SOURCE
    /* PTP hardware clocks (/dev/ptpN)? */
    for (int i = 0; i < 8; i++) {
        char p[64];
        snprintf(p, sizeof(p), "/dev/ptp%d", i);
        if (access(p, R_OK) == 0) g_phc_clocks++;
    }
    if (g_phc_clocks > 0) {
        g_ptp = 1;
        if (access("/sys/class/ptp", R_OK) == 0)
            strcpy(g_ptp_drv, "ptp");
        else
            strcpy(g_ptp_drv, "phc");
    }
    /* TSN support (taprio qdisc)? */
    if (access("/sys/kernel/debug/net", R_OK) == 0 ||
        access("/proc/net/taprio", R_OK) == 0) {
        g_tsn = 1;
    }
    /* Haptics / force-feedback present? */
    if (access("/sys/bus/usb/drivers/xpad", R_OK) == 0 ||
        access("/sys/bus/usb/drivers/sony", R_OK) == 0 ||
        access("/sys/bus/hid/drivers/hid-ff", R_OK) == 0) {
        g_haptic = 1;
        if (access("/sys/bus/usb/drivers/xpad", R_OK) == 0)
            strcpy(g_haptic_drv, "xpad");
        else if (access("/sys/bus/usb/drivers/sony", R_OK) == 0)
            strcpy(g_haptic_drv, "sony");
        else
            strcpy(g_haptic_drv, "hid-ff");
    }
#endif
}

/* ---- W2: accessors ---- */
int  wubu_ptp_present(void)     { return g_ptp; }
int  wubu_ptp_has_tsn(void)     { return g_tsn; }
int  wubu_ptp_has_haptic(void)  { return g_haptic; }
int  wubu_ptp_phc_clocks(void)  { return g_phc_clocks; }
const char *wubu_ptp_driver(void){ return g_ptp_drv[0] ? g_ptp_drv : NULL; }
const char *wubu_ptp_haptic_driver(void){ return g_haptic_drv[0] ? g_haptic_drv : NULL; }

/* ---- W3: driver routing ---- */
const char *wubu_ptp_driver_for(const char *nic)
{
    if (!nic) return NULL;
    if (strstr(nic, "igb") || strstr(nic, "i210") || strstr(nic, "i225")) return "igb-ptp";
    if (strstr(nic, "ixgbe")) return "ixgbe-ptp";
    if (strstr(nic, "ice"))   return "ice-ptp";
    if (strstr(nic, "mlx5"))  return "mlx5-ptp";
    if (strstr(nic, "tsn"))   return "tsn";
    return "ptp";
}

const char *wubu_ptp_haptic_for(const char *dev)
{
    if (!dev) return NULL;
    if (strstr(dev, "xbox") || strstr(dev, "xpad")) return "xpad";
    if (strstr(dev, "sony") || strstr(dev, "dual")) return "sony";
    if (strstr(dev, "playstation")) return "sony";
    if (strstr(dev, "hid"))   return "hid-ff";
    return "ff-memless";
}

/* ---- W4: summary ---- */
int wubu_ptp_summary(char *out, size_t cap)
{
    return snprintf(out, cap,
        "ptp[ptp=%d phc=%d tsn=%d haptic=%d(%s) drv=%s]",
        g_ptp, g_phc_clocks, g_tsn, g_haptic,
        wubu_ptp_haptic_driver() ? wubu_ptp_haptic_driver() : "none",
        wubu_ptp_driver() ? wubu_ptp_driver() : "none");
}
