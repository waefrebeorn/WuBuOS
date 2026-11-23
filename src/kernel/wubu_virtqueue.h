/*
 * wubu_virtqueue.h -- ONE owner of the legacy split-virtqueue machinery.
 *
 * Extracted from the L1 virtio-blk driver (wubu_virtio_blk.c) so every
 * legacy virtio-pci function (blk, net, ...) shares ONE ring implementation:
 * layout arithmetic, page-aligned DMA ring allocation, descriptor chains,
 * avail/used publishing, the notify doorbell and polled completions.
 * Device drivers keep only their device-specific config + request encoding.
 *
 * Legacy virtio-pci (virtio 0.9.5 §2.3): BAR0 is an I/O-port window with
 * a COMMON register layout for all device types; the device config space
 * starts at 0x14 and stays there unless MSI-X is ENABLED (these drivers
 * are polled and never enable it).
 *
 * C11 freestanding. Port I/O and DMA allocation are injectable seams:
 * metal binds real asm + the kernel heap; hosted tests bind a fake I/O
 * window + a low-address arena (the legacy queue PFN is 32 bits).
 */
#ifndef WUBU_VIRTQUEUE_H
#define WUBU_VIRTQUEUE_H

struct wubu_drv_dev;   /* forward declaration, declared outside prototypes */

#include <stdint.h>
#include <stddef.h>

/* -- legacy virtio-pci I/O window (BAR0 offsets, all device types) -- */
#define WUBU_VQ_REG_HOST_FEATURES  0x00u  /* R32 */
#define WUBU_VQ_REG_GUEST_FEATURES 0x04u  /* W32 */
#define WUBU_VQ_REG_QUEUE_PFN      0x08u  /* W32: queue phys page number */
#define WUBU_VQ_REG_QUEUE_NUM      0x0Cu  /* R16 */
#define WUBU_VQ_REG_QUEUE_SEL      0x0Eu  /* W16 */
#define WUBU_VQ_REG_QUEUE_NOTIFY   0x10u  /* W16 */
#define WUBU_VQ_REG_STATUS         0x12u  /* RW8 */
#define WUBU_VQ_REG_ISR            0x13u  /* R8  */
#define WUBU_VQ_REG_CONFIG         0x14u  /* device config (MSI-X disabled) */

#define WUBU_VQ_S_ACK       0x01u
#define WUBU_VQ_S_DRIVER    0x02u
#define WUBU_VQ_S_DRIVER_OK 0x04u
#define WUBU_VQ_S_FAILED    0x80u
#define WUBU_VQ_S_DEVICE_NEEDS_RESET 0x40u

#define WUBU_VQ_DESC_F_NEXT  0x1u
#define WUBU_VQ_DESC_F_WRITE 0x2u

/* -- split virtqueue structures (virtio 0.9.5 §2.3.2) -------------- */
typedef struct __attribute__((packed)) {
    uint64_t addr;
    uint32_t len;
    uint16_t flags;
    uint16_t next;
} wubu_vq_desc_t;

typedef struct __attribute__((packed)) {
    uint16_t flags;
    uint16_t idx;
    uint16_t ring[];
} wubu_vq_avail_t;

typedef struct __attribute__((packed)) {
    uint32_t id;
    uint32_t len;
} wubu_vq_used_elem_t;

typedef struct __attribute__((packed)) {
    uint16_t flags;
    uint16_t idx;
    wubu_vq_used_elem_t ring[];
} wubu_vq_used_t;

/* -- injectable seams ---------------------------------------------- */
typedef struct {
    uint8_t  (*inb)(uint16_t);
    void     (*outb)(uint16_t, uint8_t);
    uint16_t (*inw)(uint16_t);
    void     (*outw)(uint16_t, uint16_t);
    uint32_t (*inl)(uint16_t);
    void     (*outl)(uint16_t, uint32_t);
} wubu_vq_io_t;
extern wubu_vq_io_t wubu_vq_io;   /* metal: real ports; test: fake window */

typedef struct {
    void *(*alloc)(size_t);
    void  (*free)(void *);
} wubu_vq_mem_t;
extern wubu_vq_mem_t wubu_vq_mem; /* metal: kernel heap; test: low arena */

/* -- one split virtqueue ------------------------------------------- */
typedef struct {
    uint16_t iobase;          /* device BAR0 I/O window */
    uint16_t index;           /* queue selector (device queue number) */
    uint16_t qsize;
    uint8_t *ring_base;       /* allocator base (for free) */
    uint8_t *ring;            /* page-aligned queue region (DMA-able) */
    uint32_t ring_pages;
    uint32_t tail_off;        /* device-private DMA tail after used ring */
    wubu_vq_desc_t *desc;
    volatile wubu_vq_avail_t *avail;
    volatile wubu_vq_used_t  *used;
    uint16_t last_used;       /* next used-ring slot to consume */
    /* Modern virtio sets notify_addr; legacy uses iobase + QUEUE_NOTIFY. */
    uintptr_t notify_addr;
    uint32_t  notify_mult;
} wubu_vq_t;

