/*
 * wubu_virtio_blk.c -- legacy virtio-pci block driver (L1: real storage).
 *
 * ONE concern: a legacy virtio-blk PCI function (1AF4:1001, class 01)
 * as a real persistent block device. BAR0 is the legacy I/O-port
 * register window
 * (virtio 0.9.5 §2.3): features/queue-PFN/notify/status + device config.
 * ONE split virtqueue (desc/avail/used) in kernel memory, ONE request in
 * flight, polled completions -- a boot-volume backend, not a throughput
 * engine. Descriptor-ring discipline follows the firmware templates
 * (src/firmware/fw_nvme.c): program the ring, fence, doorbell, poll.
 *
 * DMA addressing: the metal page tables identity-map the low 1 GB
 * (crt0.S PD_ident), and the kernel heap lives at phys 0x400000, so
 * heap VA == bus address there. Every DMA region is checked against
 * that window; FAT32 hands us STACK buffers too (higher half), so all
 * data bounces through a heap buffer that IS in the window.
 *
 * C11 freestanding (metal: kernel mem_alloc/klog); the hosted test
 * build injects a fake I/O window + fake PCI through the same seams.
 */
#include "wubu_virtio_blk.h"
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

/* -- legacy virtio-pci I/O window (BAR0 offsets) ------------------- */
#define VBLK_REG_HOST_FEATURES  0x00u  /* R32 */
#define VBLK_REG_GUEST_FEATURES 0x04u  /* W32 */
#define VBLK_REG_QUEUE_PFN      0x08u  /* W32: queue phys page number */
#define VBLK_REG_QUEUE_NUM      0x0Cu  /* R16 */
#define VBLK_REG_QUEUE_SEL      0x0Eu  /* W16 */
#define VBLK_REG_QUEUE_NOTIFY   0x10u  /* W16 */
#define VBLK_REG_STATUS         0x12u  /* RW8 */
#define VBLK_REG_ISR            0x13u  /* R8  */
/* Legacy config window: 0x14. It shifts to 0x18 ONLY when the driver
 * ENABLES MSI-X; this driver is polled and never enables it, so the
 * window stays at 0x14 even on functions that carry an MSI-X cap. */
#define VBLK_REG_CONFIG         0x14u  /* device config (MSI-X disabled) */

#define VBLK_S_ACK      0x01u
#define VBLK_S_DRIVER   0x02u
#define VBLK_S_DRIVER_OK 0x04u
#define VBLK_S_FAILED   0x80u

#define VBLK_F_FLUSH    (1u << 9)   /* the only optional feature we use */

#define VBLK_T_IN       0u
#define VBLK_T_OUT      1u
#define VBLK_T_FLUSH    4u
#define VBLK_S_OK       0u

#define VBLK_DESC_F_NEXT  0x1u
#define VBLK_DESC_F_WRITE 0x2u

#define VBLK_SECTOR       512u
#define VBLK_BOUNCE_SECT  128u                     /* 64 KiB bounce */
#define VBLK_MAX_Q        1024u
#define VBLK_POLL_LIMIT   5000000u                 /* ~tens of ms */
#define VBLK_IDENT_LIMIT  100000u

/* -- split virtqueue structures (virtio 0.9.5 §2.3.2) -------------- */
typedef struct __attribute__((packed)) {
    uint64_t addr;
    uint32_t len;
    uint16_t flags;
    uint16_t next;
} vblk_desc_t;

typedef struct __attribute__((packed)) {
    uint16_t flags;
    uint16_t idx;
    uint16_t ring[];
} vblk_avail_t;

typedef struct __attribute__((packed)) {
    uint32_t id;
    uint32_t len;
} vblk_used_elem_t;

typedef struct __attribute__((packed)) {
    uint16_t flags;
    uint16_t idx;
    vblk_used_elem_t ring[];
} vblk_used_t;

