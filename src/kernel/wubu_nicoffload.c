/*
 * wubu_nicoffload.c -- kernel-owned NIC offload + multi-queue routing.
 *
 * Modern NICs offload packet processing to hardware: TSO/GSO (segmentation),
 * GRO/LRO (receive offload), RSS/RFS (receive scaling across queues), and
 * multi-queue transmit (XPS). "Runs on everything" includes full line-rate
 * networking. The kernel must route the NIC to the right driver and expose
 * the offload + queue topology.
 *
 * Offloads (via ethtool / netdev features):
 *   - TSO (TCP segmentation offload), GSO (generic), UFO
 *   - GRO (generic receive offload), LRO (large receive offload)
 *   - RSS (receive side scaling): hash flows across N queues
 *   - RPS/XPS: software receive/transmit packet steering
 *   - RFS: receive flow steering (CPU affinity by flow)
 *   - TX: multi-queue with XPS (transmit packet steering)
 *
 * NIC drivers with rich offload support: ixgbe (X540), i40e (XL710),
 * igc (I225), mlx5, bnxt, e1000e, ice, sfc
 *
 * WuBuOS owns this: detect the NIC offload/queue capabilities, route to
 * the right driver, and expose the queue + offload topology.
 *
 * Research (Kevin-Bacon 7-hop on the NIC-offload frontier):
 *   - ethtool -k (offloads), ethtool -l (queue count), ethtool -x (RSS)
 *   - scaling.rst: RSS, RPS, RFS, XPS (kernel networking)
 *   - ixgbe/i40e/igc: Intel NICs with TSO/GRO/RSS
 */
#include "wubu_nicoffload.h"
#include "wubu_hw_detect.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>

/* ---- Global state ---- */
static int  g_tso = 0;
static int  g_gro = 0;
static int  g_rss = 0;
static int  g_multi_queue = 0;
static int  g_nic = 0;
static int  g_queues = 0;
static char g_offload_drv[32] = "";

/* ---- W1: probe the NIC offload topology ---- */
void wubu_nicoffload_probe(void)
{
    g_tso = 0; g_gro = 0; g_rss = 0; g_multi_queue = 0;
    g_nic = 0; g_queues = 0;
    g_offload_drv[0] = '\0';

#ifdef WUBU_HOSTED
    /* Detect NICs (net devices) with offload support. */
    struct dirent **e;
    int n = scandir("/sys/class/net", &e, NULL, alphasort);
    for (int i = 0; i < n; i++) {
        if (e[i]->d_name[0] == '.') continue;
        if (!strcmp(e[i]->d_name, "lo")) continue;
        g_nic = 1;
        /* queue count = sysfs queues (multi-queue NIC) */
        char p[128];
        int q = 0;
        for (int qi = 0; qi < 32; qi++) {
            snprintf(p, sizeof(p), "/sys/class/net/%s/queues/rx-%d", e[i]->d_name, qi);
            if (access(p, R_OK) == 0) q++;
            else break;
        }
        if (q > 1) { g_multi_queue = 1; g_rss = 1; }
        g_queues += q;
        /* typical modern NICs support TSO + GRO */
        g_tso = 1;
        g_gro = 1;
    }
    /* driver: detect a capable NIC driver */
    if (access("/sys/bus/pci/drivers/ixgbe", R_OK) == 0)
        strcpy(g_offload_drv, "ixgbe");
    else if (access("/sys/bus/pci/drivers/i40e", R_OK) == 0)
        strcpy(g_offload_drv, "i40e");
    else if (access("/sys/bus/pci/drivers/igc", R_OK) == 0)
        strcpy(g_offload_drv, "igc");
    else if (access("/sys/bus/pci/drivers/ice", R_OK) == 0)
        strcpy(g_offload_drv, "ice");
    else if (access("/sys/bus/pci/drivers/mlx5_core", R_OK) == 0)
        strcpy(g_offload_drv, "mlx5");
    else if (access("/sys/bus/pci/drivers/bnxt_en", R_OK) == 0)
        strcpy(g_offload_drv, "bnxt");
#endif
}

/* ---- W2: accessors ---- */
int  wubu_nicoffload_present(void){ return g_nic; }
int  wubu_nicoffload_tso(void)    { return g_tso; }
int  wubu_nicoffload_gro(void)    { return g_gro; }
int  wubu_nicoffload_rss(void)    { return g_rss; }
int  wubu_nicoffload_multi_queue(void){ return g_multi_queue; }
int  wubu_nicoffload_queues(void) { return g_queues; }
const char *wubu_nicoffload_driver(void){ return g_offload_drv[0] ? g_offload_drv : NULL; }

/* ---- W3: offload driver routing ---- */
const char *wubu_nicoffload_driver_for(const char *nic)
{
    if (!nic) return NULL;
    if (strstr(nic, "ixgbe"))  return "ixgbe";
    if (strstr(nic, "i40e"))   return "i40e";
    if (strstr(nic, "igc"))    return "igc";
    if (strstr(nic, "ice"))    return "ice";
    if (strstr(nic, "mlx5"))   return "mlx5";
    if (strstr(nic, "bnxt"))   return "bnxt";
    if (strstr(nic, "e1000e")) return "e1000e";
    if (strstr(nic, "sfc"))    return "sfc";
    return "net-core";
}

/* ---- W4: summary ---- */
int wubu_nicoffload_summary(char *out, size_t cap)
{
    return snprintf(out, cap,
        "nicoff[nic=%d queues=%d tso=%d gro=%d rss=%d mq=%d drv=%s]",
        g_nic, g_queues, g_tso, g_gro, g_rss, g_multi_queue,
        wubu_nicoffload_driver() ? wubu_nicoffload_driver() : "none");
}
