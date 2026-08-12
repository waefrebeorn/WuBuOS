/*
 * wubu_trim.c -- kernel-owned storage TRIM + USB-C alt mode routing.
 *
 * Two capabilities:
 *   - Storage TRIM: SSD discard (fstrim, ATA/NVMe TRIM) to maintain
 *     write performance + endurance.
 *   - USB-C alt mode: the USB-C connector carries DisplayPort /
 *     Thunderbolt (ALT mode) in addition to USB.
 *
 * Storage TRIM:
 *   - fstrim: periodic discard (systemd fstrim.timer)
 *   - discard: mount option (ext4, btrfs, xfs, f2fs)
 *   - ATA TRIM (DSM), NVMe DEALLOCATE
 *   - /sys/block sd device discard_granularity
 *
 * USB-C alt mode:
 *   - DisplayPort alt mode: USB-C carries DP
 *   - Thunderbolt: TB3/4 via USB-C (thunderbolt.ko)
 *   - USB4: unified (already a frontier); typec_switch
 *   - /sys/class/typec port_type
 *
 * WuBuOS owns this: detect TRIM support + USB-C alt mode, route to the
 * right driver, and expose the topology.
 *
 * Research (Kevin-Bacon 7-hop on the TRIM/alt-mode frontier):
 *   - fstrim: periodic discard; discard mount option
 *   - ATA TRIM (DSM), NVMe DEALLOCATE
 *   - DisplayPort alt mode over USB-C
 *   - Thunderbolt via USB-C (TB3/4)
 */
#include "wubu_trim.h"
#include "wubu_hw_detect.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

/* ---- Global state ---- */
static int  g_trim = 0;        /* TRIM supported */
static int  g_fstrim = 0;      /* fstrim present */
static int  g_discard = 0;     /* discard mount opt */
static int  g_altmode = 0;     /* USB-C alt mode */
static int  g_tb = 0;          /* Thunderbolt */
static char g_trim_drv[24] = "";

/* ---- W1: probe the TRIM/alt-mode topology ---- */
void wubu_trim_probe(void)
{
    g_trim = 0; g_fstrim = 0; g_discard = 0; g_altmode = 0; g_tb = 0;
    g_trim_drv[0] = '\0';

#ifdef _GNU_SOURCE
    /* TRIM support (SSD discard granularity)? */
    if (access("/usr/sbin/fstrim", R_OK) == 0 ||
        access("/usr/bin/fstrim", R_OK) == 0) {
        g_fstrim = 1; g_trim = 1;
        strcpy(g_trim_drv, "fstrim");
    }
    /* discard mount option (btrfs/ext4/xfs)? */
    if (access("/proc/mounts", R_OK) == 0) {
        FILE *f = fopen("/proc/mounts", "r");
        if (f) {
            char line[512];
            while (fgets(line, sizeof(line), f)) {
                if (strstr(line, "discard")) { g_discard = 1; break; }
            }
            fclose(f);
        }
    }
    /* USB-C alt mode (typec switch)? */
    if (access("/sys/class/typec", R_OK) == 0 ||
        access("/sys/module/typec", R_OK) == 0) {
        g_altmode = 1;
        if (!g_trim_drv[0]) strcpy(g_trim_drv, "typec-altmode");
    }
    /* Thunderbolt (TB3/4)? */
    if (access("/sys/bus/thunderbolt", R_OK) == 0 ||
        access("/sys/module/thunderbolt", R_OK) == 0) {
        g_tb = 1;
        if (!g_trim_drv[0]) strcpy(g_trim_drv, "thunderbolt");
    }
#endif
}

/* ---- W2: accessors ---- */
int  wubu_trim_supported(void){ return g_trim; }
int  wubu_trim_fstrim(void)   { return g_fstrim; }
int  wubu_trim_discard(void)  { return g_discard; }
int  wubu_trim_altmode(void)  { return g_altmode; }
int  wubu_trim_thunderbolt(void){ return g_tb; }
const char *wubu_trim_driver(void){ return g_trim_drv[0] ? g_trim_drv : NULL; }

/* ---- W3: TRIM/alt-mode routing ---- */
const char *wubu_trim_mode_for(const char *fs)
{
    if (!fs) return NULL;
    if (strstr(fs, "ext4")) return "ext4-discard";
    if (strstr(fs, "btrfs")) return "btrfs-discard";
    if (strstr(fs, "xfs"))  return "xfs-discard";
    if (strstr(fs, "f2fs")) return "f2fs-discard";
    if (strstr(fs, "nvme")) return "nvme-deallocate";
    if (strstr(fs, "ata") || strstr(fs, "ahci")) return "ata-trim";
    return "trim";
}

const char *wubu_trim_altmode_for(const char *mode)
{
    if (!mode) return NULL;
    if (strstr(mode, "dp"))   return "displayport-alt";
    if (strstr(mode, "tb"))   return "thunderbolt-alt";
    if (strstr(mode, "usb4")) return "usb4";
    if (strstr(mode, "dummy"))return "dummy";
    return "typec-altmode";
}

/* ---- W4: summary ---- */
int wubu_trim_summary(char *out, size_t cap)
{
    return snprintf(out, cap,
        "trim[trim=%d fstrim=%d discard=%d altmode=%d tb=%d drv=%s]",
        g_trim, g_fstrim, g_discard, g_altmode, g_tb,
        wubu_trim_driver() ? wubu_trim_driver() : "none");
}