/* One descriptor-chain segment: a DMA buffer + direction. */
typedef struct {
    uint64_t addr;
    uint32_t len;
    int device_writable;      /* device writes into it (RX / status) */
} wubu_vq_seg_t;

/* -- ring layout arithmetic (legacy: used ring page-aligned) ------- */
void wubu_vq_layout(uint16_t q, uint32_t *avail_off,
                    uint32_t *used_off, uint32_t *total);

/* Allocate and lay out the split ring for a queue of known size. No
 * device register is touched; legacy and modern transports both use it
 * so ring arithmetic has exactly one owner. */
/* DMA_DOMAIN authority for ring setup. Lazy-init on first ring; exposed
 * for fail-closed revoke/grant gates. */
void wubu_vq_dma_cap_init(void);
int wubu_vq_dma_cap_revoke(void);
int wubu_vq_dma_cap_grant(void);

int wubu_vq_setup_ring(wubu_vq_t *vq, uint16_t index, uint16_t qsize,
                       uint32_t tail_bytes);

/* DMA window check: metal identity map covers [0, 1 GB). */
int wubu_vq_dma_ok(uint64_t pa, uint64_t bytes);

void wubu_vq_pause(void);     /* spin-wait hint (pause on x86) */

/* -- device-level legacy bring-up (shared register dance) ---------- */
/* reset -> wait status 0 -> ACK|DRIVER. 0 ok, -1 timeout. */
int wubu_vq_dev_reset(uint16_t iobase);
/* guest features = host features & accept_mask; returns the subset. */
uint32_t wubu_vq_negotiate(uint16_t iobase, uint32_t accept_mask);
/* set DRIVER_OK and verify the device accepted it. 0 ok, -1 refused. */
int wubu_vq_dev_driver_ok(uint16_t iobase);

/* VIRTIO 1.0, transport-pci: legacy device config starts at 0x14, but
 * shifts to 0x18 when MSI-X is enabled in the MSI-X capability's Message
 * Control register (bit 15). PCI Command bit 10 is INTx disable, NOT
 * MSI-X enable. Walk the capability list from 0x34, bounded by 48 steps.
 * dev == NULL -> conservative 0x14. */
uint16_t wubu_vq_config_offset(const struct wubu_drv_dev *dev);

/* Legacy config fields wider than 32 bits (capacity) or spanning several
 * bytes (MAC) have no generation counter: read twice and accept only a
 * stable pair (VIRTIO 1.0 2.3.2). len must be 1..8. */
int wubu_vq_config_read_stable(uint16_t iobase, uint16_t cfg_off,
                               uint8_t *out, uint16_t len);

/* -- queue setup ---------------------------------------------------- */
/* Select queue `index`, read its size (<= max_q), allocate the
 * page-aligned split ring + `tail_bytes` of device-private DMA tail,
 * program the queue PFN. 0 ok, -1 failed (nothing left allocated). */
int wubu_vq_setup(wubu_vq_t *vq, uint16_t iobase, uint16_t index,
                  uint32_t tail_bytes, uint16_t max_q);
void wubu_vq_teardown(wubu_vq_t *vq);
/* Device-private DMA region after the used ring (hdr/status/buffers). */
uint8_t *wubu_vq_tail(const wubu_vq_t *vq);

/* -- submission / completion ---------------------------------------- */
/* Fill a descriptor chain of n segments starting at desc slot `head`
 * (slots wrap modulo qsize). Caller owns the head slot allocation. */
void wubu_vq_chain(wubu_vq_t *vq, uint16_t head,
                   const wubu_vq_seg_t *segs, uint32_t n);
/* Publish `head` in the avail ring (fenced). Does NOT doorbell. */
void wubu_vq_publish(wubu_vq_t *vq, uint16_t head);
/* Notify the device of queue `index`. */
void wubu_vq_kick(wubu_vq_t *vq);
/* Poll for ONE completion (bounded). On success consumes the used
 * entry, acks the ISR and reports the chain head id + device-written
 * length. 0 ok, -1 timeout (nothing consumed). */
int wubu_vq_poll(wubu_vq_t *vq, uint32_t limit,
                 uint32_t *id, uint32_t *len);

#endif /* WUBU_VIRTQUEUE_H */