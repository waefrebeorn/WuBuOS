/*
 * wubu_drv_virtio.c -- the VIRTIO drivers (QEMU/KVM machines).
 *
 * "We are an AGI that runs on everything" — and a huge fraction of
 * everything is a VIRTUAL MACHINE: dev boxes, CI, cloud instances,
 * the hosted WuBuOS test path. virtio is the paravirtual device
 * family every VM presents (PCI vendor 0x1AF4, legacy device IDs
 * 0x1000 + type - 1). The canonical IDs live in wubu_virtio_ids.h;
 * this registry uses them so it matches the real drivers.
 *
 * The drivers (per virtio device type):
 *   - virtio-blk   (1AF4:1001): the VM's disk — the block device
 *   - virtio-net   (1AF4:1000): the VM's NIC — the link + MAC
 *   - virtio-gpu   (1AF4:100F): the VM's display — the framebuffer size
 *   - virtio-input (1AF4:1010): the VM's keyboard/mouse (via the HID feed)
 *
 * The virtio model: the device has a config space (the capability
 * pointers -> the config BAR) + the virtqueue doorbells. This driver
 * models the CONFIG + the negotiation contract: the feature bits, the
 * device status (ACK/DRIVER/FEATURES_OK/DRIVER_OK), and the class-
 * specific config fields (the block capacity, the MAC, the framebuffer).
 *
 * The tests inject a fake virtio config window.
 *
 * C11.
 */
#include "wubu_drv.h"
#include "wubu_drv_virtio.h"
#include "wubu_virtio_ids.h"

#include <stdio.h>
#include <string.h>

/* The config selectors ARE the canonical legacy PCI device IDs from
 * wubu_virtio_ids.h -- no low-byte "type" extraction (the old 0x01/0x03
 * model never matched a real QEMU device). */

/* the device status bits */
#define VIRTIO_ACK         (1u << 0)
#define VIRTIO_DRIVER      (1u << 1)
#define VIRTIO_FEATURES_OK (1u << 3)
#define VIRTIO_DRIVER_OK   (1u << 4)

/* the config register offsets (the capability -> the config BAR) */
#define VIRTIO_STATUS    0x00
#define VIRTIO_FEATURES  0x04
#define VIRTIO_CFG       0x40   /* the class config start */

/* the class config fields */
#define VIRTIO_BLK_CAP  0x40    /* the 64-bit capacity (sectors) */
#define VIRTIO_NET_MAC  0x40    /* the 6-byte MAC */
#define VIRTIO_GPU_W    0x40    /* the 32-bit width */
#define VIRTIO_GPU_H    0x44    /* the 32-bit height */

typedef struct {
    volatile uint8_t *mmio;
    int   present;
    int   status;          /* the negotiated status */
    uint64_t blk_sectors;
    uint8_t net_mac[6];
    int   gpu_w, gpu_h;
    int   negotiated;
} wubu_virtio_t;

static wubu_virtio_t g_v;

/* V1: the virtio probe — the negotiation contract:
 * ACK -> DRIVER -> FEATURES_OK -> DRIVER_OK. */
