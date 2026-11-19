/*
 * wubu_virtio_net.h -- legacy virtio-pci network device (L5: bare-metal
 * network). A legacy virtio-net PCI function (vendor 0x1AF4, device
 * 0x1000 -- spec legacy net ID; class 0x02, BAR0 I/O-port layout) with
 * ONE RX queue (posted buffers) + ONE TX queue, both driven through the
 * shared split-virtqueue core (wubu_virtqueue.h). Polled RX/TX, the
 * 10-byte legacy virtio_net_hdr prepended/stripped by this driver.
 *
 * Frames are full Ethernet frames (dst/src/type + payload, no FCS).
 * send/recv bounce through DMA-able heap memory, so callers may pass
 * stack buffers. Absent device: probe returns NULL (silent no-op).
 * C11 freestanding; no libc dependency in the metal build.
 */
#ifndef WUBU_VIRTIO_NET_H
#define WUBU_VIRTIO_NET_H

#include <stdint.h>
#include <stddef.h>

typedef struct wubu_virtio_net wubu_virtio_net_t;   /* opaque */

#define WUBU_VNET_MAX_FRAME 1514u   /* Ethernet MTU frame, no FCS */

/* Scan PCI bus 0 for a legacy virtio-net function and bring it up
 * (reset -> feature negotiation -> RX/TX virtqueues -> DRIVER_OK ->
 * MAC from config -> RX buffers posted). NULL when absent/failed. */
wubu_virtio_net_t *wubu_virtio_net_probe(void);

/* The device MAC (6 bytes). 0 ok, -1 not ready. */
int wubu_virtio_net_get_mac(const wubu_virtio_net_t *n, uint8_t out[6]);

/* Transmit one full Ethernet frame (<= WUBU_VNET_MAX_FRAME bytes).
 * Prepends the legacy virtio_net_hdr, polled completion. 0 ok, -1 err. */
int wubu_virtio_net_send(wubu_virtio_net_t *n, const void *frame, uint32_t len);

/* Receive one frame if available: strips the virtio_net_hdr, copies up
 * to `cap` bytes into buf, reports the payload length in *out_len and
 * re-posts the buffer. Returns 0 on a frame, -1 when none/error (bounded
 * poll -- safe to call in a loop). */
int wubu_virtio_net_recv(wubu_virtio_net_t *n, void *buf, uint32_t cap,
                         uint32_t *out_len);

#endif /* WUBU_VIRTIO_NET_H */