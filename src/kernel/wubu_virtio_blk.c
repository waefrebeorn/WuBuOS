/*
 * wubu_virtio_blk.c -- legacy virtio-pci block driver (L1: real storage).
 *
 * ONE concern: a legacy virtio-blk PCI function (1AF4:1001, class 01)
 * as a real persistent block device. BAR0 is the legacy I/O-port
 * register window (virtio 0.9.5 §2.3); the split-virtqueue machinery
 * (ring layout/alloc, desc chains, avail/used, doorbell, polled
 * completions) is OWNED by wubu_virtqueue.c -- this file keeps only the
 * block-specific request encoding, capacity config and bounce/chunk
 * policy. ONE request in flight, polled completions -- a boot-volume
 * backend, not a throughput engine.
 *
 * DMA addressing: the metal page tables identity-map the low 1 GB
 * (crt0.S PD_ident), and the kernel heap lives at phys 0x400000, so
 * heap VA == bus address there. FAT32 hands us STACK buffers too
 * (higher half), so all data bounces through a heap buffer that IS in
 * the window.
 *
 * C11 freestanding (metal: kernel mem_alloc/klog); the hosted test
 * build injects a fake I/O window + fake PCI through the same seams.
 */
#include "wubu_virtio_blk.h"
#include "wubu_virtqueue.h"
#include "wubu_pci.h"

#ifdef MYSEED_METAL
#include "klog.h"
void *memset(void *s, int c, size_t n);
void *memcpy(void *dest, const void *src, size_t n);
extern void *mem_alloc(size_t);
extern void  mem_free(void *);
#define VBLK_LOG   klog_printf
#else
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define VBLK_LOG   printf
#endif

#define VBLK_F_FLUSH    (1u << 9)   /* the only optional feature we use */

#define VBLK_T_IN       0u
#define VBLK_T_OUT      1u
#define VBLK_T_FLUSH    4u
#define VBLK_S_OK       0u

#define VBLK_SECTOR       512u
#define VBLK_BOUNCE_SECT  128u                     /* 64 KiB bounce */
#define VBLK_MAX_Q        1024u
#define VBLK_POLL_LIMIT   5000000u                 /* ~tens of ms */

/* -- block request header (device-specific encoding) --------------- */
typedef struct __attribute__((packed)) {
    uint32_t type;
    uint32_t ioprio;
    uint64_t sector;
} vblk_req_hdr_t;

struct wubu_virtio_blk {
    uint16_t iobase;
    uint16_t cfg_off;
    uint32_t features;          /* negotiated guest features */
    uint64_t capacity;          /* sectors */
    wubu_vq_t vq;               /* the ONE split virtqueue (shared core) */
    vblk_req_hdr_t *hdr;        /* in the ring region tail (DMA-able) */
    volatile uint8_t *status;   /* ditto */
    uint8_t *bounce;            /* heap bounce buffer (DMA-able) */
    int ready;
};

/* -- one request: encode, doorbell, poll completion ---------------- */
static int vblk_submit(wubu_virtio_blk_t *d, uint32_t type, uint64_t sector,
                       uint32_t nbytes, int data_in)
{
    d->hdr->type = type;
    d->hdr->ioprio = 0;
    d->hdr->sector = sector;
    *d->status = 0xFF;                      /* sentinel: device must write */

    /* desc chain: [header] -> [data]? -> [status] */
    wubu_vq_seg_t segs[3];
    uint32_t n = 0;
    segs[n].addr = (uint64_t)(uintptr_t)d->hdr;
    segs[n].len  = sizeof(vblk_req_hdr_t);
    segs[n].device_writable = 0;
    n++;
    if (nbytes) {
        segs[n].addr = (uint64_t)(uintptr_t)d->bounce;
        segs[n].len  = nbytes;
        segs[n].device_writable = data_in;
        n++;
    }
    segs[n].addr = (uint64_t)(uintptr_t)d->status;
    segs[n].len  = 1;
    segs[n].device_writable = 1;
    n++;

    wubu_vq_chain(&d->vq, 0, segs, n);
    wubu_vq_publish(&d->vq, 0);
    wubu_vq_kick(&d->vq);

    uint32_t id = 0, len = 0;
    if (wubu_vq_poll(&d->vq, VBLK_POLL_LIMIT, &id, &len) != 0) {
        VBLK_LOG("virtio-blk: request timeout (type=%u sector=%u)\n",
                 (unsigned)type, (unsigned)sector);
        return -1;
    }
    if (id != 0 || *d->status != VBLK_S_OK) return -1;
    return 0;
}

