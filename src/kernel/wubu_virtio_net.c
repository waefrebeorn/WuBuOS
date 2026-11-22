/*
 * wubu_virtio_net.c -- legacy virtio-pci network driver (L5).
 *
 * ONE concern: a legacy virtio-net PCI function (1AF4:1000, class 02)
 * as a real Ethernet NIC. BAR0 is the legacy I/O-port register window;
 * ALL split-virtqueue machinery is owned by wubu_virtqueue.c (shared
 * with virtio-blk -- one owner, not a copy). This file keeps only the
 * net-specific parts: MAC from the device config, the legacy
 * virtio_net_hdr prepend/strip, posted RX buffers and polled TX.
 *
 * Queues: RX = 0 (WUBU_VNET_RX_BUFS posted device-writable buffers),
 * TX = 1 (one frame in flight, bounced through a DMA-able heap buffer
 * so stack callers are safe). No optional offloads negotiated: the
 * stack computes every checksum in software, so the header is the
 * plain 10-byte legacy virtio_net_hdr (12 with MRG_RXBUF, which we do
 * not accept).
 *
 * C11 freestanding (metal: kernel heap/klog); the hosted test build
 * injects a fake I/O window + fake PCI through the core seams.
 */
#include "wubu_virtio_net.h"
#include "wubu_virtio_ids.h"
#include "wubu_virtqueue.h"
#include "wubu_drv.h"          /* one registry + the single I/O-BAR owner */

#ifdef MYSEED_METAL
#include "klog.h"
void *memset(void *s, int c, size_t n);
void *memcpy(void *dest, const void *src, size_t n);
#define VNET_LOG   klog_printf
#else
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define VNET_LOG   printf
#endif

/* -- virtio-net feature bits (0.9.5 §5.1.3) ------------------------- */
#define VNET_F_MAC        (1u << 5)   /* config MAC field is valid */
#define VNET_F_MRG_RXBUF  (1u << 15)  /* NOT accepted: keep 10-byte hdr */

/* Legacy virtio_net_hdr (no VERSION_1, no MRG_RXBUF): 10 bytes. */
typedef struct __attribute__((packed)) {
    uint8_t  flags;
    uint8_t  gso_type;
    uint16_t hdr_len;
    uint16_t gso_size;
    uint16_t csum_start;
    uint16_t csum_offset;
    /* uint16_t num_buffers -- only present with MRG_RXBUF */
} vnet_hdr_t;

#define VNET_RX_BUFS     16u
#define VNET_RX_BUF_SZ   2048u
#define VNET_MAX_Q       1024u
#define VNET_TX_POLL     5000000u     /* ~tens of ms, like virtio-blk */
#define VNET_RX_POLL     20000u       /* short: recv is a poll probe */

struct wubu_virtio_net {
    uint16_t iobase;
    uint16_t cfg_off;
    uint32_t features;
    uint8_t  mac[6];
    uint16_t hdr_len;               /* 10 (or 12 with MRG_RXBUF) */
    wubu_vq_t rx;                   /* queue 0: posted RX buffers */
    wubu_vq_t tx;                   /* queue 1: one frame in flight */
    uint8_t *rx_bufs;               /* VNET_RX_BUFS * VNET_RX_BUF_SZ */
    uint32_t rx_posted;             /* bitmask of buffers in the ring */
    uint8_t *tx_bounce;             /* hdr + frame (DMA-able) */
    int ready;
};

/* -- RX buffer posting ---------------------------------------------- */
static void vnet_post_rx(wubu_virtio_net_t *n, uint16_t idx)
{
    wubu_vq_seg_t seg;
    seg.addr = (uint64_t)(uintptr_t)(n->rx_bufs + (size_t)idx * VNET_RX_BUF_SZ);
    seg.len  = VNET_RX_BUF_SZ;
    seg.device_writable = 1;
    wubu_vq_chain(&n->rx, idx, &seg, 1);
    wubu_vq_publish(&n->rx, idx);
    n->rx_posted |= (1u << idx);
}

