/*
 * wubu_pd.c -- kernel-owned USB Power Delivery + NIC flow steering routing.
 *
 * Two capabilities:
 *   - USB PD (power delivery): USB-C charging (5V-48V, up to 240W), the
 *     Type-C port manager (TCPM) negotiates power contracts.
 *   - NIC flow steering: RFS/aRFS steer flows to CPUs (per-flow affinity).
 *
 * USB PD:
 *   - tcpm: Type-C port manager (typec.ko, tcpm.ko)
 *   - PD contract: PDO/RDO negotiation, source/sink roles
 *   - /sys/class/typec: port, partner, power role
 *   - charger: bq25710, tcpm psy (power supply)
 *
 * Flow steering:
 *   - RFS (receive flow steering): hash + CPU affinity
 *   - aRFS (accelerated RFS): NIC offload of flow->CPU
 *   - ethtool -N: flow rules; rx-flow-hash
 *
 * WuBuOS owns this: detect USB PD capability + flow-steering support,
 * route to the right driver, and expose the topology.
 *
 * Research (Kevin-Bacon 7-hop on the PD/flow-steering frontier):
 *   - typec/tcpm: Type-C port manager, PD contract negotiation
 *   - PDO/RDO: power delivery objects (5-240W)
 *   - RFS/aRFS: receive flow steering (networking scaling.rst)
 *   - ethtool -N: NIC flow rules
 */
#include "wubu_pd.h"
#include "wubu_hw_detect.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

/* ---- Global state ---- */
static int  g_typec = 0;
static int  g_pd = 0;           /* power delivery */
static int  g_tcpm = 0;
static int  g_rfs = 0;          /* receive flow steering */
static int  g_arfs = 0;         /* accelerated RFS */
static char g_pd_drv[24] = "";

/* ---- W1: probe the USB-PD/flow-steering topology ---- */
void wubu_pd_probe(void)
{
    g_typec = 0; g_pd = 0; g_tcpm = 0; g_rfs = 0; g_arfs = 0;
    g_pd_drv[0] = '\0';

#ifdef _GNU_SOURCE
    /* Type-C port manager present? */
    if (access("/sys/class/typec", R_OK) == 0 ||
        access("/sys/module/typec", R_OK) == 0) {
        g_typec = 1;
        strcpy(g_pd_drv, "typec");
    }
    /* tcpm (PD negotiation)? */
    if (access("/sys/bus/platform/drivers/tcpm", R_OK) == 0 ||
        access("/sys/module/tcpm", R_OK) == 0) {
        g_tcpm = 1; g_pd = 1;
        strcpy(g_pd_drv, "tcpm");
    }
    /* PD power supply (charger) present? */
    if (access("/sys/module/tcpm_psy", R_OK) == 0 ||
        access("/sys/class/power_supply", R_OK) == 0) {
        g_pd = 1;
    }
    /* RFS (networking scaling)? */
    if (access("/proc/sys/net/core", R_OK) == 0) {
        g_rfs = 1;
    }
    /* aRFS: NICs with flow steering (ixgbe/i40e/mlx5). */
    if (access("/sys/bus/pci/drivers/ixgbe", R_OK) == 0 ||
        access("/sys/bus/pci/drivers/i40e", R_OK) == 0 ||
        access("/sys/bus/pci/drivers/mlx5_core", R_OK) == 0) {
        g_arfs = 1;
    }
#endif
}

/* ---- W2: accessors ---- */
int  wubu_pd_typec(void)      { return g_typec; }
int  wubu_pd_supported(void)  { return g_pd; }
int  wubu_pd_tcpm(void)       { return g_tcpm; }
int  wubu_pd_rfs(void)        { return g_rfs; }
int  wubu_pd_arfs(void)       { return g_arfs; }
const char *wubu_pd_driver(void){ return g_pd_drv[0] ? g_pd_drv : NULL; }

/* ---- W3: routing ---- */
const char *wubu_pd_contract_for(const char *role)
{
    if (!role) return NULL;
    if (strstr(role, "source")) return "source";
    if (strstr(role, "sink"))   return "sink";
    if (strstr(role, "dual"))   return "dual-role";
    return "pd";
}

const char *wubu_pd_flow_for(const char *nic)
{
    if (!nic) return NULL;
    if (strstr(nic, "ixgbe")) return "ixgbe-arfs";
    if (strstr(nic, "i40e"))  return "i40e-arfs";
    if (strstr(nic, "mlx5"))  return "mlx5-arfs";
    if (strstr(nic, "igc"))   return "igc-rfs";
    return "rfs";
}

/* ---- W4: summary ---- */
int wubu_pd_summary(char *out, size_t cap)
{
    return snprintf(out, cap,
        "pd[typec=%d pd=%d tcpm=%d rfs=%d arfs=%d drv=%s]",
        g_typec, g_pd, g_tcpm, g_rfs, g_arfs,
        wubu_pd_driver() ? wubu_pd_driver() : "none");
}
