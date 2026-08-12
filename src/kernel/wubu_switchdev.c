/*
 * wubu_switchdev.c -- kernel-owned network switch fabric (switchdev) routing.
 *
 * Managed switches (DSA + switchdev) connect many ports. "Runs on
 * everything" includes switches, routers, and smart NICs with switch ASICs.
 * The kernel routes the switch chip to the right DSA/switchdev driver.
 *
 * Switch drivers:
 *   - Marvell: mv88e6xxx (DSA, the ubiquitous switch chip family)
 *   - Microchip: ksz9477/ksz8795 (ksz_spi/ksz_common)
 *   - Realtek: rtl8366rb (rtl8366rb)
 *   - MediaTek: mt7530 (mt7530, in switch routers)
 *   - Qualcomm: qca8k (qca8k)
 *   - Mellanox/NVIDIA: mlxsw (switch ASIC, Spectrum)
 *   - Broadcom: b53 (DSA, consumer SoC switches)
 *   - Microsemi: felix/ocelot (ocelot_switch)
 *
 * WuBuOS owns this: detect the switch chip (DSA/MDIO), route to the right
 * driver, and expose the switch ports + VLAN topology.
 *
 * Research (Kevin-Bacon 7-hop on the switch frontier):
 *   - switchdev model: drivers/net/ethernet/switchdev (mlxsw, felix)
 *   - DSA (Distributed Switch Architecture): mv88e6xxx, ksz, qca8k, b53
 *   - Router SoC switches: mt7530, qca8k
 *   - mlxsw: Mellanox Spectrum ASIC (100G data center)
 */
#include "wubu_switchdev.h"
#include "wubu_hw_detect.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

/* ---- Global state ---- */
static int  g_switch = 0;
static int  g_dsa = 0;          /* DSA (distributed switch arch) */
static int  g_switchdev = 0;    /* switchdev ASIC */
static char g_switch_drv[32] = "";
static char g_switch_name[32] = "";

/* ---- W1: probe the switch topology ---- */
void wubu_switchdev_probe(void)
{
    g_switch = 0; g_dsa = 0; g_switchdev = 0;
    g_switch_drv[0] = '\0'; g_switch_name[0] = '\0';

#ifdef _GNU_SOURCE
    /* DSA switch drivers present? */
    if (access("/sys/bus/mdio_bus/drivers/mv88e6xxx", R_OK) == 0 ||
        access("/sys/bus/mdio_bus/drivers/ksz9477", R_OK) == 0 ||
        access("/sys/bus/mdio_bus/drivers/qca8k", R_OK) == 0 ||
        access("/sys/bus/mdio_bus/drivers/b53", R_OK) == 0) {
        g_dsa = 1; g_switch = 1;
        if (access("/sys/bus/mdio_bus/drivers/mv88e6xxx", R_OK) == 0) {
            strcpy(g_switch_drv, "mv88e6xxx");
            strcpy(g_switch_name, "Marvell switch");
        } else if (access("/sys/bus/mdio_bus/drivers/ksz9477", R_OK) == 0) {
            strcpy(g_switch_drv, "ksz9477");
            strcpy(g_switch_name, "Microchip KSZ");
        } else if (access("/sys/bus/mdio_bus/drivers/qca8k", R_OK) == 0) {
            strcpy(g_switch_drv, "qca8k");
            strcpy(g_switch_name, "Qualcomm switch");
        } else {
            strcpy(g_switch_drv, "b53");
            strcpy(g_switch_name, "Broadcom switch");
        }
    }
    /* switchdev ASIC present (Mellanox Spectrum, Microsemi ocelot)? */
    if (access("/sys/bus/pci/drivers/mlxsw_spectrum", R_OK) == 0 ||
        access("/sys/bus/pci/drivers/felix", R_OK) == 0) {
        g_switchdev = 1; g_switch = 1;
        if (!g_switch_drv[0]) {
            strcpy(g_switch_drv, "mlxsw");
            strcpy(g_switch_name, "switch ASIC");
        }
    }
#endif
}

/* ---- W2: accessors ---- */
int  wubu_switchdev_present(void)  { return g_switch; }
int  wubu_switchdev_has_dsa(void)  { return g_dsa; }
int  wubu_switchdev_has_asic(void) { return g_switchdev; }
const char *wubu_switchdev_driver(void){ return g_switch_drv[0] ? g_switch_drv : NULL; }
const char *wubu_switchdev_name(void){ return g_switch_name[0] ? g_switch_name : NULL; }

/* ---- W3: switch driver routing ---- */
const char *wubu_switchdev_driver_for(const char *chip)
{
    if (!chip) return NULL;
    if (strstr(chip, "mv88e"))    return "mv88e6xxx";
    if (strstr(chip, "ksz"))      return "ksz_common";
    if (strstr(chip, "rtl8366"))  return "rtl8366rb";
    if (strstr(chip, "mt7530"))   return "mt7530";
    if (strstr(chip, "qca8k"))    return "qca8k";
    if (strstr(chip, "mlxsw") || strstr(chip, "spectrum")) return "mlxsw_spectrum";
    if (strstr(chip, "b53"))      return "b53";
    if (strstr(chip, "ocelot") || strstr(chip, "felix")) return "ocelot_switch";
    return "dsa";
}

/* ---- W4: summary ---- */
int wubu_switchdev_summary(char *out, size_t cap)
{
    return snprintf(out, cap,
        "sw[present=%d dsa=%d asic=%d drv=%s name=%s]",
        g_switch, g_dsa, g_switchdev,
        wubu_switchdev_driver() ? wubu_switchdev_driver() : "none",
        wubu_switchdev_name() ? wubu_switchdev_name() : "-");
}
