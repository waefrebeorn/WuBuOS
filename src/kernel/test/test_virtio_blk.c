/* test_virtio_blk.c -- L1 unit gate: the legacy virtio-blk driver
 * against a FAKE I/O window + fake PCI function + fake RAM disk.
 *
 * House pattern (test_xhci.c): include the driver TU directly, stub
 * the metal-only PCI boundary, inject the port-I/O hooks. The fake
 * device processes the REAL split virtqueue the driver builds: it
 * walks the descriptor chain, checks the request encoding, moves the
 * data, and posts the used ring -- so ring arithmetic, encoding,
 * sector math and completion handling are all asserted by behaviour.
 */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "wubu_virtio_blk.h"
#include "wubu_virtqueue.c"   /* shared vring core (one owner) */
#include "wubu_virtio_blk.c"

static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { \
    printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); failures++; \
} } while (0)

/* ---- fake PCI: one legacy virtio-blk function at 00:03.0 ---------- */
static int fake_pci_present = 1;
static int fake_msix = 0;
static const uint16_t fake_iobase = 0xC000;

int wubu_pci_scan(wubu_pci_dev_t *d, int n)
{
    if (!fake_pci_present || n < 1) return 0;
    memset(&d[0], 0, sizeof(d[0]));
    d[0].bus = 0; d[0].dev = 3; d[0].fn = 0;
    d[0].vendor = WUBU_VIRTIO_VENDOR_ID; d[0].device = WUBU_VIRTIO_BLK_DEVICE_ID;   /* legacy blk (spec) */
    d[0].class_code = 0x01;                       /* mass storage */
    d[0].bar0 = fake_iobase;   /* scan masks the IO bit; raw read below */
    return 1;
}
int wubu_pci_find_class(wubu_pci_dev_t *d, int n, uint8_t c, uint8_t s)
{ (void)d; (void)n; (void)c; (void)s; return -1; }
uint32_t wubu_pci_read32(uint8_t b, uint8_t dv, uint8_t f, uint8_t off)
{
    (void)b; (void)dv; (void)f;
    switch (off) {
    case 0x00: return 0x10001AF4u;
    case 0x10: return (uint32_t)fake_iobase | 1u;   /* I/O BAR */
    case 0x04: return fake_msix ? (1u << 20) : 0;   /* cap list? */
    case 0x34: return fake_msix ? 0x40u : 0;        /* cap pointer */
    case 0x40: return fake_msix ? 0x11u : 0;        /* MSI-X cap id */
    default:   return 0;
    }
}
void wubu_pci_write32(uint8_t b, uint8_t d, uint8_t f, uint8_t o, uint32_t v)
{ (void)b; (void)d; (void)f; (void)o; (void)v; }

/* ---- fake device: legacy register window + RAM disk --------------- */
#define FAKE_SECTORS 2048u
static uint8_t fake_regs[0x40];
static uint8_t fake_disk[FAKE_SECTORS * 512];
static uint16_t fake_qsize = 128;
static uint32_t fake_features = (1u << 9) | (1u << 2) | (1u << 5);
static uint64_t fake_ring_pa;
static uint32_t fake_guest_features;
static uint8_t fake_isr;
static int fake_no_complete;   /* timeout injection */
static int fake_ioerr;         /* status-byte injection */
static int fake_status_fail;   /* DRIVER_OK refusal injection */

/* what the fake observed on the last request */
static int notify_count;
static uint32_t last_type, last_data_len, last_data_flags, last_chain;
static uint64_t last_sector;
static uint16_t last_head;

