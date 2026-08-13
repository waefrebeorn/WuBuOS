/*
 * wubu_ns_world_test.c -- the /n/world subtree test.
 *
 * Asserts the file<->API routing:
 *   1. publish creates /n/world/state + hw
 *   2. refresh writes the world snapshot (from the real driver state)
 *   3. the hw inventory lists the hardware
 */
#include "wubu_ns_bridge_internal.h"
#include "wubu_ns_world.h"
#include "wubu_world.h"
#include "wubu_drv.h"
#include "wubu_drv_nvme.h"
#include "wubu_drv_net.h"
#include "wubu_drv_gpu.h"
#include "wubu_drv_battery.h"
#include "wubu_drv_sd.h"
#include "wubu_drv_thermal.h"
#include <stdio.h>
#include <string.h>

#define NSROOT "/tmp/ns_world_test"
#define FAIL(...) do { printf("  FAIL: " __VA_ARGS__); printf("\n"); return 1; } while (0)

static int read_file(const char *p, char *out, size_t cap)
{
    FILE *f = fopen(p, "r");
    if (!f) return 0;
    size_t n = fread(out, 1, cap - 1, f);
    fclose(f);
    out[n] = '\0';
    return 1;
}

int main(void)
{
    printf("=== wubu_ns_world_test (the /n/world control subtree) ===\n");
    system("rm -rf " NSROOT);
    *(const char **)&g_ns_root = NSROOT;

    /* the fake hardware: a Deck with nvme + wifi + gpu + battery */
    static uint8_t nvme_mmio[0x2000];
    memset(nvme_mmio, 0, sizeof(nvme_mmio));
    nvme_mmio[0x1C] = 1;
    wubu_nvme_set_mmio(nvme_mmio, sizeof(nvme_mmio));
    wubu_nvme_set_identify(976773168ULL, 1, 512);
    static uint8_t wifi_mmio[32];
    memset(wifi_mmio, 0, sizeof(wifi_mmio));
    wifi_mmio[0] = 1;
    wubu_net_set_wifi_mmio(wifi_mmio);
    static uint8_t gpu_mmio[32];
    memset(gpu_mmio, 0, sizeof(gpu_mmio));
    gpu_mmio[0] = 2;
    gpu_mmio[4] = 0x00; gpu_mmio[5] = 0x05;
    gpu_mmio[8] = 0x20; gpu_mmio[9] = 0x03;
    wubu_gpu_set_mmio(gpu_mmio);
    wubu_battery_set_present(1);
    wubu_battery_set_state(1, 40000, 41000, 3900, "B1001");
    wubu_thermal_set_present(1);
    wubu_thermal_set_temp(WUBU_THERMAL_CPU, 72);
    wubu_thermal_policy_update();

    wubu_drv_init();
    wubu_drv_dev_t d;
    memset(&d, 0, sizeof(d));
    d.bus = WUBU_DRV_BUS_PCI;
    d.vendor = 0x144D; d.device = 0xA80A; d.class_code = 0x01; d.subclass = 0x08;
    wubu_drv_add_device(&d);
    d.vendor = 0x14C3; d.device = 0x7922; d.class_code = 0x02; d.subclass = 0x80;
    wubu_drv_add_device(&d);
    d.vendor = 0x1002; d.device = 0x163F; d.class_code = 0x03; d.subclass = 0x00;
    wubu_drv_add_device(&d);
    d.vendor = 0; d.device = 0; d.class_code = 0x01; d.subclass = 0x0C;
    wubu_drv_add_device(&d);
    d.vendor = 0; d.device = 0; d.class_code = 0x11; d.subclass = 0x00;
    wubu_drv_add_device(&d);
    wubu_drv_probe();

    wubu_world_init();

    if (wubu_ns_publish_world() != 0) FAIL("publish");

    /* 1. the files exist */
    char p[512], buf[2048];
    snprintf(p, sizeof(p), "%s/world/state", NSROOT);
    if (!read_file(p, buf, sizeof(buf))) FAIL("no state");
    snprintf(p, sizeof(p), "%s/world/hw", NSROOT);
    if (!read_file(p, buf, sizeof(buf))) FAIL("no hw");
    printf("  PASS: publish creates /n/world/state + hw\n");

    /* 2. the refresh writes the snapshot */
    if (wubu_ns_world_refresh() != 0) FAIL("refresh state");
    snprintf(p, sizeof(p), "%s/world/state", NSROOT);
    read_file(p, buf, sizeof(buf));
    if (!strstr(buf, "nvme") || !strstr(buf, "wifi:1") ||
        !strstr(buf, "97%") || !strstr(buf, "cpu:72C"))
        FAIL("state: %s", buf);
    printf("  PASS: /n/world/state = %s", buf);

    /* 3. the hw inventory */
    if (wubu_ns_world_refresh_hw() != 0) FAIL("refresh hw");
    snprintf(p, sizeof(p), "%s/world/hw", NSROOT);
    read_file(p, buf, sizeof(buf));
    if (!strstr(buf, "nvme") || !strstr(buf, "wifi-up") ||
        !strstr(buf, "battery"))
        FAIL("hw: %s", buf);
    printf("  PASS: /n/world/hw lists the hardware\n");

    system("rm -rf " NSROOT);
    printf("=== ALL NS-WORLD TESTS PASSED ===\n");
    return 0;
}