typedef struct __attribute__((packed)) {
    uint32_t type;
    uint32_t ioprio;
    uint64_t sector;
} vblk_req_hdr_t;

struct wubu_virtio_blk {
    uint16_t iobase;
    uint16_t cfg_off;
    uint16_t qsize;
    uint32_t features;          /* negotiated guest features */
    uint64_t capacity;          /* sectors */
    uint8_t *ring_base;         /* allocator base (for free) */
    uint8_t *ring;              /* page-aligned queue region */
    uint32_t ring_pages;
    vblk_desc_t *desc;
    volatile vblk_avail_t *avail;
    volatile vblk_used_t  *used;
    vblk_req_hdr_t *hdr;        /* in the ring region tail (DMA-able) */
    volatile uint8_t *status;   /* ditto */
    uint8_t *bounce;            /* heap bounce buffer (DMA-able) */
    uint16_t last_used;
    int ready;
};

/* -- injectable port I/O (metal: real asm; test: fake window) ------ */
typedef struct {
    uint8_t  (*inb)(uint16_t);
    void     (*outb)(uint16_t, uint8_t);
    uint16_t (*inw)(uint16_t);
    void     (*outw)(uint16_t, uint16_t);
    uint32_t (*inl)(uint16_t);
    void     (*outl)(uint16_t, uint32_t);
} vblk_io_t;

#ifdef MYSEED_METAL
static uint8_t  metal_inb(uint16_t p)  { uint8_t  v; __asm__ __volatile__("inb %1,%0" : "=a"(v) : "Nd"(p)); return v; }
static void     metal_outb(uint16_t p, uint8_t v)  { __asm__ __volatile__("outb %0,%1" :: "a"(v), "Nd"(p)); }
static uint16_t metal_inw(uint16_t p)  { uint16_t v; __asm__ __volatile__("inw %1,%0" : "=a"(v) : "Nd"(p)); return v; }
static void     metal_outw(uint16_t p, uint16_t v) { __asm__ __volatile__("outw %0,%1" :: "a"(v), "Nd"(p)); }
static uint32_t metal_inl(uint16_t p)  { uint32_t v; __asm__ __volatile__("inl %1,%0" : "=a"(v) : "Nd"(p)); return v; }
static void     metal_outl(uint16_t p, uint32_t v) { __asm__ __volatile__("outl %0,%1" :: "a"(v), "Nd"(p)); }
static const vblk_io_t g_io = {
    metal_inb, metal_outb, metal_inw, metal_outw, metal_inl, metal_outl
};
#else
static vblk_io_t g_io;   /* the test installs the fake window here */
#endif

/* DMA arena allocator. Metal: the kernel heap (identity-mapped low
 * phys). Hosted test: a low-address arena, because the legacy queue
 * PFN register is 32 bits (queue phys must fit below 2^44) and hosted
 * malloc addresses do not. */
typedef struct {
    void *(*alloc)(size_t);
    void  (*free)(void *);
} vblk_mem_t;
#ifdef MYSEED_METAL
static const vblk_mem_t g_mem = { mem_alloc, mem_free };
#else
static vblk_mem_t g_mem = { malloc, free };
#endif
#define VBLK_ALLOC g_mem.alloc
#define VBLK_FREE  g_mem.free

static inline void vblk_fence(void) { __atomic_thread_fence(__ATOMIC_SEQ_CST); }
static inline void vblk_pause(void) {
#if defined(__x86_64__) || defined(__i386__)
    __asm__ __volatile__("pause");
#else
    vblk_fence();
#endif
}

/* -- ring layout arithmetic (legacy: used ring page-aligned) ------- */
static void vblk_ring_layout(uint16_t q, uint32_t *avail_off,
                             uint32_t *used_off, uint32_t *total)
{
    uint32_t desc_bytes = 16u * q;
    uint32_t avail_end  = desc_bytes + 6u + 2u * q;
    *avail_off = desc_bytes;
    *used_off  = (avail_end + 4095u) & ~4095u;
    *total     = *used_off + 6u + 8u * q;
}

