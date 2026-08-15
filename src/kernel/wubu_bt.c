/*
 * wubu_bt.c -- kernel-owned Bluetooth driver routing + LE Audio.
 *
 * Bluetooth connects controllers, headsets, audio, keyboards, and IoT.
 * "Runs on everything" includes every BT adapter. The kernel must route
 * the controller (USB/PCI/UART) to the right HCI driver and expose the
 * BlueZ stack. LE Audio (the modern Bluetooth audio, replacing A2DP) runs
 * over ISO channels and needs kernel 6.1+ isochronous support.
 *
 * BT controller drivers (by transport):
 *   - USB: btusb (the universal USB BT driver, handles Intel/Realtek/
 *     Broadcom/Cypress over USB), btrtl (Realtek), btusb + btbcm
 *   - PCI: btintel (Intel Wireless-AC/BE), btbcm (Broadcom), btrtl
 *   - UART: hci_uart, btbcm, btrtl (for internal/BT-UART radios)
 *   - MediaTek: btmtk / btmtksdio
 *
 * LE Audio: iso channels (isoc), iso-tp, BAP (Basic Audio Profile), and
 * Auracast broadcast. Needs BlueZ 5.65+ + kernel isochronous support.
 *
 * WuBuOS owns this: detect the BT controller + transport, route to the
 * right HCI driver, and flag LE Audio capability.
 *
 * Research (Kevin-Bacon 7-hop on the Bluetooth frontier):
 *   - BlueZ stack: bluetooth.ko, btusb.ko, bnep.ko, rfcomm.ko, hidp.ko
 *   - btusb: universal USB; btintel/btbcm/btrtl: vendor HCI
 *   - LE Audio: isoc channels, BAP, Auracast broadcast (kernel 6.1+)
 *   - btmtk: MediaTek (MT7921/MT7922 BT)
 */
#include "wubu_bt.h"
#include "wubu_hw_detect.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

/* ---- Global state ---- */
static int  g_bt = 0;
static int  g_bt_usb = 0;
static int  g_bt_pci = 0;
static int  g_bt_uart = 0;
static int  g_le_audio = 0;
static char g_bt_drv[32] = "";
static char g_bt_vendor[24] = "";

/* ---- W1: probe the Bluetooth topology ---- */
void wubu_bt_probe(void)
{
    g_bt = 0; g_bt_usb = 0; g_bt_pci = 0; g_bt_uart = 0; g_le_audio = 0;
    g_bt_drv[0] = '\0'; g_bt_vendor[0] = '\0';

#ifdef WUBU_HOSTED
    /* BlueZ hci dev present? */
    if (access("/sys/class/bluetooth/hci0", R_OK) == 0) {
        g_bt = 1;
        /* Read the transport/driver from the hci symlink. */
        char link[256];
        ssize_t len = readlink("/sys/class/bluetooth/hci0", link, sizeof(link)-1);
        if (len > 0) {
            link[len] = '\0';
            if (strstr(link, "usb")) { g_bt_usb = 1; strcpy(g_bt_drv, "btusb"); }
            else if (strstr(link, "pci")) { g_bt_pci = 1; strcpy(g_bt_drv, "btintel"); }
            else if (strstr(link, "uart") || strstr(link, "tty")) {
                g_bt_uart = 1; strcpy(g_bt_drv, "hci_uart");
            }
        }
    }

    /* LE Audio (isochronous) support: BlueZ iso sockets. */
    g_le_audio = (access("/sys/kernel/debug/bluetooth/hci0/iso", R_OK) == 0) ||
                 (access("/proc/net/bluetooth/iso", R_OK) == 0);
#endif
}

/* ---- W2: accessors ---- */
int  wubu_bt_present(void)   { return g_bt; }
int  wubu_bt_usb(void)       { return g_bt_usb; }
int  wubu_bt_pci(void)       { return g_bt_pci; }
int  wubu_bt_uart(void)      { return g_bt_uart; }
int  wubu_bt_le_audio(void)  { return g_le_audio; }
const char *wubu_bt_driver(void) { return g_bt_drv[0] ? g_bt_drv : NULL; }

/* ---- W3: BT controller driver routing per vendor ---- */
const char *wubu_bt_controller_driver(const char *vendor)
{
    if (!vendor) return NULL;
    if (strstr(vendor, "intel"))    return "btintel";
    if (strstr(vendor, "broadcom")) return "btbcm";
    if (strstr(vendor, "realtek"))  return "btrtl";
    if (strstr(vendor, "mediatek")) return "btmtk";
    if (strstr(vendor, "cypress"))  return "btbcm";
    return "btusb";
}

/* ---- W4: summary ---- */
int wubu_bt_summary(char *out, size_t cap)
{
    return snprintf(out, cap,
        "bt[present=%d usb=%d pci=%d uart=%d le_audio=%d drv=%s]",
        g_bt, g_bt_usb, g_bt_pci, g_bt_uart, g_le_audio,
        wubu_bt_driver() ? wubu_bt_driver() : "none");
}
