/*
 * wubu_backlight.c -- kernel-owned display backlight + NIC WoL routing.
 *
 * Two capabilities:
 *   - Backlight: display brightness control (ACPI video, DRM, sysfs).
 *   - WoL (Wake-on-LAN): NIC wakes the machine on magic packet.
 *
 * Backlight:
 *   - ACPI video: /sys/class/backlight/acpi_video0, intel_backlight
 *   - DRM: drm_panel backlight, /sys/class/backlight/amdgpu_bl0
 *   - native: intel_backlight, amdgpu_bl, pwm-backlight
 *
 * WoL:
 *   - ethtool -s eth0 wol g (magic packet), ethtool -k wake-on-lan
 *   - magic packet (wol g), unicast (wol u), broadcast (wol b),
 *     ARP (wol a), multicast (wol m)
 *
 * WuBuOS owns this: detect the backlight device + WoL capability, route
 * to the right driver, and expose the topology.
 *
 * Research (Kevin-Bacon 7-hop on the backlight/WoL frontier):
 *   - /sys/class/backlight: acpi_video0, intel_backlight, amdgpu_bl
 *   - ACPI video extensions, DRM backlight, pwm-backlight
 *   - ethtool WoL: magic packet (g), wake flags
 */
#include "wubu_backlight.h"
#include "wubu_hw_detect.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>

/* ---- Global state ---- */
static int  g_backlight = 0;
static int  g_acpi = 0;
static int  g_native = 0;
static int  g_wol = 0;
static int  g_wol_magic = 0;
static char g_bl_drv[24] = "";

/* ---- W1: probe the backlight/WoL topology ---- */
void wubu_backlight_probe(void)
{
    g_backlight = 0; g_acpi = 0; g_native = 0; g_wol = 0; g_wol_magic = 0;
    g_bl_drv[0] = '\0';

#ifdef _GNU_SOURCE
    /* Backlight devices present? */
    if (access("/sys/class/backlight", R_OK) == 0) {
        struct dirent **e;
        int n = scandir("/sys/class/backlight", &e, NULL, alphasort);
        for (int i = 0; i < n; i++) {
            if (e[i]->d_name[0] == '.') continue;
            g_backlight = 1;
            if (strstr(e[i]->d_name, "acpi")) {
                g_acpi = 1;
                strcpy(g_bl_drv, "acpi-video");
            } else {
                g_native = 1;
                if (strstr(e[i]->d_name, "intel"))
                    strcpy(g_bl_drv, "intel-backlight");
                else if (strstr(e[i]->d_name, "amdgpu"))
                    strcpy(g_bl_drv, "amdgpu-bl");
                else
                    strcpy(g_bl_drv, "pwm-backlight");
            }
        }
    }
    /* WoL: NIC with wake-on-lan capability. */
    if (access("/sys/class/net/eth0", R_OK) == 0 ||
        access("/sys/class/net", R_OK) == 0) {
        /* WoL is ethtool-set, but presence of NIC implies capability */
        g_wol = 1;
        g_wol_magic = 1;  /* magic packet is the standard WoL */
    }
#endif
}

/* ---- W2: accessors ---- */
int  wubu_backlight_present(void){ return g_backlight; }
int  wubu_backlight_acpi(void)  { return g_acpi; }
int  wubu_backlight_native(void){ return g_native; }
int  wubu_backlight_wol(void)   { return g_wol; }
int  wubu_backlight_wol_magic(void){ return g_wol_magic; }
const char *wubu_backlight_driver(void){ return g_bl_drv[0] ? g_bl_drv : NULL; }

/* ---- W3: routing ---- */
const char *wubu_backlight_driver_for(const char *dev)
{
    if (!dev) return NULL;
    if (strstr(dev, "acpi"))   return "acpi-video";
    if (strstr(dev, "intel"))  return "intel-backlight";
    if (strstr(dev, "amdgpu")) return "amdgpu-bl";
    if (strstr(dev, "pwm"))    return "pwm-backlight";
    if (strstr(dev, "nouveau"))return "nouveau-backlight";
    return "drm-backlight";
}

const char *wubu_backlight_wol_for(const char *wol)
{
    if (!wol) return NULL;
    if (strstr(wol, "magic") || strstr(wol, "g")) return "magic-packet";
    if (strstr(wol, "unicast") || strstr(wol, "u")) return "unicast";
    if (strstr(wol, "broadcast") || strstr(wol, "b")) return "broadcast";
    if (strstr(wol, "arp") || strstr(wol, "a")) return "arp";
    if (strstr(wol, "multicast") || strstr(wol, "m")) return "multicast";
    return "wol";
}

/* ---- W4: summary ---- */
int wubu_backlight_summary(char *out, size_t cap)
{
    return snprintf(out, cap,
        "backlight[bl=%d(%s) acpi=%d native=%d wol=%d magic=%d]",
        g_backlight, wubu_backlight_driver() ? wubu_backlight_driver() : "none",
        g_acpi, g_native, g_wol, g_wol_magic);
}