/* DMA window check: metal identity map covers [0, 1 GB). */
static int vblk_dma_ok(uint64_t pa, uint64_t bytes)
{
#ifdef MYSEED_METAL
    return pa + bytes <= 0x40000000ull;
#else
    (void)pa; (void)bytes;
    return 1;   /* hosted fake device shares the test address space */
#endif
}

/* -- one request: encode, doorbell, poll completion ---------------- */
static int vblk_submit(wubu_virtio_blk_t *d, uint32_t type, uint64_t sector,
                       uint32_t nbytes, int data_in)
{
    uint64_t hdr_pa  = (uint64_t)(uintptr_t)d->hdr;
    uint64_t data_pa = (uint64_t)(uintptr_t)d->bounce;
    uint64_t stat_pa = (uint64_t)(uintptr_t)d->status;

    d->hdr->type = type;
    d->hdr->ioprio = 0;
    d->hdr->sector = sector;
    *d->status = 0xFF;                      /* sentinel: device must write */

    /* desc chain: [header] -> [data]? -> [status] */
    d->desc[0].addr = hdr_pa;
    d->desc[0].len  = sizeof(vblk_req_hdr_t);
    d->desc[0].flags = VBLK_DESC_F_NEXT;
    d->desc[0].next  = 1;
    uint16_t last = 1;
    if (nbytes) {
        d->desc[1].addr  = data_pa;
        d->desc[1].len   = nbytes;
        d->desc[1].flags = (uint16_t)(VBLK_DESC_F_NEXT |
                                      (data_in ? VBLK_DESC_F_WRITE : 0));
        d->desc[1].next  = 2;
        last = 2;
    }
    d->desc[last].addr  = stat_pa;
    d->desc[last].len   = 1;
    d->desc[last].flags = VBLK_DESC_F_WRITE;
    d->desc[last].next  = 0;

    d->avail->ring[d->avail->idx % d->qsize] = 0;
    vblk_fence();
    d->avail->idx = (uint16_t)(d->avail->idx + 1);
    vblk_fence();
    g_io.outw((uint16_t)(d->iobase + VBLK_REG_QUEUE_NOTIFY), 0);

    for (uint32_t i = 0; i < VBLK_POLL_LIMIT; i++) {
        if (d->used->idx != d->last_used) {
            uint16_t slot = (uint16_t)(d->last_used % d->qsize);
            uint32_t id  = d->used->ring[slot].id;
            d->last_used = (uint16_t)(d->last_used + 1);
            (void)g_io.inb((uint16_t)(d->iobase + VBLK_REG_ISR)); /* ack */
            if (id != 0 || *d->status != VBLK_S_OK) return -1;
            return 0;
        }
        vblk_pause();
    }
    VBLK_LOG("virtio-blk: request timeout (type=%u sector=%u)\n",
             (unsigned)type, (unsigned)sector);
    return -1;
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
    if (d->bounce) VBLK_FREE(d->bounce);
    if (d->ring_base) VBLK_FREE(d->ring_base);
    VBLK_FREE(d);
}