/* -- public API ------------------------------------------------------ */
int wubu_virtio_net_get_mac(const wubu_virtio_net_t *n, uint8_t out[6])
{
    if (!n || !n->ready || !out) return -1;
    memcpy(out, n->mac, 6);
    return 0;
}

int wubu_virtio_net_send(wubu_virtio_net_t *n, const void *frame, uint32_t len)
{
    if (!n || !n->ready || !frame) return -1;
    if (len == 0 || len > WUBU_VNET_MAX_FRAME) return -1;

    /* legacy header: zeroed = no offloads, no GSO; then the frame */
    memset(n->tx_bounce, 0, n->hdr_len);
    memcpy(n->tx_bounce + n->hdr_len, frame, len);

    wubu_vq_seg_t segs[2];
    segs[0].addr = (uint64_t)(uintptr_t)n->tx_bounce;
    segs[0].len  = n->hdr_len;
    segs[0].device_writable = 0;
    segs[1].addr = (uint64_t)(uintptr_t)(n->tx_bounce + n->hdr_len);
    segs[1].len  = len;
    segs[1].device_writable = 0;
    wubu_vq_chain(&n->tx, 0, segs, 2);
    wubu_vq_publish(&n->tx, 0);
    wubu_vq_kick(&n->tx);

    uint32_t id = 0, clen = 0;
    if (wubu_vq_poll(&n->tx, VNET_TX_POLL, &id, &clen) != 0) {
        VNET_LOG("virtio-net: TX timeout (%u bytes)\n", (unsigned)len);
        return -1;
    }
    return (id == 0) ? 0 : -1;
}

int wubu_virtio_net_recv(wubu_virtio_net_t *n, void *buf, uint32_t cap,
                         uint32_t *out_len)
{
    if (!n || !n->ready || !buf || !out_len) return -1;
    uint32_t id = 0, len = 0;
    if (wubu_vq_poll(&n->rx, VNET_RX_POLL, &id, &len) != 0)
        return -1;                          /* no frame within the probe */
    if (id >= VNET_RX_BUFS || !(n->rx_posted & (1u << id)))
        return -1;                          /* device returned garbage */
    n->rx_posted &= ~(1u << id);

    uint8_t *pkt = n->rx_bufs + (size_t)id * VNET_RX_BUF_SZ;
    int rc = -1;
    if (len >= n->hdr_len && len <= VNET_RX_BUF_SZ) {
        uint32_t payload = len - n->hdr_len;
        uint32_t copy = payload < cap ? payload : cap;
        memcpy(buf, pkt + n->hdr_len, copy);
        *out_len = payload;
        rc = 0;
    }
    /* re-post the buffer whether the frame was sane or not */
    vnet_post_rx(n, (uint16_t)id);
    wubu_vq_kick(&n->rx);
    return rc;
}

/* -- init ------------------------------------------------------------- */
static void vnet_teardown(wubu_virtio_net_t *n)
{
    if (!n) return;
    if (n->rx_bufs) wubu_vq_mem.free(n->rx_bufs);
    if (n->tx_bounce) wubu_vq_mem.free(n->tx_bounce);
    wubu_vq_teardown(&n->rx);
    wubu_vq_teardown(&n->tx);
    wubu_vq_mem.free(n);
}

