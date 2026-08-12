/*
 * wubu_multigig.c -- kernel-owned Ethernet multi-gig (2.5/5/10G) PHY routing.
 *
 * Beyond gigabit: 2.5GBase-T, 5GBase-T, 10GBase-T, NBASE-T rate
 * adaptation. "Runs on everything" includes the modern multi-gig PHYs.
 *
 * Multi-gig PHY drivers:
 *   - Realtek: r8125 (2.5G), rtl8126 (2.5G), r8168 (older)
 *   - Marvell: aqr107/aqr113 (Aquantia 10G), m88x (88X3310)
 *   - Aquantia: aqr (10G NBASE-T), aqr107
 *   - Broadcom: bcm84881, bcm5482
 *   - Intel: x550 (i354 2.5G integrated)
 *   - 10GBase-T: xgmac, atlantic (Marvell AQtion AQC113)
 *
 * WuBuOS owns this: detect the multi-gig PHY, route to the right driver,
 * and expose the max link rate (2.5/5/10G).
 *
 * Research (Kevin-Bacon 7-hop on the multi-gig frontier):
 *   - r8125: Realtek 2.5G (the DKMS headache), NBASE-T
 *   - aqr107/aqr113: Aquantia/Marvell 10G NBASE-T
 *   - atlantic: Marvell AQC113 10G
 *   - x550: Intel 2.5G integrated
 *   - 802.3bz: 2.5G/5GBase-T (NBASE-T) standard
 */
#include "wubu_multigig.h"
#include "wubu_hw_detect.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

/* ---- Global state ---- */
static int  g_multigig = 0;
static int  g_2g5 = 0;
static int  g_5g = 0;
static int  g_10g = 0;
static char g_mg_drv[24] = "";
static char g_mg_name[24] = "";

/* ---- W1: probe the multi-gig topology ---- */
void wubu_multigig_probe(void)
{
    g_multigig = 0; g_2g5 = 0; g_5g = 0; g_10g = 0;
    g_mg_drv[0] = '\0'; g_mg_name[0] = '\0';

#ifdef _GNU_SOURCE
    /* Realtek 2.5G (r8125) present? */
    if (access("/sys/bus/pci/drivers/r8125", R_OK) == 0) {
        g_multigig = 1; g_2g5 = 1;
        strcpy(g_mg_drv, "r8125"); strcpy(g_mg_name, "Realtek 2.5G");
    }
    /* Aquantia/Marvell 10G (aqr107/atlantic) present? */
    if (access("/sys/bus/pci/drivers/atlantic", R_OK) == 0) {
        g_multigig = 1; g_10g = 1;
        if (!g_mg_drv[0]) { strcpy(g_mg_drv, "atlantic"); strcpy(g_mg_name, "Marvell 10G"); }
    }
    if (access("/sys/bus/mdio_bus/drivers/aquantia", R_OK) == 0) {
        g_multigig = 1; g_10g = 1;
        if (!g_mg_drv[0]) { strcpy(g_mg_drv, "aquantia"); strcpy(g_mg_name, "Aquantia 10G"); }
    }
    /* Intel x550 2.5G integrated? */
    if (access("/sys/bus/pci/drivers/x550", R_OK) == 0) {
        g_multigig = 1; g_2g5 = 1;
        if (!g_mg_drv[0]) { strcpy(g_mg_drv, "ixgbe"); strcpy(g_mg_name, "Intel 2.5G"); }
    }
    /* Broadcom 10G PHY? */
    if (access("/sys/bus/mdio_bus/drivers/bcm84881", R_OK) == 0) {
        g_multigig = 1; g_10g = 1;
        if (!g_mg_drv[0]) { strcpy(g_mg_drv, "bcm84881"); strcpy(g_mg_name, "Broadcom 10G"); }
    }
#endif
}

/* ---- W2: accessors ---- */
int  wubu_multigig_present(void){ return g_multigig; }
int  wubu_multigig_2g5(void)    { return g_2g5; }
int  wubu_multigig_5g(void)     { return g_5g; }
int  wubu_multigig_10g(void)    { return g_10g; }
const char *wubu_multigig_driver(void){ return g_mg_drv[0] ? g_mg_drv : NULL; }
const char *wubu_multigig_name(void){ return g_mg_name[0] ? g_mg_name : NULL; }

/* ---- W3: multi-gig driver routing ---- */
const char *wubu_multigig_driver_for(const char *phy)
{
    if (!phy) return NULL;
    if (strstr(phy, "r8125") || strstr(phy, "rtl8126")) return "r8125";
    if (strstr(phy, "aqr") || strstr(phy, "aquantia"))  return "aquantia";
    if (strstr(phy, "atlantic") || strstr(phy, "aqc113")) return "atlantic";
    if (strstr(phy, "m88x") || strstr(phy, "marvell"))  return "m88x3310";
    if (strstr(phy, "bcm848")) return "bcm84881";
    if (strstr(phy, "x550"))   return "ixgbe";
    if (strstr(phy, "r8168"))  return "r8168";
    return "genphy";
}

/* ---- W4: summary ---- */
int wubu_multigig_summary(char *out, size_t cap)
{
    return snprintf(out, cap,
        "mgig[present=%d 2g5=%d 5g=%d 10g=%d drv=%s name=%s]",
        g_multigig, g_2g5, g_5g, g_10g,
        wubu_multigig_driver() ? wubu_multigig_driver() : "none",
        wubu_multigig_name() ? wubu_multigig_name() : "-");
}
