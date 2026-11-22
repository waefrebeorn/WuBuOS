/*
 * wubu_virtio_ids.h -- the ONE owner of the legacy virtio PCI identity.
 *
 * A legacy virtio function on the PCI bus presents vendor 1AF4 with a
 * device ID of 0x1000 + (virtio_type - 1):
 *
 *   virtio-net   (type  1) -> 1AF4:1000
 *   virtio-blk   (type  2) -> 1AF4:1001
 *   virtio-gpu   (type 16) -> 1AF4:100F
 *   virtio-input (type 17) -> 1AF4:1010
 *
 * The real drivers (wubu_virtio_blk.c / wubu_virtio_net.c) match the
 * canonical IDs here (plus one hypervisor-alternate ID, disambiguated by
 * the PCI class). The driver REGISTRY (wubu_drv_virtio.c) and its test
 * harness use these same constants so the two models cannot drift again.
 *
 * C11.
 */
#ifndef WUBU_VIRTIO_IDS_H
#define WUBU_VIRTIO_IDS_H

#define WUBU_VIRTIO_VENDOR_ID        0x1AF4u
#define WUBU_VIRTIO_NET_DEVICE_ID    0x1000u
#define WUBU_VIRTIO_BLK_DEVICE_ID    0x1001u
#define WUBU_VIRTIO_GPU_DEVICE_ID    0x100Fu
#define WUBU_VIRTIO_INPUT_DEVICE_ID  0x1010u

/* OASIS VIRTIO 1.0 4.1.2: non-transitional PCI functions use 0x1040 +
 * the virtio device ID (net=1, blk=2). QEMU's modern virtio-pci uses
 * these IDs; matching only 0x1000/0x1001 misses them entirely. */
#define WUBU_VIRTIO_NET_MODERN_ID    0x1041u
#define WUBU_VIRTIO_BLK_MODERN_ID    0x1042u

#endif /* WUBU_VIRTIO_IDS_H */