/* -- public sector I/O (bounce + chunk) ---------------------------- */
int wubu_virtio_blk_read(wubu_virtio_blk_t *d, uint64_t lba,
                         uint32_t n, void *buf)
{
    if (!d || !d->ready || !buf) return -1;
    if (n == 0) return 0;
    if (lba >= d->capacity || n > d->capacity - lba) return -1;
    uint8_t *dst = (uint8_t *)buf;
    while (n) {
        uint32_t chunk = n > VBLK_BOUNCE_SECT ? VBLK_BOUNCE_SECT : n;
        if (vblk_submit(d, VBLK_T_IN, lba, chunk * VBLK_SECTOR, 1) != 0)
            return -1;
        memcpy(dst, d->bounce, chunk * VBLK_SECTOR);
        dst += chunk * VBLK_SECTOR;
        lba += chunk;
        n -= chunk;
    }
    return 0;
}

int wubu_virtio_blk_write(wubu_virtio_blk_t *d, uint64_t lba,
                          uint32_t n, const void *buf)
{
    if (!d || !d->ready || !buf) return -1;
    if (n == 0) return 0;
    if (lba >= d->capacity || n > d->capacity - lba) return -1;
    const uint8_t *src = (const uint8_t *)buf;
    while (n) {
        uint32_t chunk = n > VBLK_BOUNCE_SECT ? VBLK_BOUNCE_SECT : n;
        memcpy(d->bounce, src, chunk * VBLK_SECTOR);
        if (vblk_submit(d, VBLK_T_OUT, lba, chunk * VBLK_SECTOR, 0) != 0)
            return -1;
        src += chunk * VBLK_SECTOR;
        lba += chunk;
        n -= chunk;
    }
    return 0;
}

int wubu_virtio_blk_flush(wubu_virtio_blk_t *d)
{
    if (!d || !d->ready) return -1;
    if (!(d->features & VBLK_F_FLUSH)) return 0;  /* write-through */
    return vblk_submit(d, VBLK_T_FLUSH, 0, 0, 0);
}

uint64_t wubu_virtio_blk_capacity(const wubu_virtio_blk_t *d)
{
    return (d && d->ready) ? d->capacity : 0;
}

int wubu_virtio_blk_fat32_read(void *ctx, uint64_t lba, uint32_t n, void *buf)
{
    return wubu_virtio_blk_read((wubu_virtio_blk_t *)ctx, lba, n, buf);
}

int wubu_virtio_blk_fat32_write(void *ctx, uint64_t lba, uint32_t n,
                                const void *buf)
{
    return wubu_virtio_blk_write((wubu_virtio_blk_t *)ctx, lba, n, buf);
}

/* -- init ---------------------------------------------------------- */
static void vblk_teardown(wubu_virtio_blk_t *d)
{
    if (!d) return;
    if (d->bounce) wubu_vq_mem.free(d->bounce);
    wubu_vq_teardown(&d->vq);
    wubu_vq_mem.free(d);
}

