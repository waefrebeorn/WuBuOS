/*
 * wubu_world_test.c -- the world-state bridge test.
 *
 * The OS as the AGI's training space: assemble the world snapshot
 * from the REAL driver state (the fake Deck bus) and assert:
 *   1. the snapshot carries the actual hardware (nvme 512GB, sd 1TB,
 *      wifi up, the 1280x800 DSI panel, battery 97%, 72C cpu)
 *   2. the one-line state is the AGI's readable perception
 *   3. a sample refresh tracks the world's motion (the battery drops)
 */
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

#define FAIL(...) do { printf("  FAIL: " __VA_ARGS__); printf("\n"); return 1; } while (0)

int main(void)
{
    printf("=== wubu_world_test (the OS as the AGI's training space) ===\n");

    /* the fake hardware: the minimal bus (nvme + sd + wifi + gpu +
     * battery + thermal) */
    static uint8_t nvme_mmio[0x2000];
    memset(nvme_mmio, 0, sizeof(nvme_mmio));
    nvme_mmio[0x1C] = 1;
    wubu_nvme_set_mmio(nvme_mmio, sizeof(nvme_mmio));
    wubu_nvme_set_identify(976773168ULL, 1, 512);

    static uint8_t sd_mmio[0x100];
    memset(sd_mmio, 0, sizeof(sd_mmio));
    sd_mmio[0x24] = 1u << 5;
    wubu_sd_set_mmio(sd_mmio);
    wubu_sd_set_card(1, NULL, 1024 * 1024);

    static uint8_t wifi_mmio[32];
    memset(wifi_mmio, 0, sizeof(wifi_mmio));
    wifi_mmio[0] = 1;
    wubu_net_set_wifi_mmio(wifi_mmio);

    static uint8_t gpu_mmio[32];
    memset(gpu_mmio, 0, sizeof(gpu_mmio));
    gpu_mmio[0] = 2;                       /* DSI */
    gpu_mmio[4] = 0x00; gpu_mmio[5] = 0x05;
    gpu_mmio[8] = 0x20; gpu_mmio[9] = 0x03;
    gpu_mmio[12] = 60;
    gpu_mmio[17] = 0x20;                   /* 8192MB */
    wubu_gpu_set_mmio(gpu_mmio);

    wubu_battery_set_present(1);
    wubu_battery_set_state(1, 40000, 41000, 3900, "B1001");
    wubu_thermal_set_present(1);
    wubu_thermal_set_temp(WUBU_THERMAL_CPU, 72);
    wubu_thermal_set_temp(WUBU_THERMAL_GPU, 68);
    wubu_thermal_policy_update();

    /* the registry + the probe */
    wubu_drv_init();
    wubu_drv_dev_t d;
    memset(&d, 0, sizeof(d));
    d.bus = WUBU_DRV_BUS_PCI;
    d.vendor = 0x144D; d.device = 0xA80A; d.class_code = 0x01; d.subclass = 0x08;
    wubu_drv_add_device(&d);
    d.vendor = 0x1022; d.device = 0x7906; d.class_code = 0x08; d.subclass = 0x05;
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

    /* 1. the snapshot */
    wubu_world_init();
    wubu_world_sample();
    const wubu_world_t *w = wubu_world_snapshot();
    if (!w->has_nvme || w->nvme_gb != 465) FAIL("nvme: %u GB", w->nvme_gb);
    if (!w->has_sd || w->sd_gb != 1024) FAIL("sd: %u GB", w->sd_gb);
    if (!w->has_wifi || !w->wifi_link) FAIL("wifi");
    if (w->screen_w != 1280 || w->screen_h != 800) FAIL("screen");
    if (w->battery_pct != 97 || !w->battery_charging) FAIL("battery");
    if (w->cpu_temp != 72 || w->gpu_temp != 68) FAIL("temps");
    if (w->fan_duty != 64) FAIL("fan duty %u", w->fan_duty);
    printf("  PASS: the snapshot carries the real world state\n");

    /* 2. the one-line state (the AGI's perception) */
    char state[1024];
    wubu_world_state_str(state, sizeof(state));
    if (!strstr(state, "nvme") || !strstr(state, "wifi:1") ||
        !strstr(state, "1280x800") || !strstr(state, "97%") ||
        !strstr(state, "cpu:72C") || !strstr(state, "thr:0"))
        FAIL("state line: %s", state);
    printf("  PASS: the AGI reads the world: %s\n", state);

    /* 3. the world's motion: the battery drains to 60%, the wifi
     * drops — the next sample sees the change */
    wubu_battery_set_state(0, 24600, 41000, 3900, "B1001");
    wifi_mmio[0] = 0;
    wubu_world_sample();
    w = wubu_world_snapshot();
    if (w->battery_pct != 60) FAIL("battery after drain: %u", w->battery_pct);
    if (w->wifi_link) FAIL("wifi still up");
    wubu_world_state_str(state, sizeof(state));
    if (!strstr(state, "60%") || !strstr(state, "wifi:0"))
        FAIL("motion not seen: %s", state);
    printf("  PASS: the world's motion is visible to the AGI\n");

    printf("=== ALL WORLD TESTS PASSED (the OS as the training space) ===\n");
    return 0;
}
