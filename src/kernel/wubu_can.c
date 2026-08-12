/*
 * wubu_can.c -- kernel-owned CAN bus driver routing (SocketCAN).
 *
 * CAN (Controller Area Network) is the automotive + industrial control
 * bus. SocketCAN is the Linux implementation. "Runs on everything"
 * includes cars, drones, industrial controllers, and test rigs — all of
 * which speak CAN. The kernel must route the CAN controller to the right
 * driver and expose the SocketCAN netdev.
 *
 * Common CAN controllers:
 *   - mcp251x / mcp2515 (SPI, Microchip) -- the ubiquitous hobby/auto chip
 *   - peak_usb (PCAN-USB, PEAK) -- the lab standard
 *   - esd_usb2 (ESD CAN-USB) -- industrial
 *   - gs_usb (generic SocketCAN USB)
 *   - can327 (ELM327 serial adapter)
 *   - kvaser_usb (Kvaser)
 *   - sja1000 (legacy NXP/Philips, parallel/PCI)
 *
 * WuBuOS owns this: detect the CAN controller (USB/SPI/PCI), route to the
 * right driver, and expose the can0..canN interfaces.
 *
 * Research (Kevin-Bacon 7-hop on the CAN frontier):
 *   - SocketCAN core: can.ko, vcan.ko (virtual CAN for testing), can-raw
 *   - mcp251x: Microchip SPI CAN, the most common DIY/auto chip
 *   - peak_usb: PEAK PCAN-USB, the automotive lab standard
 *   - gs_usb: generic SocketCAN USB adapter (many cheap adapters)
 *   - can327: ELM327 OBD-II serial adapter (car diagnostics)
 *   - sja1000: legacy parallel/PCI CAN
 */
#include "wubu_can.h"
#include "wubu_pci.h"
#include "wubu_hw_detect.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

/* ---- Global state ---- */
static int  g_can_present = 0;
static int  g_can_usb = 0;
static int  g_can_spi = 0;
static int  g_can_pci = 0;
static char g_can_drv[32] = "";

/* ---- W1: probe the CAN topology ---- */
void wubu_can_probe(void)
{
    g_can_present = 0; g_can_usb = 0; g_can_spi = 0; g_can_pci = 0;
    g_can_drv[0] = '\0';

#ifdef _GNU_SOURCE
    /* SocketCAN netdevs present? */
    if (access("/sys/class/net/can0", R_OK) == 0 ||
        access("/sys/class/net/vcan0", R_OK) == 0) {
        g_can_present = 1;
        strcpy(g_can_drv, "socketcan");
    }

    /* USB CAN adapters (PEAK/ESD/GS). */
    if (access("/sys/bus/usb/devices", R_OK) == 0) {
        /* presence of known CAN USB drivers via /sys/bus/usb/drivers */
        if (access("/sys/bus/usb/drivers/peak_usb", R_OK) == 0 ||
            access("/sys/bus/usb/drivers/gs_usb", R_OK) == 0 ||
            access("/sys/bus/usb/drivers/esd_usb2", R_OK) == 0) {
            g_can_usb = 1;
            g_can_present = 1;
            strcpy(g_can_drv, "peak_usb");
        }
    }

    /* SPI CAN (mcp251x) via device-tree / i2c-spi. */
    if (access("/sys/bus/spi/devices", R_OK) == 0) {
        /* mcp251x binds SPI; presence of the SPI bus suggests possible. */
        g_can_spi = 1;  /* conservatively flag the SPI bus for CAN */
    }
#endif
}

/* ---- W2: accessors ---- */
int  wubu_can_present(void)    { return g_can_present; }
int  wubu_can_has_usb(void)    { return g_can_usb; }
int  wubu_can_has_spi(void)    { return g_can_spi; }
int  wubu_can_has_pci(void)    { return g_can_pci; }
const char *wubu_can_driver(void){ return g_can_drv[0] ? g_can_drv : NULL; }

/* ---- W3: controller routing ---- */
const char *wubu_can_controller_driver(const char *chip)
{
    if (!chip) return NULL;
    if (strstr(chip, "mcp2515")) return "mcp251x";
    if (strstr(chip, "mcp2518")) return "mcp251xfd";
    if (strstr(chip, "peak"))    return "peak_usb";
    if (strstr(chip, "esd"))     return "esd_usb2";
    if (strstr(chip, "gs_usb"))  return "gs_usb";
    if (strstr(chip, "elm327"))  return "can327";
    if (strstr(chip, "sja1000")) return "sja1000";
    if (strstr(chip, "kvaser"))  return "kvaser_usb";
    return "socketcan";
}

/* ---- W4: summary ---- */
int wubu_can_summary(char *out, size_t cap)
{
    return snprintf(out, cap,
        "can[present=%d usb=%d spi=%d pci=%d drv=%s]",
        g_can_present, g_can_usb, g_can_spi, g_can_pci,
        wubu_can_driver() ? wubu_can_driver() : "none");
}