static wubu_virtio_blk_t *vblk_init(const wubu_pci_dev_t *pci)
{
    uint32_t bar0_raw = wubu_pci_read32(pci->bus, pci->dev, pci->fn, 0x10);
    if (!(bar0_raw & 1u)) return NULL;           /* need the I/O layout */

    wubu_virtio_blk_t *d = (wubu_virtio_blk_t *)wubu_vq_mem.alloc(sizeof(*d));
    if (!d) return NULL;
    memset(d, 0, sizeof(*d));
    d->iobase  = (uint16_t)(bar0_raw & 0xFFFCu);
    /* Legacy config window stays 0x14: it shifts to 0x18 ONLY when the
     * driver ENABLES MSI-X; this driver is polled and never does. */
    d->cfg_off = WUBU_VQ_REG_CONFIG;

    if (wubu_vq_dev_reset(d->iobase) != 0) { vblk_teardown(d); return NULL; }

    /* feature negotiation: accept only what we implement (FLUSH) */
    d->features = wubu_vq_negotiate(d->iobase, VBLK_F_FLUSH);

    /* queue 0: split ring + tail for the request header + status byte */
    uint32_t tail_bytes = (uint32_t)(sizeof(vblk_req_hdr_t) + 1);
    if (wubu_vq_setup(&d->vq, d->iobase, 0, tail_bytes, VBLK_MAX_Q) != 0) {
        vblk_teardown(d); return NULL;
    }
    d->hdr    = (vblk_req_hdr_t *)wubu_vq_tail(&d->vq);
    d->status = (volatile uint8_t *)(wubu_vq_tail(&d->vq) + sizeof(vblk_req_hdr_t));

    if (wubu_vq_dev_driver_ok(d->iobase) != 0) {
        vblk_teardown(d); return NULL;
    }

    /* block capacity: the 64-bit config field */
    uint64_t lo = wubu_vq_io.inl((uint16_t)(d->iobase + d->cfg_off));
    uint64_t hi = wubu_vq_io.inl((uint16_t)(d->iobase + d->cfg_off + 4));
    d->capacity = lo | (hi << 32);
    if (d->capacity == 0) { vblk_teardown(d); return NULL; }

    d->bounce = (uint8_t *)wubu_vq_mem.alloc(VBLK_BOUNCE_SECT * VBLK_SECTOR);
    if (!d->bounce ||
        !wubu_vq_dma_ok((uint64_t)(uintptr_t)d->bounce,
                        VBLK_BOUNCE_SECT * VBLK_SECTOR)) {
        vblk_teardown(d); return NULL;
    }

    d->ready = 1;
    /* sanity I/O: sector 0 must round-trip through the real ring */
    uint8_t probe_buf[VBLK_SECTOR];
    if (wubu_virtio_blk_read(d, 0, 1, probe_buf) != 0) {
        d->ready = 0;
        vblk_teardown(d); return NULL;
    }
    VBLK_LOG("virtio-blk: %u MB (%u sectors, q=%u feat=%x) -- persistent disk live\n",
             (unsigned)(d->capacity >> 11), (unsigned)d->capacity,
             (unsigned)d->vq.qsize, (unsigned)d->features);
    return d;
}

wubu_virtio_blk_t *wubu_virtio_blk_probe(void)
{
    wubu_pci_dev_t devs[WUBU_PCI_MAX_DEVS];
    memset(devs, 0, sizeof(devs));
    int n = wubu_pci_scan(devs, WUBU_PCI_MAX_DEVS);
    for (int i = 0; i < n; i++) {
        /* Legacy virtio-blk: QEMU/virtio-spec ID is 0x1001 (0x1000 +
         * type - 1, block type 2); some hypervisors (VirtualBox) use
         * 0x1000. The mass-storage class disambiguates blk from net
         * (legacy net is 0x1000 on spec-correct machines). */
        if (devs[i].vendor == 0x1AF4u &&
            (devs[i].device == 0x1001u || devs[i].device == 0x1000u) &&
            devs[i].class_code == 0x01u) {
            wubu_virtio_blk_t *d = vblk_init(&devs[i]);
            if (d) return d;
        }
    }
    return NULL;
}