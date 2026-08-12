/*
 * wubu_sata.c -- kernel-owned advanced SATA/NCQ driver routing.
 *
 * SATA is the ubiquitous disk bus (HDD/SSD). NCQ (native command queuing)
 * lets multiple commands fly at once; hotplug + port multipliers extend
 * it. "Runs on everything" includes the full SATA stack.
 *
 * SATA capabilities:
 *   - NCQ (native command queuing): up to 32 outstanding commands
 *   - Hotplug: async device insert/remove (libata, /sys/bus/ata)
 *   - Port multiplier (sata_pmp): one port -> 15 devices
 *   - Link power management: partial/slumber (low power)
 *   - SMART: health/self-test via /dev/sdX + smartctl
 *   - libahci: AHCI driver (the SATA controller standard)
 *
 * WuBuOS owns this: detect the SATA/NCQ topology, route to the right
 * driver, and expose the SATA capability set.
 *
 * Research (Kevin-Bacon 7-hop on the SATA frontier):
 *   - libahci: AHCI driver (ICH8-10, SB700+, generic ahci)
 *   - NCQ: /sys/block/sdX/device/queue_depth, libata ncq
 *   - sata_pmp: port multiplier support (CONFIG_SATA_PMP)
 *   - hotplug: libata async scan, /sys/bus/ata/devices/ataN
 */
#include "wubu_sata.h"
#include "wubu_hw_detect.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

/* ---- Global state ---- */
static int  g_sata = 0;
static int  g_ahci = 0;
static int  g_ncq = 0;
static int  g_hotplug = 0;
static int  g_pmp = 0;          /* port multiplier */
static int  g_smart = 0;
static char g_sata_drv[24] = "";

/* ---- W1: probe the SATA topology ---- */
void wubu_sata_probe(void)
{
    g_sata = 0; g_ahci = 0; g_ncq = 0; g_hotplug = 0;
    g_pmp = 0; g_smart = 0;
    g_sata_drv[0] = '\0';

#ifdef _GNU_SOURCE
    /* AHCI/SATA host present? */
    if (access("/sys/bus/pci/drivers/ahci", R_OK) == 0 ||
        access("/sys/bus/ata", R_OK) == 0) {
        g_sata = 1; g_ahci = 1;
        strcpy(g_sata_drv, "ahci");
        g_ncq = 1;   /* AHCI always supports NCQ */
        g_hotplug = 1; /* AHCI supports hotplug */
    }
    /* Port multiplier present? */
    if (access("/sys/bus/pci/drivers/sata_pmp", R_OK) == 0 ||
        access("/sys/bus/ata/drivers/sata_pmp", R_OK) == 0) {
        g_pmp = 1;
    }
    /* SMART present? */
    if (access("/usr/sbin/smartctl", R_OK) == 0) {
        g_smart = 1;
    }
#endif
}

/* ---- W2: accessors ---- */
int  wubu_sata_present(void)   { return g_sata; }
int  wubu_sata_has_ahci(void)  { return g_ahci; }
int  wubu_sata_has_ncq(void)   { return g_ncq; }
int  wubu_sata_has_hotplug(void){ return g_hotplug; }
int  wubu_sata_has_pmp(void)   { return g_pmp; }
int  wubu_sata_has_smart(void) { return g_smart; }
const char *wubu_sata_driver(void){ return g_sata_drv[0] ? g_sata_drv : NULL; }

/* ---- W3: SATA driver routing ---- */
const char *wubu_sata_controller_driver(const char *ctrl)
{
    if (!ctrl) return NULL;
    if (strstr(ctrl, "ahci"))     return "ahci";
    if (strstr(ctrl, "sata_pmp")) return "sata_pmp";
    if (strstr(ctrl, "nvme"))     return "nvme";
    if (strstr(ctrl, "usb-storage")) return "usb-storage";
    if (strstr(ctrl, "ide"))      return "ata_piix";
    return "libata";
}

/* ---- W4: summary ---- */
int wubu_sata_summary(char *out, size_t cap)
{
    return snprintf(out, cap,
        "sata[present=%d ahci=%d ncq=%d hotplug=%d pmp=%d smart=%d drv=%s]",
        g_sata, g_ahci, g_ncq, g_hotplug, g_pmp, g_smart,
        wubu_sata_driver() ? wubu_sata_driver() : "none");
}