static int virtio_probe(wubu_drv_dev_t *dev)
{
    if (!g_v.mmio) return -1;
    g_v.mmio[VIRTIO_STATUS] = VIRTIO_ACK | VIRTIO_DRIVER;
    g_v.mmio[VIRTIO_STATUS] |= VIRTIO_FEATURES_OK;
    g_v.mmio[VIRTIO_STATUS] |= VIRTIO_DRIVER_OK;
    g_v.status = g_v.mmio[VIRTIO_STATUS];
    g_v.negotiated = (g_v.status & VIRTIO_DRIVER_OK) != 0;
    if (!g_v.negotiated) return -1;

    /* the class config: select by the canonical legacy PCI device ID */
    switch (dev->device) {
    case WUBU_VIRTIO_BLK_DEVICE_ID: {
        uint64_t lo = (uint64_t)g_v.mmio[VIRTIO_BLK_CAP] |
                      ((uint64_t)g_v.mmio[VIRTIO_BLK_CAP+1] << 8) |
                      ((uint64_t)g_v.mmio[VIRTIO_BLK_CAP+2] << 16) |
                      ((uint64_t)g_v.mmio[VIRTIO_BLK_CAP+3] << 24);
        uint64_t hi = (uint64_t)g_v.mmio[VIRTIO_BLK_CAP+4] |
                      ((uint64_t)g_v.mmio[VIRTIO_BLK_CAP+5] << 8) |
                      ((uint64_t)g_v.mmio[VIRTIO_BLK_CAP+6] << 16) |
                      ((uint64_t)g_v.mmio[VIRTIO_BLK_CAP+7] << 24);
        g_v.blk_sectors = lo | (hi << 32);
        break;
    }
    case WUBU_VIRTIO_NET_DEVICE_ID:
        for (int i = 0; i < 6; i++)
            g_v.net_mac[i] = g_v.mmio[VIRTIO_NET_MAC + i];
        break;
    case WUBU_VIRTIO_GPU_DEVICE_ID:
        g_v.gpu_w = (int)(g_v.mmio[VIRTIO_GPU_W] | (g_v.mmio[VIRTIO_GPU_W+1] << 8) |
                          (g_v.mmio[VIRTIO_GPU_W+2] << 16) | (g_v.mmio[VIRTIO_GPU_W+3] << 24));
        g_v.gpu_h = (int)(g_v.mmio[VIRTIO_GPU_H] | (g_v.mmio[VIRTIO_GPU_H+1] << 8) |
                          (g_v.mmio[VIRTIO_GPU_H+2] << 16) | (g_v.mmio[VIRTIO_GPU_H+3] << 24));
        break;
    default:
        break;
    }
    g_v.present = 1;
    return 0;
}

/* the per-type ID tables -- real legacy PCI IDs, owned by wubu_virtio_ids.h */
const wubu_drv_id_t wubu_virtio_blk_ids[] = {
    { WUBU_VIRTIO_VENDOR_ID, WUBU_VIRTIO_BLK_DEVICE_ID, 0, 0 },
    { 0, 0, 0, 0 },
};
const wubu_drv_id_t wubu_virtio_net_ids[] = {
    { WUBU_VIRTIO_VENDOR_ID, WUBU_VIRTIO_NET_DEVICE_ID, 0, 0 },
    { 0, 0, 0, 0 },
};
const wubu_drv_id_t wubu_virtio_gpu_ids[] = {
    { WUBU_VIRTIO_VENDOR_ID, WUBU_VIRTIO_GPU_DEVICE_ID, 0, 0 },
    { 0, 0, 0, 0 },
};
const wubu_drv_id_t wubu_virtio_input_ids[] = {
    { WUBU_VIRTIO_VENDOR_ID, WUBU_VIRTIO_INPUT_DEVICE_ID, 0, 0 },
    { 0, 0, 0, 0 },
};

const wubu_drv_t wubu_drv_virtio_blk   = { "virtio-blk",   wubu_virtio_blk_ids,   1, virtio_probe };
const wubu_drv_t wubu_drv_virtio_net   = { "virtio-net",   wubu_virtio_net_ids,   1, virtio_probe };
const wubu_drv_t wubu_drv_virtio_gpu   = { "virtio-gpu",   wubu_virtio_gpu_ids,   1, virtio_probe };
const wubu_drv_t wubu_drv_virtio_input = { "virtio-input", wubu_virtio_input_ids, 1, virtio_probe };

/* the test hooks */
void wubu_virtio_set_mmio(volatile void *mmio)
{
    g_v.mmio = (volatile uint8_t *)mmio;
}
int wubu_virtio_negotiated(void) { return g_v.negotiated; }
int wubu_virtio_present(void) { return g_v.present; }
uint64_t wubu_virtio_blk_sectors(void) { return g_v.blk_sectors; }
const uint8_t *wubu_virtio_net_mac(void) { return g_v.net_mac; }
int wubu_virtio_gpu_w(void) { return g_v.gpu_w; }
int wubu_virtio_gpu_h(void) { return g_v.gpu_h; }
