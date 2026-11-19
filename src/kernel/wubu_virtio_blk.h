/*
 * wubu_virtio_blk.h -- legacy virtio-pci block device (L1: real storage).
 *
 * The kernel's first PERSISTENT disk backend: a legacy virtio-blk PCI
 * function (vendor 0x1AF4, device 0x1001 -- spec legacy ID; 0x1000 on
 * some hypervisors -- class 0x01, BAR0 I/O-port layout) with one
 * split virtqueue and polled completions. Replaces the AHCI RAM sim disk
 * as the FAT32 boot volume when a virtio disk is present; the sim disk
 * remains the fallback when none is.
 *
 * read/write return 0 on success, -1 on error (the fat32_blk_ops
 * convention directly -- no sector-count normalization needed).
 * C11 freestanding; no libc dependency in the metal build.
 */
#ifndef WUBU_VIRTIO_BLK_H
#define WUBU_VIRTIO_BLK_H

#include <stdint.h>
#include <stddef.h>

typedef struct wubu_virtio_blk wubu_virtio_blk_t;   /* opaque */

/* Scan PCI bus 0 for a legacy virtio-blk function and bring it up
 * (reset -> feature negotiation -> virtqueue -> DRIVER_OK -> capacity).
 * Returns the device, or NULL when absent / init failed (fail-soft:
 * the caller keeps the sim-disk fallback). */
wubu_virtio_blk_t *wubu_virtio_blk_probe(void);

/* Capacity in 512-byte sectors (0 when not ready). */
uint64_t wubu_virtio_blk_capacity(const wubu_virtio_blk_t *d);

/* Sector I/O. 0 on success, -1 on error. `buf` may live anywhere
 * (stack included): the driver bounces through DMA-able memory. */
int wubu_virtio_blk_read(wubu_virtio_blk_t *d, uint64_t lba,
                         uint32_t n_sectors, void *buf);
int wubu_virtio_blk_write(wubu_virtio_blk_t *d, uint64_t lba,
                          uint32_t n_sectors, const void *buf);

/* VIRTIO_BLK_T_FLUSH when the feature was negotiated; 0 otherwise
 * (the device is write-through without it). */
int wubu_virtio_blk_flush(wubu_virtio_blk_t *d);

/* fat32_blk_ops adapters: ctx = wubu_virtio_blk_t *. */
int wubu_virtio_blk_fat32_read(void *ctx, uint64_t lba, uint32_t n, void *buf);
int wubu_virtio_blk_fat32_write(void *ctx, uint64_t lba, uint32_t n,
                                const void *buf);

#endif /* WUBU_VIRTIO_BLK_H */