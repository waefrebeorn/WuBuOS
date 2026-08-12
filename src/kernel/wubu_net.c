/*
 * wubu_net.c -- kernel-owned network driver routing + power-save tuning.
 *
 * Every Wi-Fi vendor (Intel iwlwifi, Realtek rtl88x2ce, MediaTek mt7921e)
 * degrades on Linux because of POWER-SAVE, not throughput. The kernel's
 * WLAN dynamic power management adds 30-130ms latency and up to 9% packet
 * loss. Realtek chips often ship with NO mainline driver (rtl8821ce needs
 * a DKMS build). 2.5GbE NICs need the r8168 driver, not the broken r8169.
 *
 * WuBuOS owns all of this: detect the Wi-Fi/ethernet chip via PCI, then
 * emit the vendor-specific power-save config + firmware check + driver
 * routing. The user never hunts DKMS tarballs or udev rules.
 *
 * Research (Kevin-Bacon 7-hop on the network frontier):
 *   - Intel iwlwifi: AX200/210 power_save=0 fixes crashes + slow speed
 *   - Realtek rtl8821ce/8822ce: out-of-tree DKMS driver required
 *   - MediaTek mt7921e/7922e/7925e: power-save = 9% loss + 130ms latency
 *     (kernel bugzilla 219429)
 *   - Linux WLAN DPM + the llwr latency/power-save utility
 *   - Realtek RTL8125 2.5GbE: needs r8168-dkms (r8169 is broken)
 */
#include "wubu_net.h"
#include "wubu_pci.h"
#include "wubu_hw_detect.h"
#include <stdio.h>
#include <string.h>

/* ---- PCI classes ---- */
#define PCI_CLASS_NETWORK       0x02
#define PCI_SUBCLASS_ETHERNET   0x00
#define PCI_SUBCLASS_WIFI       0x80   /* 0x0280 = WLAN */

/* ---- Wi-Fi vendor IDs ---- */
#define PCI_VENDOR_INTEL    0x8086
#define PCI_VENDOR_REALTEK  0x10EC
#define PCI_VENDOR_MEDIATEK 0x14C3
#define PCI_VENDOR_BROADCOM 0x14E4
#define PCI_VENDOR_ATHEROS  0x168C

/* ---- Global state ---- */
static int  g_wifi_vendor = 0;      /* PCI vendor of the Wi-Fi chip */
static int  g_wifi_device = 0;
static int  g_wifi_present = 0;
static int  g_eth_present = 0;
static int  g_eth_2g5 = 0;          /* 2.5GbE (needs r8168) */
static char g_net_driver[64] = "";  /* Wi-Fi driver name */
static char g_eth_driver[64] = "";

/* Known Wi-Fi chips (vendor/device → driver + power-save flag).
 * EVERY vendor x generation, the full headache matrix. */
typedef struct {
    int vendor, device;
    const char *driver;
    const char *name;
    const char *ps_disable;    /* power-save-disabling sysfs/module knob */
} wubu_wifi_t;

