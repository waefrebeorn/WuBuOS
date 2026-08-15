/*
 * wubu_wifi7.c -- kernel-owned Wi-Fi 7 (802.11be) / 6GHz driver routing.
 *
 * Wi-Fi 7 (802.11be) is the current-gen wireless: MLO (multi-link
 * operation), 320MHz channels, 4096-QAM, and the 6GHz band. "Runs on
 * everything" includes the newest wireless hardware. The kernel must route
 * the Wi-Fi 7 card to the right driver and flag MLO/6GHz support.
 *
 * Wi-Fi 7 drivers (by vendor):
 *   - Intel: BE200/BE201 (iwlwifi with 802.11be + MLO), BE202
 *   - Qualcomm: FastConnect 7800 / WCN7850 (ath12k_pci)
 *   - MediaTek: MT7925 (mt7925e / mt76)
 *   - Realtek: RTL8922 (rtw89), RTL8852C (rtw89)
 *   - Broadcom: BCM4389 (brcmfmac, mostly Apple/embedded)
 *
 * WuBuOS owns this: detect the Wi-Fi 7 card (PCI), route to the right
 * driver, and flag MLO + 6GHz capability.
 *
 * Research (Kevin-Bacon 7-hop on the Wi-Fi 7 frontier):
 *   - 802.11be: MLO (multi-link), 320MHz, 4096-QAM, 6GHz band
 *   - Intel BE200: iwlwifi (iwlwifi-mvm), 802.11be + MLO
 *   - Qualcomm FastConnect 7800 (WCN7850): ath12k_pci
 *   - MediaTek MT7925: mt7925e (mt76 family)
 *   - Realtek RTL8922/RTL8852C: rtw89
 *   - ath12k missing in some kernels (Ubuntu 24.04) = the gap
 */
#include "wubu_wifi7.h"
#include "wubu_pci.h"
#include "wubu_hw_detect.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

/* ---- PCI class: network (Wi-Fi) ---- */
#define PCI_CLASS_NETWORK 0x02
#define PCI_SUBCLASS_WIFI 0x80
#define PCI_VENDOR_INTEL    0x8086
#define PCI_VENDOR_QUALCOMM 0x17CB
#define PCI_VENDOR_MEDIATEK 0x14C3
#define PCI_VENDOR_REALTEK  0x10EC
#define PCI_VENDOR_BROADCOM 0x14E4

/* ---- Wi-Fi 7 device IDs ---- */
#define INTEL_BE200  0x2725
#define INTEL_BE201  0x2726
#define QC_WCN7850   0x1107
#define MTK_MT7925   0x0712
#define RTK_RTL8922  0x8922
#define RTK_RTL8852C 0x8852

/* ---- Global state ---- */
static int  g_wifi7 = 0;
static int  g_mlo = 0;         /* MLO (multi-link operation) */
static int  g_band6ghz = 0;
static int  g_320mhz = 0;
static char g_wifi7_drv[24] = "";
static char g_wifi7_name[24] = "";
static int  g_wifi7_vendor = 0;

/* ---- W1: probe the Wi-Fi 7 topology ---- */
void wubu_wifi7_probe(void)
{
    g_wifi7 = 0; g_mlo = 0; g_band6ghz = 0; g_320mhz = 0;
    g_wifi7_vendor = 0;
    g_wifi7_drv[0] = '\0'; g_wifi7_name[0] = '\0';

#ifdef WUBU_HOSTED
    /* Bare metal: scan PCI for Wi-Fi 7 devices. */
    if (wubu_hw_is_wsl()) return;
    wubu_pci_dev_t devs[WUBU_PCI_MAX_DEVS];
    int n = wubu_pci_scan(devs, WUBU_PCI_MAX_DEVS);
    for (int i = 0; i < n; i++) {
        if ((devs[i].class_code >> 8) != PCI_CLASS_NETWORK ||
            devs[i].subclass != PCI_SUBCLASS_WIFI) continue;
        int v = devs[i].vendor, d = devs[i].device;
        if (v == PCI_VENDOR_INTEL && (d == INTEL_BE200 || d == INTEL_BE201)) {
            g_wifi7 = 1; g_wifi7_vendor = v;
            strcpy(g_wifi7_drv, "iwlwifi"); strcpy(g_wifi7_name, "Intel BE200");
        } else if (v == PCI_VENDOR_QUALCOMM && d == QC_WCN7850) {
            g_wifi7 = 1; g_wifi7_vendor = v;
            strcpy(g_wifi7_drv, "ath12k_pci"); strcpy(g_wifi7_name, "Qualcomm FC7800");
        } else if (v == PCI_VENDOR_MEDIATEK && d == MTK_MT7925) {
            g_wifi7 = 1; g_wifi7_vendor = v;
            strcpy(g_wifi7_drv, "mt7925e"); strcpy(g_wifi7_name, "MediaTek MT7925");
        } else if (v == PCI_VENDOR_REALTEK && (d == RTK_RTL8922 || d == RTK_RTL8852C)) {
            g_wifi7 = 1; g_wifi7_vendor = v;
            strcpy(g_wifi7_drv, "rtw89"); strcpy(g_wifi7_name, "Realtek RTL8922");
        } else if (v == PCI_VENDOR_BROADCOM && d == 0x43f5) {
            g_wifi7 = 1; g_wifi7_vendor = v;
            strcpy(g_wifi7_drv, "brcmfmac"); strcpy(g_wifi7_name, "Broadcom BCM4389");
        }
        if (g_wifi7) break;
    }
    /* Wi-Fi 7 cards all support 6GHz + MLO + 320MHz. */
    if (g_wifi7) { g_mlo = 1; g_band6ghz = 1; g_320mhz = 1; }
#endif
}

/* ---- W2: accessors ---- */
int  wubu_wifi7_present(void)  { return g_wifi7; }
int  wubu_wifi7_mlo(void)      { return g_mlo; }
int  wubu_wifi7_6ghz(void)     { return g_band6ghz; }
int  wubu_wifi7_320mhz(void)   { return g_320mhz; }
int  wubu_wifi7_vendor(void)   { return g_wifi7_vendor; }
const char *wubu_wifi7_driver(void){ return g_wifi7_drv[0] ? g_wifi7_drv : NULL; }
const char *wubu_wifi7_name(void){ return g_wifi7_name[0] ? g_wifi7_name : NULL; }

/* ---- W3: Wi-Fi 7 driver routing ---- */
const char *wubu_wifi7_driver_for(int vendor, int device)
{
    if (vendor == PCI_VENDOR_INTEL && (device == INTEL_BE200 || device == INTEL_BE201))
        return "iwlwifi";
    if (vendor == PCI_VENDOR_QUALCOMM && device == QC_WCN7850)
        return "ath12k_pci";
    if (vendor == PCI_VENDOR_MEDIATEK && device == MTK_MT7925)
        return "mt7925e";
    if (vendor == PCI_VENDOR_REALTEK && (device == RTK_RTL8922 || device == RTK_RTL8852C))
        return "rtw89";
    return NULL;
}

/* ---- W4: summary ---- */
int wubu_wifi7_summary(char *out, size_t cap)
{
    return snprintf(out, cap,
        "wifi7[present=%d mlo=%d 6ghz=%d 320mhz=%d drv=%s name=%s]",
        g_wifi7, g_mlo, g_band6ghz, g_320mhz,
        wubu_wifi7_driver() ? wubu_wifi7_driver() : "none",
        wubu_wifi7_name() ? wubu_wifi7_name() : "-");
}
