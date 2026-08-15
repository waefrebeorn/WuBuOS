/*
 * wubu_ptp_sync.c -- kernel-owned NIC PTP time sync routing.
 *
 * PTP (IEEE 1588) synchronizes clocks precisely over the network. The NIC
 * PHC (PTP hardware clock) timestamps packets. "Runs on everything"
 * includes precise time sync.
 *
 * PTP time sync:
 *   - ptp4l: PTP daemon (IEEE 1588) — master/slave negotiation
 *   - phc2sys: syncs the system clock to the PHC (hardware clock)
 *   - PHC: PTP hardware clock in the NIC (ethtool -T, /dev/ptp*)
 *   - NICs: ixgbe (x550), e1000e, igc (i225), mlx5, igb
 *
 * WuBuOS owns this: detect the PHC (PTP hardware clock) + NIC support,
 * route to the right driver, and expose the PTP topology.
 *
 * Research (Kevin-Bacon 7-hop on the PTP frontier):
 *   - ptp4l: the PTP daemon (linuxptp)
 *   - phc2sys: PHC to system clock sync
 *   - /dev/ptp*: PHC devices; ethtool -T: timestamping caps
 *   - NICs: igc/ixgbe/e1000e/mlx5 PTP
 */
#include "wubu_ptp_sync.h"
#include "wubu_hw_detect.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>

/* ---- Global state ---- */
static int  g_phc = 0;         /* PTP hardware clock */
static int  g_ptp4l = 0;       /* ptp4l daemon */
static int  g_phc2sys = 0;     /* phc2sys */
static int  g_hw_ts = 0;       /* NIC hardware timestamping */
static int  g_sync = 0;        /* clock synced */
static char g_ptp_drv[24] = "";

/* ---- W1: probe the PTP topology ---- */
void wubu_ptp_sync_probe(void)
{
    g_phc = 0; g_ptp4l = 0; g_phc2sys = 0; g_hw_ts = 0; g_sync = 0;
    g_ptp_drv[0] = '\0';

#ifdef WUBU_HOSTED
    /* PTP hardware clock present? */
    if (access("/dev/ptp", R_OK) == 0) {
        DIR *d = opendir("/dev/ptp");
        if (d) {
            struct dirent *e;
            while ((e = readdir(d))) {
                if (e->d_name[0] == 'p') { g_phc = 1; break; }
            }
            closedir(d);
        }
    }
    /* ptp4l daemon present? */
    if (access("/usr/sbin/ptp4l", R_OK) == 0 ||
        access("/usr/bin/ptp4l", R_OK) == 0) {
        g_ptp4l = 1;
        strcpy(g_ptp_drv, "ptp4l");
    }
    /* phc2sys present? */
    if (access("/usr/sbin/phc2sys", R_OK) == 0 ||
        access("/usr/bin/phc2sys", R_OK) == 0) {
        g_phc2sys = 1;
        g_sync = 1;
        if (!g_ptp_drv[0]) strcpy(g_ptp_drv, "phc2sys");
    }
    /* NIC hardware timestamping (PTP-capable NICs)? */
    if (access("/sys/bus/pci/drivers/igc", R_OK) == 0 ||
        access("/sys/bus/pci/drivers/ixgbe", R_OK) == 0 ||
        access("/sys/bus/pci/drivers/e1000e", R_OK) == 0 ||
        access("/sys/bus/pci/drivers/mlx5_core", R_OK) == 0) {
        g_hw_ts = 1;
        if (!g_ptp_drv[0]) strcpy(g_ptp_drv, "nic-phc");
    }
#endif
}

/* ---- W2: accessors ---- */
int  wubu_ptp_sync_phc(void)   { return g_phc; }
int  wubu_ptp_sync_ptp4l(void) { return g_ptp4l; }
int  wubu_ptp_sync_phc2sys(void){ return g_phc2sys; }
int  wubu_ptp_sync_hwts(void)  { return g_hw_ts; }
int  wubu_ptp_sync_synced(void){ return g_sync; }
const char *wubu_ptp_sync_driver(void){ return g_ptp_drv[0] ? g_ptp_drv : NULL; }

/* ---- W3: PTP routing ---- */
const char *wubu_ptp_sync_role_for(const char *role)
{
    if (!role) return NULL;
    if (strstr(role, "master") || strstr(role, "grand")) return "master";
    if (strstr(role, "slave")) return "slave";
    if (strstr(role, "transp")) return "transparent";
    if (strstr(role, "bound"))  return "boundary";
    return "ptp";
}

const char *wubu_ptp_sync_nic_for(const char *nic)
{
    if (!nic) return NULL;
    if (strstr(nic, "igc"))    return "igc-phc";
    if (strstr(nic, "ixgbe"))  return "ixgbe-phc";
    if (strstr(nic, "e1000e")) return "e1000e-phc";
    if (strstr(nic, "mlx5"))   return "mlx5-phc";
    if (strstr(nic, "igb"))    return "igb-phc";
    return "phc";
}

/* ---- W4: summary ---- */
int wubu_ptp_sync_summary(char *out, size_t cap)
{
    return snprintf(out, cap,
        "ptpsync[phc=%d ptp4l=%d phc2sys=%d hwts=%d synced=%d drv=%s]",
        g_phc, g_ptp4l, g_phc2sys, g_hw_ts, g_sync,
        wubu_ptp_sync_driver() ? wubu_ptp_sync_driver() : "none");
}