static const wubu_wifi_t wifi_table[] = {
    /* Intel: iwlwifi (PCI IDs) — all generations, power_save=0 per Intel docs */
    { 0x8086, 0x08B1, "iwlwifi", "Intel 6E AX210",          "iwlwifi.power_save=0" },
    { 0x8086, 0x08B2, "iwlwifi", "Intel 6E AX210",          "iwlwifi.power_save=0" },
    { 0x8086, 0x2723, "iwlwifi", "Intel AX210",             "iwlwifi.power_save=0" },
    { 0x8086, 0x2720, "iwlwifi", "Intel AX200",             "iwlwifi.power_save=0" },
    { 0x8086, 0x2724, "iwlwifi", "Intel AX211",             "iwlwifi.power_save=0" },
    { 0x8086, 0x2713, "iwlwifi", "Intel AX201",             "iwlwifi.power_save=0" },
    { 0x8086, 0x2725, "iwlwifi", "Intel BE200 Wi-Fi 7",     "iwlwifi.power_save=0" },
    { 0x8086, 0x2726, "iwlwifi", "Intel BE200",             "iwlwifi.power_save=0" },
    { 0x8086, 0xA840, "iwlwifi", "Intel 9560 ACF",          "iwlwifi.power_save=0" },
    { 0x8086, 0x02F0, "iwlwifi", "Intel 9462/9560",         "iwlwifi.power_save=0" },
    { 0x8086, 0x15B5, "iwlwifi", "Intel Wireless 8265",     "iwlwifi.power_save=0" },
    { 0x8086, 0x24ED, "iwlwifi", "Intel Dual Band 7265",    "iwlwifi.power_save=0" },
    /* Realtek: rtl8821ce / rtl8822ce / rtl8852be / rtw88 — out-of-tree pain */
    { 0x10EC, 0xC821, "rtl8821ce",  "Realtek RTL8821CE",  "rtl8821ce.ips=0"  },
    { 0x10EC, 0xC822, "rtl8821ce",  "Realtek RTL8822CE",  "rtl8821ce.ips=0"  },
    { 0x10EC, 0x8822, "rtw88_8822ce", "Realtek RTL8822CE", "rtw88.ps=0"     },
    { 0x10EC, 0x8852, "rtw88_8852be", "Realtek RTL8852BE", "rtw88.ps=0"     },
    { 0x10EC, 0x8851, "rtw88_8852be", "Realtek RTL8852BE", "rtw88.ps=0"     },
    { 0x10EC, 0xA853, "rtw88_8852ae", "Realtek RTL8852AE", "rtw88.ps=0"     },
    { 0x10EC, 0xA854, "rtw88_8852ae", "Realtek RTL8852AE", "rtw88.ps=0"     },
    /* MediaTek: mt7921 / mt7922 / mt7925e — ASPM power-save = 9% loss */
    { 0x14C3, 0x7961, "mt7921e", "MediaTek MT7921/22",      "mt7921e.disable_aspm=1" },
    { 0x14C3, 0x0616, "mt76",    "MediaTek MT7616",          "mt76.disable_usb_sg=1"  },
    { 0x14C3, 0x7902, "mt7921e", "MediaTek MT7925e",         "mt7921e.disable_aspm=1" },
    { 0x14C3, 0x7927, "mt7922",  "MediaTek MT7922",          "disable_aspm=1"         },
    { 0x14C3, 0x0712, "mt7925e", "MediaTek MT7925e",         "disable_aspm=1"         },
    /* Qualcomm: ath10k / ath11k / ath12k — FastConnect */
    { 0x168C, 0x003C, "ath10k_pci", "Qualcomm QCA6174",       "ath10k.ps_enable=0"      },
    { 0x168C, 0x0046, "ath11k_pci", "Qualcomm QCA6390",       "ath11k.disable_ps=1"      },
    { 0x168C, 0x0056, "ath11k_pci", "Qualcomm QCA6490",       "ath11k.disable_ps=1"      },
    { 0x168C, 0x0062, "ath12k_pci", "Qualcomm QCA6490 Gen2",  "ath12k.disable_ps=1"      },
    { 0x168C, 0x0050, "ath11k_pci", "Qualcomm QCA2099",       "ath11k.disable_ps=1"      },
    /* Broadcom: brcmfmac — firmware blobs + BCM43602/4366/4387 (the Apple pain) */
    { 0x14E4, 0x43A3, "brcmfmac", "Broadcom BCM4350/4360",  "brcmfmac.feature_disable=0x82000" },
    { 0x14E4, 0x43AD, "brcmfmac", "Broadcom BCM43602",      "brcmfmac.feature_disable=0x82000" },
    { 0x14E4, 0x43B1, "brcmfmac", "Broadcom BCM4366",       "brcmfmac.feature_disable=0x82000" },
    { 0x14E4, 0x43C3, "brcmfmac", "Broadcom BCM43458",      "brcmfmac.feature_disable=0x82000" },
    { 0x14E4, 0x43F5, "brcmfmac", "Broadcom BCM4387",       "brcmfmac.feature_disable=0x82000" },
    { 0x14E4, 0x43B8, "brcmfmac", "Broadcom BCM4356",       "brcmfmac.feature_disable=0x82000" },
    /* Atheros (legacy) */
    { 0x168C, 0x002A, "ath9k", "Atheros AR5008/5009", "ath9k.ps_enable=0"   },
    { 0x168C, 0x002E, "ath9k", "Atheros AR9280/9285",   "ath9k.ps_enable=0"   },
    { 0x168C, 0x0032, "ath9k", "Atheros AR9285",        "ath9k.ps_enable=0"   },
    { 0x168C, 0x0036, "ath9k", "Atheros AR10xx 5GHz",   "ath9k.ps_enable=0"   },
    { 0x168C, 0x003F, "ath9k", "Atheros AR93xx",        "ath9k.ps_enable=0"   },
    /* Ralink/MediaTek legacy (rt2x00 / mt7601u) */
    { 0x1814, 0x0301, "rt61pci", "Ralink RT2561S",        "rt61pci.ps=0"          },
    { 0x1814, 0x0401, "rt2400pci", "Ralink RT2400",        "rt2400pci.ps=0"        },
    { 0x1814, 0x0501, "rt2500pci", "Ralink RT2500",        "rt2500pci.ps=0"        },
    { 0x1814, 0x0601, "rt2561pci", "Ralink RT2561",        "rt2561pci.ps=0"        },
    { 0x1814, 0x0701, "rt2600pci", "Ralink RT2600",        "rt2600pci.ps=0"        },
    { 0x14B4, 0x43FD, "rt2800usb","Ralink RT3070 USB",     ""                       },
    { 0x2955, 0x1001, "mt7601u",  "MediaTek MT7601U",     ""                       },
    { 0x2955, 0x1002, "mt7601u",  "MediaTek MT7612U",     ""                       },
    { 0, 0, NULL, NULL },
};