static void fake_process(void)
{
    notify_count++;
    if (fake_no_complete) return;
    uint8_t *ring = (uint8_t *)(uintptr_t)fake_ring_pa;
    /* the fake derives the layout from the SPEC, independently of the
     * driver: a wrong driver layout reads garbage and fails checks. */
    uint32_t avail_off = 16u * fake_qsize;
    uint32_t used_off = (avail_off + 6u + 2u * fake_qsize + 4095u) & ~4095u;
    wubu_vq_desc_t *desc = (wubu_vq_desc_t *)ring;
    wubu_vq_avail_t *av = (wubu_vq_avail_t *)(ring + avail_off);
    wubu_vq_used_t *us = (wubu_vq_used_t *)(ring + used_off);

    uint16_t head = av->ring[(uint16_t)(av->idx - 1) % fake_qsize];
    last_head = head;
    /* walk the chain */
    wubu_vq_desc_t *d0 = &desc[head];
    vblk_req_hdr_t hdr;
    memcpy(&hdr, (void *)(uintptr_t)d0->addr, sizeof(hdr));
    last_type = hdr.type;
    last_sector = hdr.sector;
    last_chain = 1;
    last_data_len = 0;
    last_data_flags = 0;
    uint8_t *data = NULL;
    uint32_t dlen = 0;
    wubu_vq_desc_t *cur = d0;
    while (cur->flags & WUBU_VQ_DESC_F_NEXT) {
        cur = &desc[cur->next];
        last_chain++;
        if (cur->len == 1 && (cur->flags & WUBU_VQ_DESC_F_WRITE)) {
            /* terminal status descriptor: device-writable, 1 byte */
            uint8_t st = (fake_ioerr || hdr.sector >= FAKE_SECTORS ||
                          hdr.sector * 512 + dlen > sizeof(fake_disk)) ? 1 : 0;
            if (!st && data) {
                if (hdr.type == VBLK_T_IN)
                    memcpy(data, fake_disk + hdr.sector * 512, dlen);
                if (hdr.type == VBLK_T_OUT)
                    memcpy(fake_disk + hdr.sector * 512, data, dlen);
            }
            *(uint8_t *)(uintptr_t)cur->addr = st;
        } else {
            /* data descriptor (IN: NEXT|WRITE, OUT: NEXT) */
            data = (uint8_t *)(uintptr_t)cur->addr;
            dlen = cur->len;
            last_data_len = dlen;
            last_data_flags = cur->flags;
        }
    }
    us->ring[us->idx % fake_qsize].id = head;
    us->ring[us->idx % fake_qsize].len = dlen + 1;
    us->idx = (uint16_t)(us->idx + 1);
    fake_isr = 1;
}

static uint8_t f_inb(uint16_t p)
{
    uint16_t o = (uint16_t)(p - fake_iobase);
    if (o == WUBU_VQ_REG_ISR) { uint8_t v = fake_isr; fake_isr = 0; return v; }
    if (o == WUBU_VQ_REG_STATUS && fake_status_fail)
        return (uint8_t)(fake_regs[o] | WUBU_VQ_S_FAILED);
    return fake_regs[o];
}
static void f_outb(uint16_t p, uint8_t v) { fake_regs[p - fake_iobase] = v; }
static uint16_t f_inw(uint16_t p)
{
    uint16_t o = (uint16_t)(p - fake_iobase);
    if (o == WUBU_VQ_REG_QUEUE_NUM) return fake_qsize;
    uint16_t v; memcpy(&v, fake_regs + o, 2); return v;
}
static void f_outw(uint16_t p, uint16_t v)
{
    uint16_t o = (uint16_t)(p - fake_iobase);
    if (o == WUBU_VQ_REG_QUEUE_NOTIFY) { fake_process(); return; }
    memcpy(fake_regs + o, &v, 2);
}
static uint32_t f_inl(uint16_t p)
{
    uint16_t o = (uint16_t)(p - fake_iobase);
    if (o == WUBU_VQ_REG_HOST_FEATURES) return fake_features;
    uint32_t v; memcpy(&v, fake_regs + o, 4); return v;
}
static void f_outl(uint16_t p, uint32_t v)
{
    uint16_t o = (uint16_t)(p - fake_iobase);
    if (o == WUBU_VQ_REG_GUEST_FEATURES) { fake_guest_features = v; return; }
    if (o == WUBU_VQ_REG_QUEUE_PFN) { fake_ring_pa = (uint64_t)v << 12; return; }
    memcpy(fake_regs + o, &v, 4);
}

/* ---- fake DMA arena: low addresses so the 32-bit queue PFN fits --- */
static uint8_t fake_arena[4 * 1024 * 1024] __attribute__((aligned(4096)));
static size_t fake_arena_used;
static void *arena_alloc(size_t n)
{
    n = (n + 15) & ~(size_t)15;
    if (fake_arena_used + n > sizeof(fake_arena)) return NULL;
    void *p = fake_arena + fake_arena_used;
    fake_arena_used += n;
    return p;
}
static void arena_free(void *p) { (void)p; }   /* arena: reset per probe */