/* The I/O base comes from the ONE registry's I/O-BAR owner. */
static wubu_virtio_net_t *vnet_init(uint16_t iobase,
                                    const wubu_drv_dev_t *dev)
{
    if (!iobase) return NULL;
    wubu_virtio_net_t *n = (wubu_virtio_net_t *)wubu_vq_mem.alloc(sizeof(*n));
    if (!n) return NULL;
    memset(n, 0, sizeof(*n));
    n->iobase  = iobase;
    /* 0x14, or 0x18 if firmware left MSI-X enabled in PCI command. */
    n->cfg_off = wubu_vq_config_offset(dev);

    if (wubu_vq_dev_reset(n->iobase) != 0) { vnet_teardown(n); return NULL; }

    /* accept only MAC validity -- no offloads, no mergeable RX buffers:
     * the stack owns every checksum and the header stays 10 bytes. */
    n->features = wubu_vq_negotiate(n->iobase, VNET_F_MAC);
    n->hdr_len = (uint16_t)((n->features & VNET_F_MRG_RXBUF)
                            ? sizeof(vnet_hdr_t) + 2 : sizeof(vnet_hdr_t));

    /* queue 0 = RX, queue 1 = TX (legacy net, no ctrl queue negotiated) */
    if (wubu_vq_setup(&n->rx, n->iobase, 0, 0, VNET_MAX_Q) != 0 ||
        wubu_vq_setup(&n->tx, n->iobase, 1, 0, VNET_MAX_Q) != 0) {
        vnet_teardown(n); return NULL;
    }

    if (wubu_vq_dev_driver_ok(n->iobase) != 0) {
        vnet_teardown(n); return NULL;
    }

    /* MAC: the 6-byte config field, read as a stable pair. */
    if (wubu_vq_config_read_stable(n->iobase, n->cfg_off, n->mac, 6) != 0) {
        vnet_teardown(n); return NULL;
    }

    /* DMA buffers: RX arena + TX bounce (hdr + max frame) */
    n->rx_bufs = (uint8_t *)wubu_vq_mem.alloc(VNET_RX_BUFS * VNET_RX_BUF_SZ);
    n->tx_bounce = (uint8_t *)wubu_vq_mem.alloc(VNET_RX_BUF_SZ);
    if (!n->rx_bufs || !n->tx_bounce ||
        !wubu_vq_dma_ok((uint64_t)(uintptr_t)n->rx_bufs,
                        VNET_RX_BUFS * VNET_RX_BUF_SZ) ||
        !wubu_vq_dma_ok((uint64_t)(uintptr_t)n->tx_bounce, VNET_RX_BUF_SZ)) {
        vnet_teardown(n); return NULL;
    }
    memset(n->rx_bufs, 0, VNET_RX_BUFS * VNET_RX_BUF_SZ);

    /* post every RX buffer, then one doorbell */
    for (uint16_t i = 0; i < VNET_RX_BUFS; i++)
        vnet_post_rx(n, i);
    wubu_vq_kick(&n->rx);

    n->ready = 1;
    VNET_LOG("virtio-net: mac %x:%x:%x:%x:%x:%x (rxq=%u txq=%u hdr=%u) -- network live\n",
             (unsigned)n->mac[0], (unsigned)n->mac[1], (unsigned)n->mac[2],
             (unsigned)n->mac[3], (unsigned)n->mac[4], (unsigned)n->mac[5],
             (unsigned)n->rx.qsize, (unsigned)n->tx.qsize, (unsigned)n->hdr_len);
    return n;
}

wubu_virtio_net_t *wubu_virtio_net_probe(void)
{
    /* One registry, one I/O-BAR owner. Canonical net ID plus the legacy
     * alternate; the network class disambiguates net from blk. */
    static const uint16_t ids[] = { WUBU_VIRTIO_NET_DEVICE_ID, 0x1001u };
    for (unsigned i = 0; i < sizeof(ids) / sizeof(ids[0]); i++) {
        const wubu_drv_dev_t *dev =
            wubu_drv_find_pci_id(WUBU_VIRTIO_VENDOR_ID, ids[i], 0x02u);
        if (!dev) continue;
        uint16_t iobase = 0;
        if (wubu_drv_io_base(dev, &iobase) != 0) continue;
        wubu_virtio_net_t *nic = vnet_init(iobase, dev);
        if (nic) return nic;
    }
    return NULL;
}