/* 2.5GbE/10GbE ethernet NICs that need the r8168 driver (r8169 is broken).
 * Full ethernet driver matrix: vendor/device → driver name. */
static const struct { int vendor, device; const char *driver; const char *name; } eth_driver_table[] = {
    /* Realtek: r8169 is the in-kernel fallback, but 2.5GbE needs r8168-dkms */
    { 0x10EC, 0x8125, "r8168-dkms", "Realtek RTL8125 2.5GbE"   },
    { 0x10EC, 0x8168, "r8168-dkms", "Realtek RTL8168"           },
    { 0x10EC, 0x8169, "r8169",      "Realtek RTL8111/8169"      },
    { 0x10EC, 0x8139, "8139cp",     "Realtek RTL8139 (legacy)"  },
    /* Intel: e1000e (legacy), igc (2.5/5/10G), ice (E800 10/25/40/100G) */
    { 0x8086, 0x10D3, "e1000e",     "Intel 82576"               },
    { 0x8086, 0x10FB, "ixgbe",      "Intel 82599"               },
    { 0x8086, 0x125C, "igc",        "Intel I226-V 2.5GbE"       },
    { 0x8086, 0x10D8, "e1000e",     "Intel I210"                },
    { 0x8086, 0x10A7, "igc",        "Intel 2.5GbE I225"         },
    { 0x8086, 0x1889, "ice",        "Intel E810 10/25/40/100GbE" },
    { 0x8086, 0x1457, "e1000e",     "Intel ICH10"               },
    { 0x8086, 0x294C, "e1000e",     "Intel ICH9"                },
    /* Broadcom: tg3 (legacy), bnxt (10/25/40/50/100Gb) */
    { 0x14E4, 0x164C, "tg3",        "Broadcom BCM5751"          },
    { 0x14E4, 0x1657, "tg3",        "Broadcom BCM5717"          },
    { 0x14E4, 0x1687, "bnxt",       "Broadcom BCM5720"          },
    { 0x14E4, 0x16D8, "bnxt",       "Broadcom BCM5720 Gen2"     },
    /* Mellanox/NVIDIA: mlx5 */
    { 0x15B3, 0x1013, "mlx5_core",  "Mellanox ConnectX-4"       },
    { 0x15B3, 0x1015, "mlx5_core",  "Mellanox ConnectX-5"       },
    { 0x15B3, 0x1017, "mlx5_core",  "Mellanox ConnectX-6"       },
    { 0x15B3, 0x1019, "mlx5_core",  "Mellanox ConnectX-7"       },
    { 0x10DE, 0x1AF0, "mlx5_core",  "NVIDIA ConnectX-8"         },
    /* Marvell, QEMU/virtio, other */
    { 0x11AB, 0x2A08, "mvneta",     "Marvell Armada"            },
    { 0x8086, 0x2922, "e1000e",     "Intel ICH9 (SATA/eth combo)"},
    { 0x1234, 0x1111, "virtio_net", "QEMU virtio-net"           },
    { 0x1AF4, 0x1000, "virtio_net", "Red Hat virtio-net"      },
    { 0, 0, NULL, NULL },
};