static wubu_virtio_blk_t *vblk_init(const wubu_pci_dev_t *pci)
{
    uint32_t bar0_raw = wubu_pci_read32(pci->bus, pci->dev, pci->fn, 0x10);
    if (!(bar0_raw & 1u)) return NULL;           /* need the I/O layout */

    wubu_virtio_blk_t *d = (wubu_virtio_blk_t *)VBLK_ALLOC(sizeof(*d));
    if (!d) return NULL;
    memset(d, 0, sizeof(*d));
    d->iobase  = (uint16_t)(bar0_raw & 0xFFFCu);
    d->cfg_off = VBLK_REG_CONFIG;

    /* reset */
    g_io.outb((uint16_t)(d->iobase + VBLK_REG_STATUS), 0);
    uint32_t i;
    for (i = 0; i < VBLK_IDENT_LIMIT; i++) {
        if (g_io.inb((uint16_t)(d->iobase + VBLK_REG_STATUS)) == 0) break;
        vblk_pause();
    }
    if (i == VBLK_IDENT_LIMIT) { vblk_teardown(d); return NULL; }

    g_io.outb((uint16_t)(d->iobase + VBLK_REG_STATUS), VBLK_S_ACK | VBLK_S_DRIVER);

    /* feature negotiation: accept only what we implement (FLUSH) */
    uint32_t host = g_io.inl((uint16_t)(d->iobase + VBLK_REG_HOST_FEATURES));
    d->features = host & VBLK_F_FLUSH;
    g_io.outl((uint16_t)(d->iobase + VBLK_REG_GUEST_FEATURES), d->features);

    /* queue 0: size, then the page-aligned split-ring region */
    g_io.outw((uint16_t)(d->iobase + VBLK_REG_QUEUE_SEL), 0);
    uint16_t q = g_io.inw((uint16_t)(d->iobase + VBLK_REG_QUEUE_NUM));
    if (q == 0 || q > VBLK_MAX_Q) { vblk_teardown(d); return NULL; }
    d->qsize = q;

    uint32_t avail_off, used_off, total;
    vblk_ring_layout(q, &avail_off, &used_off, &total);
    uint32_t tail = (total + 15u) & ~15u;        /* hdr + status live here */
    d->ring_pages = (tail + 32u + 4095u) / 4096u;
    size_t ring_bytes = (size_t)d->ring_pages * 4096u;
    d->ring_base = (uint8_t *)VBLK_ALLOC(ring_bytes + 4096u);
    if (!d->ring_base) { vblk_teardown(d); return NULL; }
    d->ring = (uint8_t *)(((uintptr_t)d->ring_base + 4095u) & ~(uintptr_t)4095u);
    memset(d->ring, 0, ring_bytes);
    if (!vblk_dma_ok((uint64_t)(uintptr_t)d->ring, ring_bytes)) {
        VBLK_LOG("virtio-blk: ring outside the identity DMA window\n");
        vblk_teardown(d); return NULL;
    }
    d->desc  = (vblk_desc_t *)d->ring;
    d->avail = (vblk_avail_t *)(d->ring + avail_off);
    d->used  = (vblk_used_t *)(d->ring + used_off);
    d->hdr   = (vblk_req_hdr_t *)(d->ring + tail);
    d->status = (volatile uint8_t *)(d->ring + tail + sizeof(vblk_req_hdr_t));

    g_io.outl((uint16_t)(d->iobase + VBLK_REG_QUEUE_PFN),
              (uint32_t)((uint64_t)(uintptr_t)d->ring >> 12));

    g_io.outb((uint16_t)(d->iobase + VBLK_REG_STATUS),
              VBLK_S_ACK | VBLK_S_DRIVER | VBLK_S_DRIVER_OK);
    uint8_t st = g_io.inb((uint16_t)(d->iobase + VBLK_REG_STATUS));
    if (!(st & VBLK_S_DRIVER_OK) || (st & VBLK_S_FAILED)) {
        vblk_teardown(d); return NULL;
    }

    /* block capacity: the 64-bit config field */
    uint64_t lo = g_io.inl((uint16_t)(d->iobase + d->cfg_off));
    uint64_t hi = g_io.inl((uint16_t)(d->iobase + d->cfg_off + 4));
    d->capacity = lo | (hi << 32);
    if (d->capacity == 0) { vblk_teardown(d); return NULL; }

    d->bounce = (uint8_t *)VBLK_ALLOC(VBLK_BOUNCE_SECT * VBLK_SECTOR);
    if (!d->bounce ||
        !vblk_dma_ok((uint64_t)(uintptr_t)d->bounce,
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
             (unsigned)q, (unsigned)d->features);
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