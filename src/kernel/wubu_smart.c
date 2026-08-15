/*
 * wubu_smart.c -- kernel-owned storage SMART (S.M.A.R.T.) routing.
 *
 * SMART reports disk health + predictive failure from device sensors.
 * "Runs on everything" includes correct storage health.
 *
 * SMART:
 *   - /dev/sd*, /dev/nvme*: block device
 *   - smartctl (smartmontools): S.M.A.R.T. tool
 *   - /sys/class/ata_device dev smart: ATA SMART
 *   - NVMe: health log, smart log (0x02)
 *   - attributes: reallocated sectors, wear leveling, temp
 *   - threshold: warn/critical per attribute
 *
 * WuBuOS owns this: detect SMART + health + threshold, route to the
 * right driver, expose the topology.
 *
 * Research (7-hop on the SMART frontier):
 *   - ATA S.M.A.R.T. attributes
 *   - NVMe smart log
 *   - smartmontools smartctl
 */
#include "wubu_smart.h"
#include "wubu_hw_detect.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static int  g_smart = 0;       /* SMART present */
static int  g_ata = 0;         /* ATA SMART */
static int  g_nvme = 0;        /* NVMe smart */
static int  g_health = 0;      /* health log */
static int  g_temp = 0;        /* temp sensor */
static char g_smart_drv[24] = "";

void wubu_smart_probe(void)
{
    g_smart = 0; g_ata = 0; g_nvme = 0; g_health = 0; g_temp = 0;
    g_smart_drv[0] = '\0';

#ifdef WUBU_HOSTED
    if (access("/sys/class/ata_device", R_OK) == 0 ||
        access("/dev/sda", R_OK) == 0) {
        g_smart = 1; g_ata = 1; g_health = 1; g_temp = 1;
        strcpy(g_smart_drv, "ata-smart");
    }
    if (access("/sys/class/nvme", R_OK) == 0 ||
        access("/dev/nvme0", R_OK) == 0) {
        g_smart = 1; g_nvme = 1; g_health = 1; g_temp = 1;
        if (!g_smart_drv[0]) strcpy(g_smart_drv, "nvme-smart");
    }
    if (access("/usr/sbin/smartctl", R_OK) == 0 ||
        access("/usr/bin/smartctl", R_OK) == 0) {
        g_smart = 1; g_health = 1;
        if (!g_smart_drv[0]) strcpy(g_smart_drv, "smartmontools");
    }
#endif
}

int  wubu_smart_present(void){ return g_smart; }
int  wubu_smart_ata(void)     { return g_ata; }
int  wubu_smart_nvme(void)    { return g_nvme; }
int  wubu_smart_health(void)  { return g_health; }
int  wubu_smart_temp(void)    { return g_temp; }
const char *wubu_smart_driver(void){ return g_smart_drv[0] ? g_smart_drv : NULL; }

const char *wubu_smart_attr_for(const char *a)
{
    if (!a) return NULL;
    if (strstr(a, "realloc")) return "reallocated-sectors";
    if (strstr(a, "wear"))   return "wear-leveling";
    if (strstr(a, "temp"))   return "temperature";
    if (strstr(a, "pending"))return "pending-sector";
    if (strstr(a, "uncorrect")) return "uncorrectable";
    return "unknown";
}

const char *wubu_smart_status_for(const char *s)
{
    if (!s) return NULL;
    if (strstr(s, "pass") || strstr(s, "ok")) return "ok";
    if (strstr(s, "warn") || strstr(s, "thresh")) return "warning";
    if (strstr(s, "fail") || strstr(s, "crit")) return "critical";
    return "unknown";
}

int wubu_smart_summary(char *out, size_t cap)
{
    return snprintf(out, cap,
        "smart[smart=%d ata=%d nvme=%d health=%d temp=%d drv=%s]",
        g_smart, g_ata, g_nvme, g_health, g_temp,
        wubu_smart_driver() ? wubu_smart_driver() : "none");
}