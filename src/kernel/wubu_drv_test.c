/*
 * wubu_drv_test.c -- the DRIVER REGISTRY test (the Steam Deck bus).
 *
 * Simulates the REAL Steam Deck PCI bus + the class MMIO windows:
 *   - 1002:163F  Van Gogh APU (display)   class 03/00 -> gpu
 *   - 144D:A80A  Samsung 980 NVMe         class 01/08 -> nvme
 *   - 14C3:7922  MediaTek RZ616 Wi-Fi     class 02/80 -> wifi
 *   - 1022:1457  Van Gogh HDA audio       class 04/03 -> hda
 *   - 10EC:8168  Realtek Ethernet         class 02/00 -> net
 *   - a battery (ACPI)                    class 01/0C -> battery
 *   - 1B21:0xxx  ASMedia USB 3.0          class 0C/03 -> unbound
 *
 * Asserts:
 *   1. the registry binds every real device to its driver
 *   2. the NVMe controller comes ready with the namespace info
 *   3. the Wi-Fi + Ethernet links come up with the MACs
 *   4. the GPU modesets the Deck's 1280x800 DSI panel
 *   5. the battery reports the percent
 *   6. the summary shows the full bus
 */
#include "wubu_drv.h"
#include "wubu_drv_nvme.h"
#include "wubu_drv_net.h"
#include "wubu_drv_hda.h"
#include "wubu_drv_gpu.h"
#include "wubu_drv_battery.h"
#include "wubu_drv_ahci.h"

#include <stdio.h>
#include <string.h>

#define FAIL(...) do { printf("  FAIL: " __VA_ARGS__); printf("\n"); return 1; } while (0)

/* the fake NVMe controller MMIO: the CAP + VS + CSTS.RDY + the
 * identify result set via wubu_nvme_set_identify */
static uint8_t nvme_mmio[0x2000];

/* the fake Wi-Fi + Ethernet windows */
static uint8_t wifi_mmio[32];
static uint8_t eth_mmio[32];

/* the fake GPU window: DSI connector, 1280x800@60, 8GB VRAM */
static uint8_t gpu_mmio[32];

