/*
 * wubu_usb4.c -- kernel-owned USB4/Thunderbolt driver routing.
 *
 * USB4/Thunderbolt is the high-speed daisy-chaining interconnect (40/80
 * Gbps): external GPUs, docks, NVMe, displays over one cable. "Runs on
 * everything" includes the TB/USB4 routing + security (bolt).
 *
 * Thunderbolt/USB4 controller drivers:
 *   - Intel: thunderbolt.ko (Titan Ridge, Maple Ridge, Goshen Ridge),
 *     iommu + nvm firmware update
 *   - AMD: usb4 (USB4 domain manager, amd_pmc)
 *   - Apple: thunderbolt (Titan Ridge/ridge)
 *   - routing: bolt (userspace bolt manager), /sys/bus/thunderbolt
 *
 * WuBuOS owns this: detect the TB/USB4 host controller + topology, route
 * to the right driver, and expose the TB security mode (user/none/secure).
 *
 * Research (Kevin-Bacon 7-hop on the USB4/TB frontier):
 *   - thunderbolt.ko: Intel/AMD/Apple TB3 + USB4 host controller driver
 *   - bolt: userspace TB manager (authorization, security levels)
 *   - /sys/bus/thunderbolt: domains, routers, ports, NVM
 *   - USB4: thunderbolt driver now handles USB4 (retimer, CLx)
 */
#include "wubu_usb4.h"
#include "wubu_pci.h"
#include "wubu_hw_detect.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>

/* ---- Global state ---- */
static int  g_tb = 0;           /* Thunderbolt present */
static int  g_usb4 = 0;         /* USB4 present */
static int  g_bolt = 0;         /* bolt manager present */
static int  g_secure = 0;       /* TB security mode */
static char g_tb_drv[32] = "";
static int  g_domains = 0;

/* ---- W1: probe the USB4/TB topology ---- */
void wubu_usb4_probe(void)
{
    g_tb = 0; g_usb4 = 0; g_bolt = 0; g_secure = 0; g_domains = 0;
    g_tb_drv[0] = '\0';

#ifdef _GNU_SOURCE
    /* Thunderbolt bus present? */
    if (access("/sys/bus/thunderbolt/devices", R_OK) == 0 ||
        access("/sys/bus/thunderbolt", R_OK) == 0) {
        g_tb = 1;
        strcpy(g_tb_drv, "thunderbolt");
        /* domains */
        struct dirent **e;
        int n = scandir("/sys/bus/thunderbolt/devices", &e, NULL, alphasort);
        for (int i = 0; i < n; i++) {
            if (strstr(e[i]->d_name, "domain")) g_domains++;
        }
        /* security mode */
        for (int i = 0; i < 16; i++) {
            char p[128];
            snprintf(p, sizeof(p), "/sys/bus/thunderbolt/devices/domain%d/security", i);
            if (access(p, R_OK) == 0) {
                FILE *f = fopen(p, "r");
                if (f) {
                    char m[16] = "";
                    if (fgets(m, sizeof(m), f) && strcmp(m, "user\n") && strcmp(m, "secure\n"))
                        g_secure = 1;
                    fclose(f);
                }
                break;
            }
        }
    }
    /* bolt manager? */
    if (access("/usr/bin/boltctl", R_OK) == 0 ||
        access("/usr/libexec/bolt/boltd", R_OK) == 0) {
        g_bolt = 1;
    }
    /* USB4 (vs TB3) - presence of usb4_port sysfs or amd usb4 */
    if (access("/sys/bus/thunderbolt/devices/usb4_port", R_OK) == 0) {
        g_usb4 = 1;
    }
#endif
}

/* ---- W2: accessors ---- */
int  wubu_usb4_tb(void)       { return g_tb; }
int  wubu_usb4_usb4(void)     { return g_usb4; }
int  wubu_usb4_bolt(void)     { return g_bolt; }
int  wubu_usb4_secure(void)   { return g_secure; }
int  wubu_usb4_domains(void)  { return g_domains; }
const char *wubu_usb4_driver(void){ return g_tb_drv[0] ? g_tb_drv : NULL; }

/* ---- W3: TB/USB4 driver routing ---- */
const char *wubu_usb4_driver_for(const char *host)
{
    if (!host) return NULL;
    if (strstr(host, "intel"))    return "thunderbolt";
    if (strstr(host, "amd"))      return "usb4";
    if (strstr(host, "apple"))    return "thunderbolt";
    if (strstr(host, "titan"))    return "thunderbolt";
    if (strstr(host, "maple"))    return "thunderbolt";
    if (strstr(host, "goshen"))   return "thunderbolt";
    return "thunderbolt";
}

/* ---- W4: summary ---- */
int wubu_usb4_summary(char *out, size_t cap)
{
    return snprintf(out, cap,
        "usb4[tb=%d usb4=%d bolt=%d secure=%d domains=%d drv=%s]",
        g_tb, g_usb4, g_bolt, g_secure, g_domains,
        wubu_usb4_driver() ? wubu_usb4_driver() : "none");
}
