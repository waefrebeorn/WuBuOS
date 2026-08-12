/*
 * wubu_bus.c -- kernel-owned I2C/SPI bus controller driver routing.
 *
 * I2C and SPI are the two ubiquitous on-board buses connecting sensors,
 * codecs, RTCs, EEPROMs, and touch controllers. "Runs on everything"
 * includes the bus controller chips that drive them.
 *
 * I2C controller drivers:
 *   - Intel: i2c-piix4 (SMBus), i2c-designware (Intel/AMD)
 *   - NXP/Freescale: i2c-imx
 *   - Broadcom: i2c-bcm2835, brcmstb
 *   - Qualcomm: i2c-qcom-geni, i2c-qup
 *   - Nvidia: i2c-tegra
 *   - AMD: i2c-amd-pci, i2c-designware
 *   - Atmel: i2c-at91
 *
 * SPI controller drivers:
 *   - NXP/Freescale: spi-imx
 *   - Marvell: spi-orion
 *   - Broadcom: spi-bcm2835, spi-bcm63xx
 *   - Nvidia: spi-tegra
 *   - Qualcomm: spi-qcom-qspi
 *   - Intel: spi-pxa2xx
 *
 * WuBuOS owns this: detect the I2C/SPI bus controllers (PCI/ACPI/DT),
 * route to the right driver, and expose the bus topology.
 *
 * Research (Kevin-Bacon 7-hop on the bus-controller frontier):
 *   - I2C: i2c-piix4 (SMBus), i2c-designware (Intel/AMD),
 *     i2c-imx, i2c-bcm2835, i2c-qcom-geni
 *   - SPI: spi-orion, spi-bcm2835, spi-imx, spi-tegra
 *   - i2c-core, spi-core subsystems
 */
#include "wubu_bus.h"
#include "wubu_pci.h"
#include "wubu_hw_detect.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

/* ---- PCI class: serial bus ---- */
#define PCI_CLASS_SERIAL   0x0C
#define PCI_SUBCLASS_I2C   0x05   /* SMBus / I2C controller */
#define PCI_VENDOR_INTEL   0x8086
#define PCI_VENDOR_AMD     0x1022

/* ---- Global state ---- */
static int  g_i2c = 0;
static int  g_spi = 0;
static int  g_i2c_controllers = 0;
static int  g_spi_controllers = 0;
static char g_i2c_drv[32] = "";
static char g_spi_drv[32] = "";

/* ---- W1: probe the I2C/SPI bus topology ---- */
void wubu_bus_probe(void)
{
    g_i2c = 0; g_spi = 0; g_i2c_controllers = 0; g_spi_controllers = 0;
    g_i2c_drv[0] = '\0'; g_spi_drv[0] = '\0';

#ifdef _GNU_SOURCE
    /* I2C controllers (i2c-N buses) present? */
    if (access("/sys/bus/i2c/devices", R_OK) == 0) {
        g_i2c = 1;
        for (int i = 0; i < 16; i++) {
            char p[64];
            snprintf(p, sizeof(p), "/sys/bus/i2c/devices/i2c-%d", i);
            if (access(p, R_OK) == 0) g_i2c_controllers++;
        }
        if (access("/sys/bus/pci/drivers/i2c-piix4", R_OK) == 0)
            strcpy(g_i2c_drv, "i2c-piix4");
        else if (access("/sys/bus/pci/drivers/i2c-designware-pci", R_OK) == 0)
            strcpy(g_i2c_drv, "i2c-designware");
        else
            strcpy(g_i2c_drv, "i2c-core");
    }
    /* SPI controllers present? */
    if (access("/sys/bus/spi/devices", R_OK) == 0) {
        g_spi = 1;
        for (int i = 0; i < 16; i++) {
            char p[64];
            snprintf(p, sizeof(p), "/sys/bus/spi/devices/spi%d", i);
            if (access(p, R_OK) == 0) g_spi_controllers++;
        }
        if (access("/sys/bus/platform/drivers/spi-orion", R_OK) == 0)
            strcpy(g_spi_drv, "spi-orion");
        else if (access("/sys/bus/platform/drivers/spi-imx", R_OK) == 0)
            strcpy(g_spi_drv, "spi-imx");
        else if (access("/sys/bus/platform/drivers/spi-bcm2835", R_OK) == 0)
            strcpy(g_spi_drv, "spi-bcm2835");
        else
            strcpy(g_spi_drv, "spi-core");
    }
#endif
}

/* ---- W2: accessors ---- */
int  wubu_bus_has_i2c(void)  { return g_i2c; }
int  wubu_bus_has_spi(void)  { return g_spi; }
int  wubu_bus_i2c_controllers(void){ return g_i2c_controllers; }
int  wubu_bus_spi_controllers(void){ return g_spi_controllers; }
const char *wubu_bus_i2c_driver(void){ return g_i2c_drv[0] ? g_i2c_drv : NULL; }
const char *wubu_bus_spi_driver(void){ return g_spi_drv[0] ? g_spi_drv : NULL; }

/* ---- W3: bus controller driver routing ---- */
const char *wubu_bus_i2c_driver_for(const char *ctrl)
{
    if (!ctrl) return NULL;
    if (strstr(ctrl, "piix4"))  return "i2c-piix4";
    if (strstr(ctrl, "designware") || strstr(ctrl, "dw")) return "i2c-designware";
    if (strstr(ctrl, "imx"))    return "i2c-imx";
    if (strstr(ctrl, "bcm2835") || strstr(ctrl, "brcm")) return "i2c-bcm2835";
    if (strstr(ctrl, "qcom") || strstr(ctrl, "geni")) return "i2c-qcom-geni";
    if (strstr(ctrl, "tegra"))  return "i2c-tegra";
    return "i2c-core";
}

const char *wubu_bus_spi_driver_for(const char *ctrl)
{
    if (!ctrl) return NULL;
    if (strstr(ctrl, "orion"))  return "spi-orion";
    if (strstr(ctrl, "imx"))    return "spi-imx";
    if (strstr(ctrl, "bcm2835"))return "spi-bcm2835";
    if (strstr(ctrl, "tegra"))  return "spi-tegra";
    if (strstr(ctrl, "qcom"))   return "spi-qcom-qspi";
    if (strstr(ctrl, "pxa2xx")) return "spi-pxa2xx";
    return "spi-core";
}

/* ---- W4: summary ---- */
int wubu_bus_summary(char *out, size_t cap)
{
    return snprintf(out, cap,
        "bus[i2c=%d(%d/%s) spi=%d(%d/%s)]",
        g_i2c, g_i2c_controllers,
        wubu_bus_i2c_driver() ? wubu_bus_i2c_driver() : "none",
        g_spi, g_spi_controllers,
        wubu_bus_spi_driver() ? wubu_bus_spi_driver() : "none");
}
