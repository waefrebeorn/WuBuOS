/*
 * wubu_virtqueue.c -- ONE owner of the legacy split-virtqueue machinery.
 *
 * Extracted verbatim-in-behaviour from the L1 virtio-blk driver: ring
 * layout, page-aligned DMA allocation, descriptor chains, avail/used
 * publishing, doorbell, polled completions. Generic over the device --
 * virtio-blk and virtio-net both drive their rings through this file.
 *
 * DMA addressing: the metal page tables identity-map the low 1 GB
 * (crt0.S PD_ident) and the kernel heap lives at phys 0x400000, so
 * heap VA == bus address there. Every ring is checked against that
 * window. The legacy queue PFN register is 32 bits, so hosted tests
 * inject a low-address arena instead of malloc.
 *
 * C11 freestanding (metal: kernel mem_alloc); the hosted test build
 * injects a fake I/O window + arena through wubu_vq_io / wubu_vq_mem.
 */
#include "wubu_virtqueue.h"

#ifdef MYSEED_METAL
void *memset(void *s, int c, size_t n);
extern void *mem_alloc(size_t);
extern void  mem_free(void *);
#else
#include <stdlib.h>
#include <string.h>
#endif

/* -- injectable port I/O (metal: real asm; test: fake window) ------ */
#ifdef MYSEED_METAL
static uint8_t  metal_inb(uint16_t p)  { uint8_t  v; __asm__ __volatile__("inb %1,%0" : "=a"(v) : "Nd"(p)); return v; }
static void     metal_outb(uint16_t p, uint8_t v)  { __asm__ __volatile__("outb %0,%1" :: "a"(v), "Nd"(p)); }
static uint16_t metal_inw(uint16_t p)  { uint16_t v; __asm__ __volatile__("inw %1,%0" : "=a"(v) : "Nd"(p)); return v; }
static void     metal_outw(uint16_t p, uint16_t v) { __asm__ __volatile__("outw %0,%1" :: "a"(v), "Nd"(p)); }
static uint32_t metal_inl(uint16_t p)  { uint32_t v; __asm__ __volatile__("inl %1,%0" : "=a"(v) : "Nd"(p)); return v; }
static void     metal_outl(uint16_t p, uint32_t v) { __asm__ __volatile__("outl %0,%1" :: "a"(v), "Nd"(p)); }
wubu_vq_io_t wubu_vq_io = {
    metal_inb, metal_outb, metal_inw, metal_outw, metal_inl, metal_outl
};
#else
wubu_vq_io_t wubu_vq_io;   /* the test installs the fake window here */
#endif

/* -- injectable DMA allocator --------------------------------------- */
#ifdef MYSEED_METAL
wubu_vq_mem_t wubu_vq_mem = { mem_alloc, mem_free };
#else
wubu_vq_mem_t wubu_vq_mem = { malloc, free };
#endif

#define VQ_ALLOC wubu_vq_mem.alloc
#define VQ_FREE  wubu_vq_mem.free

static inline void vq_fence(void) { __atomic_thread_fence(__ATOMIC_SEQ_CST); }

void wubu_vq_pause(void)
{
#if defined(__x86_64__) || defined(__i386__)
    __asm__ __volatile__("pause");
#else
    vq_fence();
#endif
}

/* -- ring layout arithmetic (legacy: used ring page-aligned) ------- */
void wubu_vq_layout(uint16_t q, uint32_t *avail_off,
                    uint32_t *used_off, uint32_t *total)
{
    uint32_t desc_bytes = 16u * q;
    uint32_t avail_end  = desc_bytes + 6u + 2u * q;
    *avail_off = desc_bytes;
    *used_off  = (avail_end + 4095u) & ~4095u;
    *total     = *used_off + 6u + 8u * q;
}

/* DMA window check: metal identity map covers [0, 1 GB). */
int wubu_vq_dma_ok(uint64_t pa, uint64_t bytes)
{
#ifdef MYSEED_METAL
    return pa + bytes <= 0x40000000ull;
#else
    (void)pa; (void)bytes;
    return 1;   /* hosted fake device shares the test address space */
#endif
}

/* -- device-level legacy bring-up ----------------------------------- */
#define VQ_IDENT_LIMIT 100000u

int wubu_vq_dev_reset(uint16_t iobase)
{
    wubu_vq_io.outb((uint16_t)(iobase + WUBU_VQ_REG_STATUS), 0);
    uint32_t i;
    for (i = 0; i < VQ_IDENT_LIMIT; i++) {
        if (wubu_vq_io.inb((uint16_t)(iobase + WUBU_VQ_REG_STATUS)) == 0)
            break;
        wubu_vq_pause();
    }
    if (i == VQ_IDENT_LIMIT) return -1;
    wubu_vq_io.outb((uint16_t)(iobase + WUBU_VQ_REG_STATUS),
                    WUBU_VQ_S_ACK | WUBU_VQ_S_DRIVER);
    return 0;
}

uint32_t wubu_vq_negotiate(uint16_t iobase, uint32_t accept_mask)
{
    uint32_t host = wubu_vq_io.inl((uint16_t)(iobase + WUBU_VQ_REG_HOST_FEATURES));
    uint32_t guest = host & accept_mask;
    wubu_vq_io.outl((uint16_t)(iobase + WUBU_VQ_REG_GUEST_FEATURES), guest);
    return guest;
}

