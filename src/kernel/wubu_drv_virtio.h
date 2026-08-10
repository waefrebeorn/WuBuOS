/*
 * wubu_drv_virtio.h -- the virtio drivers (QEMU/KVM machines).
 */
#ifndef WUBU_DRV_VIRTIO_H
#define WUBU_DRV_VIRTIO_H

#include <stdint.h>

/* the drivers (registered by the registry) */
extern const struct wubu_drv wubu_drv_virtio_blk;
extern const struct wubu_drv wubu_drv_virtio_net;
extern const struct wubu_drv wubu_drv_virtio_gpu;
extern const struct wubu_drv wubu_drv_virtio_input;

/* the test hooks */
void wubu_virtio_set_mmio(volatile void *mmio);

/* the state */
int wubu_virtio_negotiated(void);
int wubu_virtio_present(void);
uint64_t wubu_virtio_blk_sectors(void);
const uint8_t *wubu_virtio_net_mac(void);
int wubu_virtio_gpu_w(void);
int wubu_virtio_gpu_h(void);

#endif
