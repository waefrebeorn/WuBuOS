/*
 * wubu_raid.c -- kernel-owned RAID/SAS storage driver routing.
 *
 * Servers and high-end workstations use hardware RAID controllers and SAS
 * HBAs. "Runs on everything" includes the data-center storage tier. The
 * kernel must route the RAID controller/HBA to the right driver and expose
 * the RAID arrays (via mdadm for software RAID, or the controller's own
 * driver for hardware RAID).
 *
 * RAID/HBA drivers:
 *   - Broadcom/LSI MegaRAID: megaraid_sas (hardware RAID)
 *   - Broadcom/LSI SAS HBA: mpt3sas (SAS/SATA HBA)
 *   - Microsemi/Avago aacraid: aacraid (Adaptec/PMC)
 *   - HPE Smart Array: hpsa (smartpqi successor)
 *   - Microchip SmartPQI: smartpqi (HPE Gen11+)
 *   - Marvell: mv_sas, mvsas
 *   - Areca: arcmsr
 *   - 3ware/LSI: 3w-9xxx, 3w-sas
 *   - Software RAID: md (md/raid0-10, mdadm userspace)
 *
 * WuBuOS owns this: detect the RAID/HBA (PCI class), route to the right
 * driver, and expose the storage-array topology.
 *
 * Research (Kevin-Bacon 7-hop on the RAID frontier):
 *   - megaraid_sas: Broadcom MegaRAID hardware RAID (the server standard)
 *   - mpt3sas: LSI/Broadcom SAS HBA (JBOD/RAID0/1/10)
 *   - hpsa/smartpqi: HPE Smart Array
 *   - aacraid: Adaptec/PMC/Microsemi
 *   - md: software RAID (mdadm), raid0/1/4/5/6/10
 */
#include "wubu_raid.h"
#include "wubu_pci.h"
#include "wubu_hw_detect.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

/* ---- PCI classes: RAID/HBA ---- */
#define PCI_CLASS_STORAGE   0x01
#define PCI_SUBCLASS_RAID   0x04   /* RAID controller */
#define PCI_SUBCLASS_SAS    0x07   /* Serial Attached SCSI */
#define PCI_SUBCLASS_SCSI   0x00   /* SCSI */
#define PCI_VENDOR_BROADCOM 0x1000  /* LSI/Broadcom/MegaRAID */
#define PCI_VENDOR_MICROSEMI 0x9005 /* Adaptec/aacraid */
#define PCI_VENDOR_MICROCHIP 0x1137 /* SmartPQI */
#define PCI_VENDOR_MARVELL  0x1B4B  /* SAS */
#define PCI_VENDOR_ARECA    0x1B41

/* ---- Global state ---- */
static int  g_raid = 0;
static int  g_sas = 0;
static int  g_md = 0;           /* software RAID (md) */
static char g_raid_drv[32] = "";
static char g_raid_name[32] = "";

/* ---- W1: probe the RAID topology ---- */
void wubu_raid_probe(void)
{
    g_raid = 0; g_sas = 0; g_md = 0;
    g_raid_drv[0] = '\0'; g_raid_name[0] = '\0';

#ifdef _GNU_SOURCE
    /* Software RAID (md) present? */
    if (access("/dev/md0", R_OK) == 0 || access("/proc/mdstat", R_OK) == 0) {
        g_md = 1;
        g_raid = 1;
        strcpy(g_raid_drv, "md");
        strcpy(g_raid_name, "software RAID");
    }

    /* Bare metal: scan PCI for RAID/SAS controllers. */
    if (wubu_hw_is_wsl()) return;
    wubu_pci_dev_t devs[WUBU_PCI_MAX_DEVS];
    int n = wubu_pci_scan(devs, WUBU_PCI_MAX_DEVS);
    for (int i = 0; i < n; i++) {
        if ((devs[i].class_code >> 8) != PCI_CLASS_STORAGE) continue;
        if (devs[i].subclass == PCI_SUBCLASS_RAID) {
            g_raid = 1;
            g_raid_drv[0] = '\0';
            if (devs[i].vendor == PCI_VENDOR_BROADCOM) {
                strcpy(g_raid_drv, "megaraid_sas");
                strcpy(g_raid_name, "MegaRAID");
            } else if (devs[i].vendor == PCI_VENDOR_MICROSEMI) {
                strcpy(g_raid_drv, "aacraid");
                strcpy(g_raid_name, "Adaptec");
            } else if (devs[i].vendor == PCI_VENDOR_MICROCHIP) {
                strcpy(g_raid_drv, "smartpqi");
                strcpy(g_raid_name, "SmartPQI");
            } else if (devs[i].vendor == PCI_VENDOR_ARECA) {
                strcpy(g_raid_drv, "arcmsr");
                strcpy(g_raid_name, "Areca");
            } else {
                strcpy(g_raid_drv, "megaraid_sas");
                strcpy(g_raid_name, "hardware RAID");
            }
        } else if (devs[i].subclass == PCI_SUBCLASS_SAS) {
            g_sas = 1;
            g_raid = 1;
            if (devs[i].vendor == PCI_VENDOR_BROADCOM) {
                strcpy(g_raid_drv, "mpt3sas");
                strcpy(g_raid_name, "SAS HBA");
            } else if (devs[i].vendor == PCI_VENDOR_MARVELL) {
                strcpy(g_raid_drv, "mv_sas");
                strcpy(g_raid_name, "Marvell SAS");
            } else {
                strcpy(g_raid_drv, "mpt3sas");
                strcpy(g_raid_name, "SAS HBA");
            }
        }
        if (g_raid_drv[0]) break;
    }
#endif
}

/* ---- W2: accessors ---- */
int  wubu_raid_present(void)   { return g_raid; }
int  wubu_raid_has_sas(void)   { return g_sas; }
int  wubu_raid_has_md(void)    { return g_md; }
const char *wubu_raid_driver(void){ return g_raid_drv[0] ? g_raid_drv : NULL; }
const char *wubu_raid_name(void){ return g_raid_name[0] ? g_raid_name : NULL; }

/* ---- W3: RAID driver routing ---- */
const char *wubu_raid_controller_driver(const char *vendor)
{
    if (!vendor) return NULL;
    if (strstr(vendor, "megaraid")) return "megaraid_sas";
    if (strstr(vendor, "mpt3") || strstr(vendor, "lsi")) return "mpt3sas";
    if (strstr(vendor, "smartpqi") || strstr(vendor, "hpe")) return "smartpqi";
    if (strstr(vendor, "aacraid") || strstr(vendor, "adaptec")) return "aacraid";
    if (strstr(vendor, "mv_sas") || strstr(vendor, "marvell")) return "mv_sas";
    if (strstr(vendor, "arcmsr") || strstr(vendor, "areca")) return "arcmsr";
    if (strstr(vendor, "3w") || strstr(vendor, "3ware")) return "3w-sas";
    return "scsi_mod";
}

/* ---- W4: summary ---- */
int wubu_raid_summary(char *out, size_t cap)
{
    return snprintf(out, cap,
        "raid[present=%d sas=%d md=%d drv=%s name=%s]",
        g_raid, g_sas, g_md,
        wubu_raid_driver() ? wubu_raid_driver() : "none",
        wubu_raid_name() ? wubu_raid_name() : "-");
}
