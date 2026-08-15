/*
 * wubu_fw.c -- kernel-owned storage controller firmware routing.
 *
 * Storage controller firmware (RAID card, HBA flash) is updated via the
 * kernel / fw loader. "Runs on everything" includes correct firmware
 * loading on every storage controller.
 *
 * Firmware:
 *   - firmware_class: kernel fw loader (request_firmware)
 *   - /lib/firmware: firmware blob storage
 *   - megaraid_sas: MegaRAID flash
 *   - mpt2/3: LSI SAS flash (mpt2sas/mpt3sas)
 *   - hpsa: HP Smart Array firmware
 *   - raid card flash: via sysfs / proc flash interface
 *
 * WuBuOS owns this: detect firmware support + controller, route to the
 * right driver, and expose the topology.
 *
 * Research (Kevin-Bacon 7-hop on the FW frontier):
 *   - firmware_class: request_firmware
 *   - /lib/firmware blob storage
 *   - megaraid_sas / mpt2sas / mpt3sas / hpsa RAID flash
 */
#include "wubu_fw.h"
#include "wubu_hw_detect.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

/* ---- Global state ---- */
static int  g_fw = 0;          /* firmware loader */
static int  g_lib = 0;         /* /lib/firmware */
static int  g_raid = 0;        /* RAID controller */
static int  g_hba = 0;         /* SAS HBA */
static int  g_update = 0;      /* firmware update iface */
static char g_fw_drv[24] = "";

/* ---- W1: probe the FW topology ---- */
void wubu_fw_probe(void)
{
    g_fw = 0; g_lib = 0; g_raid = 0; g_hba = 0; g_update = 0;
    g_fw_drv[0] = '\0';

#ifdef WUBU_HOSTED
    /* firmware_class? */
    if (access("/sys/module/firmware_class", R_OK) == 0) {
        g_fw = 1;
        strcpy(g_fw_drv, "fw-loader");
    }
    /* /lib/firmware? */
    if (access("/lib/firmware", R_OK) == 0) {
        g_lib = 1;
        if (!g_fw_drv[0]) strcpy(g_fw_drv, "lib-firmware");
    }
    /* RAID controller (megaraid)? */
    if (access("/sys/bus/pci/drivers/megaraid_sas", R_OK) == 0 ||
        access("/sys/bus/pci/drivers/hpsa", R_OK) == 0 ||
        access("/sys/bus/pci/drivers/aacraid", R_OK) == 0) {
        g_raid = 1;
        if (!g_fw_drv[0]) strcpy(g_fw_drv, "raid-flash");
    }
    /* SAS HBA (mpt)? */
    if (access("/sys/bus/pci/drivers/mpt2sas", R_OK) == 0 ||
        access("/sys/bus/pci/drivers/mpt3sas", R_OK) == 0) {
        g_hba = 1;
        if (!g_fw_drv[0]) strcpy(g_fw_drv, "sas-hba");
    }
    /* firmware update interface (sysfs / flashing)? */
    if (access("/sys/class/firmware", R_OK) == 0) {
        g_update = 1;
    }
#endif
}

/* ---- W2: accessors ---- */
int  wubu_fw_loader(void) { return g_fw; }
int  wubu_fw_lib(void)    { return g_lib; }
int  wubu_fw_raid(void)   { return g_raid; }
int  wubu_fw_hba(void)    { return g_hba; }
int  wubu_fw_update(void) { return g_update; }
const char *wubu_fw_driver(void){ return g_fw_drv[0] ? g_fw_drv : NULL; }

/* ---- W3: firmware routing ---- */
const char *wubu_fw_controller_for(const char *ctrl)
{
    if (!ctrl) return NULL;
    if (strstr(ctrl, "megaraid") || strstr(ctrl, "megasas")) return "megaraid-sas";
    if (strstr(ctrl, "hpsa") || strstr(ctrl, "smartarray"))  return "hpsa";
    if (strstr(ctrl, "mpt3")) return "mpt3sas";
    if (strstr(ctrl, "mpt2")) return "mpt2sas";
    if (strstr(ctrl, "aac"))  return "aacraid";
    if (strstr(ctrl, "flash")) return "fw-flash";
    return "fw-loader";
}

const char *wubu_fw_stage_for(const char *stage)
{
    if (!stage) return NULL;
    if (strstr(stage, "load"))   return "load";
    if (strstr(stage, "verify")) return "verify";
    if (strstr(stage, "apply"))  return "apply";
    if (strstr(stage, "commit")) return "commit";
    return "load";
}

/* ---- W4: summary ---- */
int wubu_fw_summary(char *out, size_t cap)
{
    return snprintf(out, cap,
        "fw[fw=%d lib=%d raid=%d hba=%d update=%d drv=%s]",
        g_fw, g_lib, g_raid, g_hba, g_update,
        wubu_fw_driver() ? wubu_fw_driver() : "none");
}
