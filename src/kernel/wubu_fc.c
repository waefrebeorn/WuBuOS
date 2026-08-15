/*
 * wubu_fc.c -- kernel-owned ethernet flow control (pause) routing.
 *
 * Flow control uses pause frames to stop the sender when the RX buffer
 * overflows. "Runs on everything" includes correct link flow control.
 *
 * Flow control:
 *   - pause frames: 802.3x MAC control PAUSE
 *   - ethtool -A: autoneg/rx/tx pause settings
 *   - priority-based flow control (PFC): 802.1Qbb
 *   - link-level flow control: half/full duplex
 *
 * WuBuOS owns this: detect flow-control capability (pause + PFC), route
 * to the right driver, and expose the topology.
 *
 * Research (Kevin-Bacon 7-hop on the ethernet-FC frontier):
 *   - 802.3x PAUSE frames (ethtool -A)
 *   - 802.1Qbb PFC (priority-based)
 *   - autonegotiation of pause
 */
#include "wubu_fc.h"
#include "wubu_hw_detect.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

/* ---- Global state ---- */
static int  g_fc = 0;          /* flow control */
static int  g_pause = 0;       /* pause frames */
static int  g_pfc = 0;         /* PFC */
static int  g_autoneg = 0;     /* pause autoneg */
static int  g_ethtool = 0;     /* ethtool */
static char g_fc_drv[24] = "";

/* ---- W1: probe the FC topology ---- */
void wubu_fc_probe(void)
{
    g_fc = 0; g_pause = 0; g_pfc = 0; g_autoneg = 0; g_ethtool = 0;
    g_fc_drv[0] = '\0';

#ifdef WUBU_HOSTED
    /* NIC present? */
    if (access("/sys/class/net", R_OK) == 0) {
        g_fc = 1;
        strcpy(g_fc_drv, "pause");
        /* autoneg of pause (ethtool) */
        if (access("/usr/sbin/ethtool", R_OK) == 0 ||
            access("/usr/bin/ethtool", R_OK) == 0) {
            g_ethtool = 1;
            g_autoneg = 1;
            g_pause = 1;
        }
    }
    /* PFC (802.1Qbb) present? */
    if (access("/sys/bus/pci/drivers/mlx5_core", R_OK) == 0 ||
        access("/sys/bus/pci/drivers/ixgbe", R_OK) == 0 ||
        access("/sys/bus/pci/drivers/i40e", R_OK) == 0) {
        g_pfc = 1;
        if (!g_fc_drv[0]) strcpy(g_fc_drv, "pfc");
    }
#endif
}

/* ---- W2: accessors ---- */
int  wubu_fc_supported(void){ return g_fc; }
int  wubu_fc_pause(void)    { return g_pause; }
int  wubu_fc_pfc(void)      { return g_pfc; }
int  wubu_fc_autoneg(void)  { return g_autoneg; }
int  wubu_fc_ethtool(void)  { return g_ethtool; }
const char *wubu_fc_driver(void){ return g_fc_drv[0] ? g_fc_drv : NULL; }

/* ---- W3: FC routing ---- */
const char *wubu_fc_mode_for(const char *mode)
{
    if (!mode) return NULL;
    if (strstr(mode, "rx"))    return "rx-pause";
    if (strstr(mode, "tx"))    return "tx-pause";
    if (strstr(mode, "both"))  return "both-pause";
    if (strstr(mode, "pfc"))   return "pfc";
    return "fc";
}

const char *wubu_fc_autoneg_for(const char *mode)
{
    if (!mode) return NULL;
    if (strstr(mode, "on"))    return "on";
    if (strstr(mode, "off"))   return "off";
    return "auto";
}

/* ---- W4: summary ---- */
int wubu_fc_summary(char *out, size_t cap)
{
    return snprintf(out, cap,
        "fc[fc=%d pause=%d pfc=%d autoneg=%d ethtool=%d drv=%s]",
        g_fc, g_pause, g_pfc, g_autoneg, g_ethtool,
        wubu_fc_driver() ? wubu_fc_driver() : "none");
}