static void fake_reset(uint16_t qsize, uint32_t features, int msix)
{
    memset(fake_regs, 0, sizeof(fake_regs));
    memset(fake_disk, 0, sizeof(fake_disk));
    fake_arena_used = 0;
    wubu_vq_mem.alloc = arena_alloc;
    wubu_vq_mem.free = arena_free;
    fake_qsize = qsize;
    fake_features = features;
    fake_msix = msix;
    fake_ring_pa = 0;
    fake_guest_features = 0xFFFFFFFFu;
    fake_isr = 0;
    fake_no_complete = fake_ioerr = fake_status_fail = 0;
    notify_count = 0;
    /* capacity at the legacy config offset: MSI-X is never ENABLED by
     * the polled driver, so the window stays 0x14 even with the cap */
    uint16_t cfg = WUBU_VQ_REG_CONFIG;
    uint32_t lo = FAKE_SECTORS, hi = 0;
    memcpy(fake_regs + cfg, &lo, 4);
    memcpy(fake_regs + cfg + 4, &hi, 4);
    wubu_vq_io.inb = f_inb; wubu_vq_io.outb = f_outb;
    wubu_vq_io.inw = f_inw; wubu_vq_io.outw = f_outw;
    wubu_vq_io.inl = f_inl; wubu_vq_io.outl = f_outl;
}

