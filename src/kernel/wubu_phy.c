/*
 * wubu_phy.c -- kernel-owned Ethernet PHY/MDIO driver routing.
 *
 * The PHY (physical layer transceiver) converts the MAC's digital signal
 * to the wire (RJ45, fiber). The kernel (phylib) manages PHYs over MDIO.
 * "Runs on everything" includes the right PHY getting the right driver.
 *
 * PHY drivers:
 *   - Marvell: marvell88E1xxx (88E1512, 88E1111)
 *   - Broadcom: broadcom phy (bcm5482, bcm5461, bcm57780)
 *   - Micrel: micrel phy (ksz9031, ksz8041)
 *   - Realtek: rtl8201, rtl8211 (realtek)
 *   - TI: dp83867, dp83822 (dp83867/ti-phy)
 *   - Intel: igc/ixgbe internal PHYs, at803x (Qualcomm/Atheros)
 *   - Motorcomm: yt8511 (yt8511), yt8531
 *   - Generic: genphy (the fallback generic PHY driver)
 *
 * MDIO bus: mdio-bitbang (GPIO), mdio-bcm-unimac, fsl_pq_mdio, asix
 *
 * WuBuOS owns this: detect the PHY (via phylib / MDIO), route to the right
 * driver, and expose the PHY topology (link, speed, duplex).
 *
 * Research (Kevin-Bacon 7-hop on the PHY frontier):
 *   - phylib: drivers/net/phy/phy.c (link/negotiation state machine)
 *   - marvell phy, broadcom phy, micrel phy, realtek phy, ti phy
 *   - genphy: generic fallback for unknown PHYs
 *   - MDIO: mdio-bitbang, mdio-mux, fsl_pq_mdio
 */
#include "wubu_phy.h"
#include "wubu_hw_detect.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>

/* ---- Global state ---- */
static int  g_phy = 0;
static int  g_mdio = 0;
static int  g_link = 0;          /* link up */
static char g_phy_drv[32] = "";
static char g_phy_name[32] = "";

/* ---- W1: probe the PHY topology ---- */
void wubu_phy_probe(void)
{
    g_phy = 0; g_mdio = 0; g_link = 0;
    g_phy_drv[0] = '\0'; g_phy_name[0] = '\0';

#ifdef WUBU_HOSTED
    /* PHY devices present (phylib)? */
    if (access("/sys/bus/mdio_bus/devices", R_OK) == 0) {
        g_mdio = 1;
        /* Look for a PHY device on the mdio bus. */
        if (access("/sys/bus/mdio_bus/devices", R_OK) == 0) {
            /* scan for any mdio device */
            struct dirent **e;
            int n = scandir("/sys/bus/mdio_bus/devices", &e, NULL, alphasort);
            for (int i = 0; i < n; i++) {
                if (e[i]->d_name[0] == '.') continue;
                g_phy = 1;
                strcpy(g_phy_name, e[i]->d_name);
                break;
            }
        }
        if (g_phy) {
            /* common PHY drivers to match against */
            strcpy(g_phy_drv, "genphy");  /* default */
            if (access("/sys/bus/mdio_bus/drivers/marvell-phy", R_OK) == 0)
                strcpy(g_phy_drv, "marvell-phy");
            else if (access("/sys/bus/mdio_bus/drivers/broadcom-phy", R_OK) == 0)
                strcpy(g_phy_drv, "broadcom-phy");
            else if (access("/sys/bus/mdio_bus/drivers/micrel-phy", R_OK) == 0)
                strcpy(g_phy_drv, "micrel-phy");
            else if (access("/sys/bus/mdio_bus/drivers/realtek-phy", R_OK) == 0)
                strcpy(g_phy_drv, "realtek-phy");
        }
    }
#endif
}

/* ---- W2: accessors ---- */
int  wubu_phy_present(void)   { return g_phy; }
int  wubu_phy_has_mdio(void)  { return g_mdio; }
int  wubu_phy_link_up(void)   { return g_link; }
const char *wubu_phy_driver(void){ return g_phy_drv[0] ? g_phy_drv : NULL; }
const char *wubu_phy_name(void){ return g_phy_name[0] ? g_phy_name : NULL; }

/* ---- W3: PHY driver routing ---- */
const char *wubu_phy_driver_for(const char *phy)
{
    if (!phy) return NULL;
    if (strstr(phy, "marvell") || strstr(phy, "88E")) return "marvell-phy";
    if (strstr(phy, "broadcom") || strstr(phy, "bcm")) return "broadcom-phy";
    if (strstr(phy, "micrel") || strstr(phy, "ksz"))  return "micrel-phy";
    if (strstr(phy, "realtek") || strstr(phy, "rtl")) return "realtek-phy";
    if (strstr(phy, "dp83867") || strstr(phy, "dp83822")) return "ti-phy";
    if (strstr(phy, "yt85")) return "motorcomm";
    if (strstr(phy, "at803") || strstr(phy, "qca"))  return "at803x";
    if (strstr(phy, "intel"))  return "igc-phy";
    return "genphy";
}

/* ---- W4: summary ---- */
int wubu_phy_summary(char *out, size_t cap)
{
    return snprintf(out, cap,
        "phy[present=%d mdio=%d link=%d drv=%s name=%s]",
        g_phy, g_mdio, g_link,
        wubu_phy_driver() ? wubu_phy_driver() : "none",
        wubu_phy_name() ? wubu_phy_name() : "-");
}