int wubu_vq_dev_driver_ok(uint16_t iobase)
{
    wubu_vq_io.outb((uint16_t)(iobase + WUBU_VQ_REG_STATUS),
                    WUBU_VQ_S_ACK | WUBU_VQ_S_DRIVER | WUBU_VQ_S_DRIVER_OK);
    uint8_t st = wubu_vq_io.inb((uint16_t)(iobase + WUBU_VQ_REG_STATUS));
    if (!(st & WUBU_VQ_S_DRIVER_OK) || (st & WUBU_VQ_S_FAILED)) return -1;
    return 0;
}

/* -- queue setup ----------------------------------------------------- */
int wubu_vq_setup(wubu_vq_t *vq, uint16_t iobase, uint16_t index,
                  uint32_t tail_bytes, uint16_t max_q)
{
    memset(vq, 0, sizeof(*vq));
    vq->iobase = iobase;
    vq->index  = index;

    wubu_vq_io.outw((uint16_t)(iobase + WUBU_VQ_REG_QUEUE_SEL), index);
    uint16_t q = wubu_vq_io.inw((uint16_t)(iobase + WUBU_VQ_REG_QUEUE_NUM));
    if (q == 0 || q > max_q) return -1;
    vq->qsize = q;

    uint32_t avail_off, used_off, total;
    wubu_vq_layout(q, &avail_off, &used_off, &total);
    uint32_t tail = (total + 15u) & ~15u;   /* device-private DMA tail */
    vq->ring_pages = (tail + tail_bytes + 4095u) / 4096u;
    size_t ring_bytes = (size_t)vq->ring_pages * 4096u;
    vq->ring_base = (uint8_t *)VQ_ALLOC(ring_bytes + 4096u);
    if (!vq->ring_base) return -1;
    vq->ring = (uint8_t *)(((uintptr_t)vq->ring_base + 4095u) & ~(uintptr_t)4095u);
    memset(vq->ring, 0, ring_bytes);
    if (!wubu_vq_dma_ok((uint64_t)(uintptr_t)vq->ring, ring_bytes)) {
        VQ_FREE(vq->ring_base);
        vq->ring_base = NULL;
        return -1;
    }
    vq->desc     = (wubu_vq_desc_t *)vq->ring;
    vq->avail    = (wubu_vq_avail_t *)(vq->ring + avail_off);
    vq->used     = (wubu_vq_used_t *)(vq->ring + used_off);
    vq->tail_off = tail;

    wubu_vq_io.outl((uint16_t)(iobase + WUBU_VQ_REG_QUEUE_PFN),
                    (uint32_t)((uint64_t)(uintptr_t)vq->ring >> 12));
    return 0;
}

void wubu_vq_teardown(wubu_vq_t *vq)
{
    if (!vq) return;
    if (vq->ring_base) VQ_FREE(vq->ring_base);
    memset(vq, 0, sizeof(*vq));
}

uint8_t *wubu_vq_tail(const wubu_vq_t *vq)
{
    return vq->ring + vq->tail_off;
}

/* -- submission / completion ----------------------------------------- */
void wubu_vq_chain(wubu_vq_t *vq, uint16_t head,
                   const wubu_vq_seg_t *segs, uint32_t n)
{
    for (uint32_t i = 0; i < n; i++) {
        uint16_t slot = (uint16_t)((head + i) % vq->qsize);
        uint16_t flags = (uint16_t)(segs[i].device_writable ? WUBU_VQ_DESC_F_WRITE : 0);
        if (i + 1 < n) {
            flags |= WUBU_VQ_DESC_F_NEXT;
            vq->desc[slot].next = (uint16_t)((head + i + 1) % vq->qsize);
        } else {
            vq->desc[slot].next = 0;
        }
        vq->desc[slot].addr  = segs[i].addr;
        vq->desc[slot].len   = segs[i].len;
        vq->desc[slot].flags = flags;
    }
}

void wubu_vq_publish(wubu_vq_t *vq, uint16_t head)
{
    vq->avail->ring[vq->avail->idx % vq->qsize] = head;
    vq_fence();
    vq->avail->idx = (uint16_t)(vq->avail->idx + 1);
    vq_fence();
}

void wubu_vq_kick(wubu_vq_t *vq)
{
    wubu_vq_io.outw((uint16_t)(vq->iobase + WUBU_VQ_REG_QUEUE_NOTIFY),
                    vq->index);
}

int wubu_vq_poll(wubu_vq_t *vq, uint32_t limit,
                 uint32_t *id, uint32_t *len)
{
    for (uint32_t i = 0; i < limit; i++) {
        if (vq->used->idx != vq->last_used) {
            uint16_t slot = (uint16_t)(vq->last_used % vq->qsize);
            uint32_t cid = vq->used->ring[slot].id;
            uint32_t clen = vq->used->ring[slot].len;
            vq->last_used = (uint16_t)(vq->last_used + 1);
            /* ack the ISR (read-to-clear; harmless when polled) */
            (void)wubu_vq_io.inb((uint16_t)(vq->iobase + WUBU_VQ_REG_ISR));
            if (id)  *id  = cid;
            if (len) *len = clen;
            return 0;
        }
        wubu_vq_pause();
    }
    return -1;
}