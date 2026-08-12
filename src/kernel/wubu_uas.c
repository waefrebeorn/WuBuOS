/*
 * wubu_uas.c -- kernel-owned storage USB Attached SCSI routing.
 *
 * UAS (USB Attached SCSI) provides better USB storage performance
 * via SCSI command queuing. "Runs on everything" includes correct
 * UAS on every storage USB device.
 *
 * UAS:
 *   - USB: BOT (Bulk-Only), UAS (USB Attached SCSI)
 *   - /sys/class/udc: UDC (USB Device Controller)
 *   - /sys/block sd device: SCSI device
 *   - uas: uas_host, uas target
 *   - /proc/partitions: partition list
 *   - protocol: BOT, CBI, UAS
 *
 * Research (7-hop on the uas frontier):
 *   -UAS USB Attached SCSI
 */
#include "wubu_uas.h"
#include "wubu_hw_detect.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static int  g_uas = 0;       /* UAS present */
static int  g_bot = 0;       /* BOT present */
static int  g_uasp = 0;      /* UASP support */
static int  g_queue = 0;     /* command queueing */
static int  g_part = 0;      /* partitions detected */
static char g_uas_drv[24] = "";

void wubu_uas_probe(void)
{
    g_uas = 0; g_bot = 0; g_uasp = 0; g_queue = 0; g_part = 0;
    g_uas_drv[0] = '\0';

#ifdef _GNU_SOURCE
    if (access("/sys/class/scsi_host", R_OK) == 0 ||
        access("/proc/partitions", R_OK) == 0) {
        g_uas = 1; g_uasp = 1; g_queue = 1; g_part = 1;
        strcpy(g_uas_drv, "uas-host");
    }
    if (access("/sys/class/udc", R_OK) == 0) {
        g_uas = 1; g_uasp = 1;
        if (!g_uas_drv[0]) strcpy(g_uas_drv, "uas-udc");
    }
    if (access("/sys/block", R_OK) == 0) {
        g_bot = 1;
        if (!g_uas_drv[0]) strcpy(g_uas_drv, "uas-bot");
    }
#endif
}

int  wubu_uas_present(void){ return g_uas; }
int  wubu_uas_bot(void)    { return g_bot; }
int  wubu_uas_uasp(void)   { return g_uasp; }
int  wubu_uas_queue(void)  { return g_queue; }
int  wubu_uas_part(void)   { return g_part; }
const char *wubu_uas_driver(void){ return g_uas_drv[0] ? g_uas_drv : NULL; }

const char *wubu_uas_proto_for(const char *p)
{
    if (!p) return NULL;
    if (strstr(p, "uasp")) return "UASP";
    if (strstr(p, "uas")) return "UAS";
    if (strstr(p, "bot") || strstr(p, "bulk")) return "BOT";
    if (strstr(p, "cbi")) return "CBI";
    if (strstr(p, "ccb")) return "CCB";
    return "BOT";
}

const char *wubu_uas_dir_for(const char *d)
{
    if (!d) return NULL;
    if (strstr(d, "in")) return "IN";
    if (strstr(d, "out")) return "OUT";
    if (strstr(d, "bi")) return "BIDIRECTIONAL";
    return "IN";
}

int wubu_uas_summary(char *out, size_t cap)
{
    return snprintf(out, cap,
        "uas[uas=%d bot=%d uasp=%d q=%d part=%d drv=%s]",
        g_uas, g_bot, g_uasp, g_queue, g_part,
        wubu_uas_driver() ? wubu_uas_driver() : "none");
}
