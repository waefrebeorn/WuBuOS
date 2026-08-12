/*
 * wubu_qos.c -- kernel-owned Ethernet switch QoS/ACL (tc offload) routing.
 *
 * Modern NICs + switch ASICs offload QoS + ACL to hardware via tc
 * (traffic control): flower filter offload, rate shaping, policing,
 * DSCP marking, ECN. "Runs on everything" includes hardware QoS.
 *
 * QoS/ACL offload:
 *   - tc flower: match on IP/ports/flow -> offload (hw_tc)
 *   - Rate shaping: htb/tbf/sfq (shaper qdiscs)
 *   - Policing: police/ingress policing
 *   - DSCP/priority marking: prio, RED/ECN (explicit congestion)
 *   - switchdev: mlxsw (Mellanox), ocelot, felix offload tc
 *
 * WuBuOS owns this: detect the QoS/ACL offload capability, route to the
 * right driver, and expose the QoS topology.
 *
 * Research (Kevin-Bacon 7-hop on the QoS/ACL frontier):
 *   - tc: flower (match), htb (rate), police, RED/ECN, prio
 *   - switchdev: mlxsw, ocelot offload tc filters to the ASIC
 *   - hw_tc: ethtool -k hw-tc-offload
 *   - clsact: the modern ingress/egress filter hook
 */
#include "wubu_qos.h"
#include "wubu_hw_detect.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

/* ---- Global state ---- */
static int  g_tc = 0;           /* tc available */
static int  g_offload = 0;      /* hw tc offload */
static int  g_flower = 0;       /* flower filter */
static int  g_shaping = 0;      /* rate shaping */
static int  g_ecn = 0;          /* ECN/RED */
static char g_qos_drv[24] = "";

/* ---- W1: probe the QoS/ACL topology ---- */
void wubu_qos_probe(void)
{
    g_tc = 0; g_offload = 0; g_flower = 0; g_shaping = 0; g_ecn = 0;
    g_qos_drv[0] = '\0';

#ifdef _GNU_SOURCE
    /* tc available (iproute2)? */
    if (access("/sbin/tc", R_OK) == 0 || access("/usr/sbin/tc", R_OK) == 0) {
        g_tc = 1;
        strcpy(g_qos_drv, "tc");
    }
    /* hw tc offload: switchdev ASIC drivers. */
    if (access("/sys/bus/pci/drivers/mlxsw_spectrum", R_OK) == 0 ||
        access("/sys/bus/pci/drivers/felix", R_OK) == 0) {
        g_offload = 1;
        g_flower = 1;
        if (access("/sys/bus/pci/drivers/mlxsw_spectrum", R_OK) == 0)
            strcpy(g_qos_drv, "mlxsw");
        else
            strcpy(g_qos_drv, "felix");
    }
    /* rate shaping / qdisc present (htb/tbf)? */
    if (access("/proc/net/psched", R_OK) == 0) {
        g_shaping = 1;
    }
    /* ECN (sysctl tcp_ecn). */
    if (access("/proc/sys/net/ipv4/tcp_ecn", R_OK) == 0) {
        g_ecn = 1;
    }
    if (!g_qos_drv[0]) strcpy(g_qos_drv, "net-core");
#endif
}

/* ---- W2: accessors ---- */
int  wubu_qos_tc(void)         { return g_tc; }
int  wubu_qos_offload(void)    { return g_offload; }
int  wubu_qos_flower(void)     { return g_flower; }
int  wubu_qos_shaping(void)    { return g_shaping; }
int  wubu_qos_ecn(void)        { return g_ecn; }
const char *wubu_qos_driver(void){ return g_qos_drv[0] ? g_qos_drv : NULL; }

/* ---- W3: QoS driver routing ---- */
const char *wubu_qos_driver_for(const char *hw)
{
    if (!hw) return NULL;
    if (strstr(hw, "mlxsw") || strstr(hw, "spectrum")) return "mlxsw";
    if (strstr(hw, "felix") || strstr(hw, "ocelot")) return "felix";
    if (strstr(hw, "mv88e") || strstr(hw, "marvell")) return "mv88e6xxx";
    if (strstr(hw, "ksz"))   return "ksz";
    return "net-core";
}

/* ---- W4: summary ---- */
int wubu_qos_summary(char *out, size_t cap)
{
    return snprintf(out, cap,
        "qos[tc=%d offload=%d flower=%d shaping=%d ecn=%d drv=%s]",
        g_tc, g_offload, g_flower, g_shaping, g_ecn,
        wubu_qos_driver() ? wubu_qos_driver() : "none");
}
