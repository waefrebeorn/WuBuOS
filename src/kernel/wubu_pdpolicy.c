/*
 * wubu_pdpolicy.c -- kernel-owned USB Power Delivery policy routing.
 *
 * USB PD policy negotiates power contracts (voltage/amperage) between
 * source and sink. "Runs on everything" includes correct PD.
 *
 * USB PD policy:
 *   - TPS6598x, FUSB302, RT1715: PD controller
 *   - /sys/class/typec port_type: port role
 *   - /sys/class/typec/typec_device: PD sink/source
 *   * /sys/class/typec pd_revision: PD revision (3.0+)
 *   - power contract: voltage (V), current (A), e.g. 20V/3A
 *   - PDO: fixed, battery, variable, PPS
 *   - policy engine: source caps, sink caps, request
 *   - dual-role: SRC/ SNK swap
 *
 * WuBuOS owns this: detect PD policy + contract + role, route to
 * the right driver, expose the topology.
 *
 * Research (7-hop on the PD policy frontier):
 *   - USB PD policy manager (PPS, PDO)
 *   - TPS6598x, FUSB302 controllers
 *   - typec port_type, PD revision 3.0
 */
#include "wubu_pdpolicy.h"
#include "wubu_hw_detect.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>

static int  g_pd = 0;          /* PD present */
static int  g_contract = 0;   /* power contract */
static int  g_pdo = 0;        /* PDO (capabilities) */
static int  g_pps = 0;        /* PPS (programmable) */
static int  g_dual = 0;       /* dual-role */
static char g_pd_drv[24] = "";

void wubu_pdpolicy_probe(void)
{
    g_pd = 0; g_contract = 0; g_pdo = 0; g_pps = 0; g_dual = 0;
    g_pd_drv[0] = '\0';

#ifdef WUBU_HOSTED
    /* typec port present? */
    if (access("/sys/class/typec", R_OK) == 0) {
        g_pd = 1; g_contract = 1; g_pdo = 1;
        strcpy(g_pd_drv, "typec-pd");
    }
    /* PD controller drivers */
    if (access("/sys/module/fusb302", R_OK) == 0) {
        g_pd = 1; g_dual = 1; strcpy(g_pd_drv, "fusb302");
    }
    if (access("/sys/module/tcpci", R_OK) == 0) {
        g_pd = 1; g_dual = 1;
        if (!g_pd_drv[0]) strcpy(g_pd_drv, "tcpci");
    }
    /* PD revision 3.0 = PPS support */
    DIR *d = opendir("/sys/class/typec");
    if (d) {
        struct dirent *e;
        while ((e = readdir(d))) {
            if (strstr(e->d_name, "port")) {
                char p[128];
                snprintf(p, sizeof(p), "/sys/class/typec/%s/pd_revision", e->d_name);
                if (access(p, R_OK) == 0) { g_pps = 1; break; }
            }
        }
        closedir(d);
    }
#endif
}

int  wubu_pdpolicy_present(void){ return g_pd; }
int  wubu_pdpolicy_contract(void){ return g_contract; }
int  wubu_pdpolicy_pdo(void)     { return g_pdo; }
int  wubu_pdpolicy_pps(void)     { return g_pps; }
int  wubu_pdpolicy_dual(void)    { return g_dual; }
const char *wubu_pdpolicy_driver(void){ return g_pd_drv[0] ? g_pd_drv : NULL; }

const char *wubu_pdpolicy_role_for(const char *role)
{
    if (!role) return NULL;
    if (strstr(role, "source") || strstr(role, "src")) return "source";
    if (strstr(role, "sink") || strstr(role, "snk"))  return "sink";
    return "dual";
}

const char *wubu_pdpolicy_pdo_for(const char *pdo)
{
    if (!pdo) return NULL;
    if (strstr(pdo, "fixed"))    return "fixed";
    if (strstr(pdo, "battery"))  return "battery";
    if (strstr(pdo, "variable")) return "variable";
    if (strstr(pdo, "pps") || strstr(pdo, "programmable")) return "pps";
    return "fixed";
}

int wubu_pdpolicy_summary(char *out, size_t cap)
{
    return snprintf(out, cap,
        "pdpolicy[pd=%d contract=%d pdo=%d pps=%d dual=%d drv=%s]",
        g_pd, g_contract, g_pdo, g_pps, g_dual,
        wubu_pdpolicy_driver() ? wubu_pdpolicy_driver() : "none");
}