int main(void)
{
    printf("test_virtio_blk: L1 legacy virtio-blk unit gate\n");

    /* 1. ring layout arithmetic (pure function, spec values) */
    uint32_t ao, uo, tot;
    wubu_vq_layout(128, &ao, &uo, &tot);
    CHECK(ao == 2048 && uo == 4096 && tot == 4096 + 6 + 8 * 128);
    wubu_vq_layout(256, &ao, &uo, &tot);
    CHECK(ao == 4096 && uo == 8192 && tot == 8192 + 6 + 8 * 256);

    /* 2. probe: negotiation + ring programming + capacity */
    fake_reset(128, (1u << 9) | (1u << 2) | (1u << 5), 0);
    wubu_virtio_blk_t *d = wubu_virtio_blk_probe();
    CHECK(d != NULL);
    if (!d) { printf("test_virtio_blk: probe died, aborting\n"); return 1; }
    CHECK(wubu_virtio_blk_capacity(d) == FAKE_SECTORS);
    CHECK(d->vq.qsize == 128);
    CHECK(fake_guest_features == (1u << 9));      /* only FLUSH accepted */
    CHECK(fake_regs[WUBU_VQ_REG_STATUS] ==
          (WUBU_VQ_S_ACK | WUBU_VQ_S_DRIVER | WUBU_VQ_S_DRIVER_OK));
    CHECK(fake_ring_pa == (uint64_t)(uintptr_t)d->vq.ring);
    CHECK(((uintptr_t)d->vq.ring & 4095) == 0);      /* page-aligned queue */
    CHECK((uint8_t *)d->vq.desc == d->vq.ring);
    CHECK((uint8_t *)d->vq.avail - d->vq.ring == 2048);
    CHECK((uint8_t *)d->vq.used - d->vq.ring == 4096);
    CHECK((uint8_t *)d->hdr >= d->vq.ring + 4096 + 6 + 8 * 128);
    CHECK(d->vq.avail->flags == 0);                  /* no interrupt spam */
    CHECK(notify_count == 1);                     /* probe sanity read */
    CHECK(last_type == VBLK_T_IN && last_sector == 0);

    /* 3. write: request encoding + descriptor flags + data landing */
    uint8_t wbuf[3 * 512], rbuf[3 * 512];
    for (int i = 0; i < 3 * 512; i++) wbuf[i] = (uint8_t)(i * 7 + 3);
    CHECK(wubu_virtio_blk_write(d, 100, 3, wbuf) == 0);
    CHECK(last_type == VBLK_T_OUT);
    CHECK(last_sector == 100);
    CHECK(last_data_len == 3 * 512);
    CHECK(last_chain == 3);                       /* hdr -> data -> status */
    CHECK(!(last_data_flags & WUBU_VQ_DESC_F_WRITE));/* OUT data is read-only */
    CHECK((last_data_flags & WUBU_VQ_DESC_F_NEXT) != 0);
    CHECK(memcmp(fake_disk + 100 * 512, wbuf, 3 * 512) == 0);

    /* 4. read back through the ring (IN data must be device-writable) */
    memset(rbuf, 0, sizeof(rbuf));
    CHECK(wubu_virtio_blk_read(d, 100, 3, rbuf) == 0);
    CHECK(last_type == VBLK_T_IN);
    CHECK((last_data_flags & WUBU_VQ_DESC_F_WRITE) != 0);
    CHECK(memcmp(rbuf, wbuf, 3 * 512) == 0);

    /* 5. sector math: out-of-range refused BEFORE any doorbell */
    int nc = notify_count;
    CHECK(wubu_virtio_blk_read(d, FAKE_SECTORS, 1, rbuf) == -1);
    CHECK(wubu_virtio_blk_write(d, FAKE_SECTORS - 1, 2, wbuf) == -1);
    CHECK(wubu_virtio_blk_read(d, 0, 0, rbuf) == 0);
    CHECK(notify_count == nc);

    /* 6. chunking: 300 sectors -> 128 + 128 + 44, data intact */
    uint8_t *big = malloc(300 * 512);
    uint8_t *big2 = malloc(300 * 512);
    CHECK(big && big2);
    for (int i = 0; i < 300 * 512; i++) big[i] = (uint8_t)(i ^ 0xA5);
    nc = notify_count;
    CHECK(wubu_virtio_blk_write(d, 0, 300, big) == 0);
    CHECK(notify_count == nc + 3);
    CHECK(wubu_virtio_blk_read(d, 0, 300, big2) == 0);
    CHECK(memcmp(big, big2, 300 * 512) == 0);
    CHECK(memcmp(fake_disk, big, 300 * 512) == 0);
    free(big); free(big2);

    /* 7. used-ring wrap: > qsize requests, idx arithmetic survives */
    nc = notify_count;
    for (int i = 0; i < 300; i++) {
        uint8_t one[512];
        memset(one, (uint8_t)i, 512);
        CHECK(wubu_virtio_blk_write(d, 500 + (i % 100), 1, one) == 0);
    }
    CHECK(notify_count == nc + 300);
    CHECK(d->vq.used->idx == (uint16_t)(nc + 300));
    CHECK(d->vq.last_used == d->vq.used->idx);

    /* 8. completion handling: device error status -> -1, then recovery */
    CHECK(wubu_virtio_blk_write(d, 1000, 3, wbuf) == 0);
    fake_ioerr = 1;
    CHECK(wubu_virtio_blk_write(d, 10, 1, wbuf) == -1);
    fake_ioerr = 0;
    CHECK(wubu_virtio_blk_read(d, 1000, 3, rbuf) == 0);
    CHECK(memcmp(rbuf, wbuf, 3 * 512) == 0);

    /* 9. FLUSH: encoded as type 4 when negotiated */
    nc = notify_count;
    CHECK(wubu_virtio_blk_flush(d) == 0);
    CHECK(notify_count == nc + 1);
    CHECK(last_type == VBLK_T_FLUSH);
    CHECK(last_chain == 2);                       /* hdr -> status, no data */

    /* 10. timeout: device never completes -> -1 (bounded poll) */
    fake_no_complete = 1;
    CHECK(wubu_virtio_blk_read(d, 0, 1, rbuf) == -1);
    fake_no_complete = 0;

    /* 11. absent device / refused DRIVER_OK -> NULL, no crash */
    vblk_teardown(d);
    fake_pci_present = 0;
    CHECK(wubu_virtio_blk_probe() == NULL);
    fake_pci_present = 1;
    fake_reset(128, 1u << 9, 0);
    fake_status_fail = 1;
    CHECK(wubu_virtio_blk_probe() == NULL);

    /* 12. no FLUSH feature: flush is a write-through no-op success */
    fake_reset(128, 0, 0);
    d = wubu_virtio_blk_probe();
    CHECK(d != NULL);
    nc = notify_count;
    CHECK(wubu_virtio_blk_flush(d) == 0);
    CHECK(notify_count == nc);
    CHECK(fake_guest_features == 0);
    vblk_teardown(d);

    /* 13. qsize 256 + MSI-X cap present (not enabled): config stays
     * 0x14, layout still right */
    fake_reset(256, 1u << 9, 1);
    d = wubu_virtio_blk_probe();
    CHECK(d != NULL);
    CHECK(d->vq.qsize == 256);
    CHECK(d->cfg_off == WUBU_VQ_REG_CONFIG);
    CHECK(wubu_virtio_blk_capacity(d) == FAKE_SECTORS);
    CHECK((uint8_t *)d->vq.avail - d->vq.ring == 4096);
    CHECK((uint8_t *)d->vq.used - d->vq.ring == 8192);
    CHECK(wubu_virtio_blk_write(d, 7, 1, wbuf) == 0);
    CHECK(wubu_virtio_blk_read(d, 7, 1, rbuf) == 0);
    CHECK(memcmp(rbuf, wbuf, 512) == 0);
    vblk_teardown(d);

    if (failures == 0) printf("test_virtio_blk: ALL PASS\n");
    else printf("test_virtio_blk: %d FAILURES\n", failures);
    return failures ? 1 : 0;
}