int main(void)
{
    printf("=== wubu_drv_test (the Steam Deck + laptop driver registry) ===\n");

    /* the fake NVMe: CAP (64-bit, ready), version 1.4, CSTS.RDY */
    memset(nvme_mmio, 0, sizeof(nvme_mmio));
    nvme_mmio[0x1C] = 1;                    /* CSTS.RDY */
    nvme_mmio[0x08] = 0x14; nvme_mmio[0x0B] = 0x01;  /* VS 1.4 */
    wubu_nvme_set_mmio(nvme_mmio, sizeof(nvme_mmio));
    wubu_nvme_set_identify(976773168ULL, 1, 512);   /* 512GB */

    /* the fake Wi-Fi: link up + a MAC */
    memset(wifi_mmio, 0, sizeof(wifi_mmio));
    wifi_mmio[0] = 1;                        /* link up */
    wifi_mmio[4] = 0x00; wifi_mmio[5] = 0x1A; wifi_mmio[6] = 0x7D;
    wifi_mmio[7] = 0xDA; wifi_mmio[8] = 0x71; wifi_mmio[9] = 0x13;
    wubu_net_set_wifi_mmio(wifi_mmio);
    memset(eth_mmio, 0, sizeof(eth_mmio));
    eth_mmio[0] = 1;                         /* link up */
    eth_mmio[4] = 0x00; eth_mmio[5] = 0x1A; eth_mmio[6] = 0x7D;
    eth_mmio[7] = 0xDA; eth_mmio[8] = 0x71; eth_mmio[9] = 0x22;
    wubu_net_set_eth_mmio(eth_mmio);

    /* the fake GPU: DSI, 1280x800, 60Hz, 8192MB */
    memset(gpu_mmio, 0, sizeof(gpu_mmio));
    gpu_mmio[0] = 2;                         /* DSI */
    gpu_mmio[4] = 0x00; gpu_mmio[5] = 0x05;  /* 1280 */
    gpu_mmio[8] = 0x20; gpu_mmio[9] = 0x03;  /* 800 */
    gpu_mmio[12] = 60;
    gpu_mmio[16] = 0x00; gpu_mmio[17] = 0x20; gpu_mmio[18] = 0x00; gpu_mmio[19] = 0x00; /* 8192MB */
    wubu_gpu_set_mmio(gpu_mmio);

    /* the battery: charging, 40Wh capacity / 41Wh design, 3.9V */
    wubu_battery_set_present(1);
    wubu_battery_set_state(1, 40000, 41000, 3900, "B1001");

    /* the registry + the fake Deck bus */
    wubu_drv_init();
    wubu_hda_set_present(1);

    wubu_drv_dev_t d;
    memset(&d, 0, sizeof(d));
    d.bus = WUBU_DRV_BUS_PCI;
    d.vendor = 0x1002; d.device = 0x163F; d.class_code = 0x03; d.subclass = 0x00;
    wubu_drv_add_device(&d);   /* Van Gogh iGPU */
    d.vendor = 0x144D; d.device = 0xA80A; d.class_code = 0x01; d.subclass = 0x08;
    wubu_drv_add_device(&d);   /* NVMe SSD */
    d.vendor = 0x14C3; d.device = 0x7922; d.class_code = 0x02; d.subclass = 0x80;
    wubu_drv_add_device(&d);   /* RZ616 Wi-Fi */
    d.vendor = 0x1022; d.device = 0x1457; d.class_code = 0x04; d.subclass = 0x03;
    wubu_drv_add_device(&d);   /* HDA audio */
    d.vendor = 0x10EC; d.device = 0x8168; d.class_code = 0x02; d.subclass = 0x00;
    wubu_drv_add_device(&d);   /* Ethernet */
    d.vendor = 0x0000; d.device = 0x0000; d.class_code = 0x01; d.subclass = 0x0C;
    wubu_drv_add_device(&d);   /* the battery */
    d.vendor = 0x1B21; d.device = 0x1242; d.class_code = 0x0C; d.subclass = 0x03;
    wubu_drv_add_device(&d);   /* the USB 3 controller (unbound) */

    if (wubu_drv_device_count() != 7) FAIL("device count = %d, want 7",
                                           wubu_drv_device_count());

    /* 1. probe every device */
    int probed = wubu_drv_probe();
    if (probed != 6) FAIL("probed = %d, want 6 (all but the USB)", probed);
    printf("  PASS: the registry binds every real Deck device\n");

    /* 2. the NVMe */
    const wubu_drv_dev_t *nvme = wubu_drv_find("nvme");
    if (!nvme || !nvme->bound) FAIL("nvme not bound");
    if (!wubu_nvme_ready()) FAIL("nvme not ready");
    if (wubu_nvme_nsze() != 976773168ULL) FAIL("nsze");
    if (wubu_nvme_block_size() != 512) FAIL("block size");
    printf("  PASS: the NVMe SSD is ready (512GB, 512B blocks)\n");

    /* 3. the network */
    if (!wubu_net_wifi_link()) FAIL("wifi link down");
    if (!wubu_net_eth_link()) FAIL("eth link down");
    char mac[32];
    wubu_net_mac_str(wubu_net_wifi_mac(), mac, sizeof(mac));
    if (strcmp(mac, "00:1a:7d:da:71:13") != 0) FAIL("wifi mac = %s", mac);
    printf("  PASS: the Wi-Fi + Ethernet links up with the MACs\n");

    /* 4. the GPU */
    if (wubu_gpu_connector() != WUBU_GPU_CONNECTOR_DSI) FAIL("gpu connector");
    if (wubu_gpu_width() != 1280 || wubu_gpu_height() != 800) FAIL("gpu mode");
    if (wubu_gpu_refresh() != 60) FAIL("gpu refresh");
    if (wubu_gpu_vram_mb() != 8192) FAIL("gpu vram");
    printf("  PASS: the GPU modesets the Deck's 1280x800 DSI panel\n");

    /* 5. the battery */
    const wubu_drv_dev_t *bat = wubu_drv_find("battery");
    if (!bat || !bat->bound) FAIL("battery not bound");
    if (wubu_battery_percent() != 97) FAIL("battery percent = %d, want 97",
                                           wubu_battery_percent());
    if (!wubu_battery_charging()) FAIL("battery not charging");
    printf("  PASS: the battery reports 97%% while charging\n");

    /* 6. the summary */
    char summary[4096];
    int lines = wubu_drv_summary(summary, sizeof(summary));
    if (lines != 7) FAIL("summary lines = %d, want 7", lines);
    if (!strstr(summary, "nvme") || !strstr(summary, "wifi") ||
        !strstr(summary, "gpu") || !strstr(summary, "battery"))
        FAIL("summary lacks the bound drivers");
    printf("  PASS: the boot summary shows the whole bus\n");
    printf("%s", summary);

    printf("=== ALL DRV TESTS PASSED (the Steam Deck + laptop driver registry) ===\n");
    return 0;
}