/* ---- W1: probe the network topology ---- */
void wubu_net_probe(void)
{
    g_wifi_present = 0; g_eth_present = 0; g_eth_2g5 = 0;
    g_wifi_vendor = 0; g_wifi_device = 0;
    g_net_driver[0] = '\0'; g_eth_driver[0] = '\0';

    /* WSL2: no PCI network access, host owns networking. */
    if (!(wubu_hw_gpu_present() && wubu_hw_gpu_path() &&
          strstr(wubu_hw_gpu_path(), "/dev/dri")))
        return;

    wubu_pci_dev_t devs[WUBU_PCI_MAX_DEVS];
    int n = wubu_pci_scan(devs, WUBU_PCI_MAX_DEVS);

    for (int i = 0; i < n; i++) {
        if (devs[i].class_code != PCI_CLASS_NETWORK) continue;

        /* Wi-Fi (subclass 0x80). */
        if (devs[i].subclass == PCI_SUBCLASS_WIFI) {
            g_wifi_present = 1;
            g_wifi_vendor = devs[i].vendor;
            g_wifi_device = devs[i].device;
            for (int j = 0; wifi_table[j].driver; j++) {
                if (wifi_table[j].vendor == devs[i].vendor &&
                    wifi_table[j].device == devs[i].device) {
                    strcpy(g_net_driver, wifi_table[j].driver);
                    break;
                }
            }
            if (!g_net_driver[0])
                strcpy(g_net_driver, "unknown");
        }
        /* Ethernet (subclass 0x00) — route to the right driver via the
         * full eth_driver_table (Intel igc/ice, Realtek r8169/r8168,
         * Broadcom tg3/bnxt, Mellanox mlx5, virtio). */
        else if (devs[i].subclass == PCI_SUBCLASS_ETHERNET) {
            g_eth_present = 1;
            for (int j = 0; eth_driver_table[j].driver; j++) {
                if (eth_driver_table[j].vendor == devs[i].vendor &&
                    eth_driver_table[j].device == devs[i].device) {
                    strcpy(g_eth_driver, eth_driver_table[j].driver);
                    g_eth_2g5 = (strcmp(eth_driver_table[j].driver, "r8168-dkms") == 0);
                    break;
                }
            }
            if (!g_eth_driver[0]) {
                /* Fallback by vendor. */
                if (devs[i].vendor == 0x8086)
                    strcpy(g_eth_driver, "igc");
                else if (devs[i].vendor == 0x10EC)
                    strcpy(g_eth_driver, "r8169");
                else
                    strcpy(g_eth_driver, "generic");
            }
        }
    }
}

/* ---- W2: accessors ---- */
int          wubu_net_has_wifi(void)      { return g_wifi_present; }
int          wubu_net_has_eth(void)       { return g_eth_present; }
int          wubu_net_has_2g5(void)       { return g_eth_2g5; }
const char *wubu_net_wifi_driver(void)    { return g_net_driver[0] ? g_net_driver : NULL; }
const char *wubu_net_eth_driver(void)     { return g_eth_driver[0] ? g_eth_driver : NULL; }
int          wubu_net_wifi_vendor(void)   { return g_wifi_vendor; }

/* Human-readable chip name (looked up from the wifi_table). */
const char *wubu_net_wifi_chip_name(void)
{
    if (!g_wifi_present) return NULL;
    for (int j = 0; wifi_table[j].driver; j++) {
        if (wifi_table[j].vendor == g_wifi_vendor &&
            wifi_table[j].device == g_wifi_device)
            return wifi_table[j].name;
    }
    return "unknown";
}

/* ---- W3: the power-save-disabling kernel params ----
 * The fix for every vendor's Wi-Fi latency/speed problem. */
const char *wubu_net_power_save_disable(void)
{
    for (int j = 0; wifi_table[j].driver; j++) {
        if (wifi_table[j].vendor == g_wifi_vendor &&
            wifi_table[j].device == g_wifi_device)
            return wifi_table[j].ps_disable;
    }
    /* fallback: generic iwlwifi-style disable */
    return g_wifi_present ? "power_save=0" : NULL;
}

/* ---- W4: driver-routing summary ----
 * Surfaces whether an out-of-tree DKMS driver is required (Realtek) and
 * whether the 2.5GbE NIC needs r8168 instead of the broken r8169. */
int wubu_net_summary(char *out, size_t cap)
{
    int n = snprintf(out, cap,
        "net[wifi=%d %s eth=%d 2g5=%d ethdrv=%s ps=%s]",
        g_wifi_present,
        g_net_driver[0] ? g_net_driver : "none",
        g_eth_present, g_eth_2g5,
        g_eth_driver[0] ? g_eth_driver : "none",
        wubu_net_power_save_disable() ? wubu_net_power_save_disable() : "-");
    return n < 0 ? -1 : 0;
}
