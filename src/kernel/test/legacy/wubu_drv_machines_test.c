/*
 * wubu_drv_machines_test.c -- the OTHER machines (the AGI runs on
 * everything).
 *
 * Three machines beyond the Steam Deck:
 *   1. A QEMU/KVM VM: the virtio family (blk 1AF4:1001, net 1AF4:1000,
 *      gpu 1AF4:100F, input 1AF4:1010) — the negotiation contract
 *      and the driver-registry ID mapping (the gate for this fix)
 *   2. An ARM machine (the CM4 gate): the platform bus (the Pi 4's
 *      peripheral block) — the UART/timer/GIC/GPIO blocks
 *   3. An Intel laptop (ThinkPad-class): the DPTF platform thermal
 *
 * Asserts:
 *   - the virtio negotiation completes + the blk/net/gpu configs read
 *   - the ARM platform probes + the blocks report ok
 *   - the Intel DPTF binds
 */
#include "wubu_drv.h"
#include "wubu_drv_virtio.h"
#include "wubu_virtio_ids.h"
#include "wubu_drv_arm.h"
#include "wubu_drv_intel.h"
#include <stdio.h>
#include <string.h>

#define FAIL(...) do { printf("  FAIL: " __VA_ARGS__); printf("\n"); return 1; } while (0)

int main(void)
{
    printf("=== wubu_drv_machines_test (the other machines) ===\n");

    /* ---- 1. the QEMU/KVM VM: the virtio family ---- */
    static uint8_t vblk_mmio[0x100];
    memset(vblk_mmio, 0, sizeof(vblk_mmio));
    /* the blk capacity: 8GB = 16777216 sectors x 512 */
    uint64_t sectors = 16777216;
    for (int i = 0; i < 8; i++) vblk_mmio[0x40 + i] = (uint8_t)(sectors >> (i * 8));
    wubu_virtio_set_mmio(vblk_mmio);

    wubu_drv_init();
    wubu_drv_dev_t d;
    memset(&d, 0, sizeof(d));
    d.bus = WUBU_DRV_BUS_PCI;
    d.vendor = WUBU_VIRTIO_VENDOR_ID;
    d.device = WUBU_VIRTIO_BLK_DEVICE_ID;   /* real legacy virtio-blk */
    wubu_drv_add_device(&d);
    d.device = WUBU_VIRTIO_NET_DEVICE_ID;   /* real legacy virtio-net */
    wubu_drv_add_device(&d);
    d.device = WUBU_VIRTIO_GPU_DEVICE_ID;   /* real legacy virtio-gpu */
    wubu_drv_add_device(&d);
    d.device = WUBU_VIRTIO_INPUT_DEVICE_ID; /* real legacy virtio-input */
    wubu_drv_add_device(&d);

    wubu_drv_probe();

    /* the behavior gate: real legacy PCI IDs must bind to the RIGHT driver */
    const wubu_drv_dev_t *vblk = wubu_drv_find("virtio-blk");
    if (!vblk) FAIL("virtio-blk (1AF4:1001) not bound");
    if (vblk->vendor != WUBU_VIRTIO_VENDOR_ID ||
        vblk->device != WUBU_VIRTIO_BLK_DEVICE_ID)
        FAIL("virtio-blk bound to %04X:%04X, want %04X:%04X",
             vblk->vendor, vblk->device,
             WUBU_VIRTIO_VENDOR_ID, WUBU_VIRTIO_BLK_DEVICE_ID);
    const wubu_drv_dev_t *vnet = wubu_drv_find("virtio-net");
    if (!vnet) FAIL("virtio-net (1AF4:1000) not bound");
    if (vnet->vendor != WUBU_VIRTIO_VENDOR_ID ||
        vnet->device != WUBU_VIRTIO_NET_DEVICE_ID)
        FAIL("virtio-net bound to %04X:%04X, want %04X:%04X",
             vnet->vendor, vnet->device,
             WUBU_VIRTIO_VENDOR_ID, WUBU_VIRTIO_NET_DEVICE_ID);
    const wubu_drv_dev_t *vgpu = wubu_drv_find("virtio-gpu");
    if (!vgpu || vgpu->device != WUBU_VIRTIO_GPU_DEVICE_ID)
        FAIL("virtio-gpu (1AF4:100F) not bound");
    const wubu_drv_dev_t *vinput = wubu_drv_find("virtio-input");
    if (!vinput || vinput->device != WUBU_VIRTIO_INPUT_DEVICE_ID)
        FAIL("virtio-input (1AF4:1010) not bound");

    if (!wubu_virtio_negotiated()) FAIL("virtio negotiation failed");
    if (wubu_virtio_blk_sectors() != 16777216)
        FAIL("virtio-blk sectors = %llu", (unsigned long long)wubu_virtio_blk_sectors());

    printf("  PASS: virtio registry maps blk=1AF4:1001 net=1AF4:1000 "
           "(gpu=1AF4:100F input=1AF4:1010)\n");
    printf("  PASS: the VM's virtio family negotiates + binds\n");

    /* ---- 2. the ARM machine (the CM4 gate) ---- */
    wubu_arm_probe_blocks(WUBU_ARM_RPI4);
    if (!wubu_arm_present()) FAIL("arm not present");
    if (!wubu_arm_uart_ok() || !wubu_arm_timer_ok() ||
        !wubu_arm_gic_ok() || !wubu_arm_gpio_ok())
        FAIL("arm blocks not ok");
    if (strcmp(wubu_arm_model_name(wubu_arm_model()),
               "Raspberry Pi 4 / CM4") != 0)
        FAIL("arm model name");
    printf("  PASS: the ARM machine probes (the %s)\n",
           wubu_arm_model_name(wubu_arm_model()));

    /* ---- 3. the Intel laptop (DPTF) ---- */
    memset(&d, 0, sizeof(d));
    d.bus = WUBU_DRV_BUS_PCI;
    d.vendor = 0x8086; d.device = 0xA131;   /* the Alder Lake PCH thermal */
    wubu_drv_add_device(&d);
    wubu_drv_probe();
    const wubu_drv_dev_t *intel = wubu_drv_find("intel-platform");
    if (!intel || !intel->bound) FAIL("intel platform not bound");
    if (!wubu_intel_dptf_ok() || wubu_intel_dptf_zones() != 3)
        FAIL("intel dptf");
    printf("  PASS: the Intel laptop's DPTF binds (3 thermal zones)\n");

    /* ---- the full summary ---- */
    char summary[4096];
    int lines = wubu_drv_summary(summary, sizeof(summary));
    if (lines != 5) FAIL("summary lines = %d, want 5", lines);
    printf("  PASS: the machines summary shows all 5 devices\n");
    printf("%s", summary);

    printf("=== ALL MACHINES TESTS PASSED (the AGI runs on everything) ===\n");
    return 0;
}
