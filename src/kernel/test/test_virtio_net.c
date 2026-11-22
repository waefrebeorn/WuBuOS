/* test_virtio_net.c -- L5 unit gate: the legacy virtio-net driver
 * against a FAKE I/O window + fake PCI function + fake dual-queue
 * device. House pattern (test_virtio_blk.c): include the driver TUs
 * directly, stub the metal-only PCI boundary, inject the core seams.
 * The fake device walks the REAL split virtqueues the driver builds
 * (through the shared wubu_virtqueue core): TX chains are verified +
 * captured, RX buffers are filled by independent injection -- so ring
 * arithmetic, the legacy hdr encode/strip, buffer posting/re-posting
 * and completion handling are all asserted by behaviour.
 */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "wubu_virtio_net.h"
#include "wubu_drv.h"
#include "wubu_virtqueue.c"   /* shared vring core (one owner) */
#include "wubu_virtio_net.c"

static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { \
    printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); failures++; \
} } while (0)

/* ---- fake registry: one legacy virtio-net function at 00:04.0 ------ */
static int g_registry_present = 1;
static int g_registry_io_space = 1;     /* negative control: set to 0 */
static uint8_t fake_class = 0x02;              /* network */
static const uint16_t fake_iobase = 0xC100;
static const uint8_t fake_mac[6] = { 0x52, 0x54, 0x00, 0x12, 0x34, 0x56 };
static wubu_drv_dev_t g_fake_pci_dev;

const wubu_drv_dev_t *wubu_drv_find_pci_id(uint16_t vendor, uint16_t device,
                                            uint8_t class_code)
{
    if (!g_registry_present) return NULL;
    if (vendor != WUBU_VIRTIO_VENDOR_ID || class_code != fake_class) return NULL;
    if (device != WUBU_VIRTIO_NET_DEVICE_ID && device != 0x1001u) return NULL;
    memset(&g_fake_pci_dev, 0, sizeof(g_fake_pci_dev));
    g_fake_pci_dev.bus = WUBU_DRV_BUS_PCI;
    g_fake_pci_dev.vendor = vendor;
    g_fake_pci_dev.device = device;
    g_fake_pci_dev.class_code = class_code;
    g_fake_pci_dev.bar0 = fake_iobase;
    g_fake_pci_dev.bar0_is_io = (uint8_t)g_registry_io_space;
    return &g_fake_pci_dev;
}

int wubu_drv_io_base(const wubu_drv_dev_t *dev, uint16_t *io_out)
{
    if (!dev || !io_out) return -1;
    if (!dev->bar0_is_io || !dev->bar0 || dev->bar0 > 0xFFFFu) return -1;
    *io_out = (uint16_t)(dev->bar0 & 0xFFFCu);
    return 0;
}
static int g_msix_state;   /* 0 none, 1 enabled, 2 present-but-disabled */
uint32_t wubu_pci_read32(uint8_t b, uint8_t dv, uint8_t f, uint8_t off)
{
    (void)b; (void)dv; (void)f;
    switch (off) {
    case 0x00: return 0x10001AF4u;
    case 0x10: return (uint32_t)fake_iobase | 1u;
    case 0x04: return g_msix_state ? (1u << 20) : 0;
    case 0x34: return g_msix_state ? 0x40u : 0;
    case 0x40: return g_msix_state == 1 ? (0x11u | 0x80000000u)
                                        : (g_msix_state == 2 ? 0x11u : 0);
    default:   return 0;
    }
}
void wubu_pci_write32(uint8_t b, uint8_t d, uint8_t f, uint8_t o, uint32_t v)
{ (void)b; (void)d; (void)f; (void)o; (void)v; }

/* ---- fake device: legacy window + two queues ---------------------- */
static uint8_t fake_regs[0x40];
static uint16_t fake_qsize = 128;
static uint32_t fake_features = (1u << 5) | (1u << 0) | (1u << 15); /* MAC|CSUM|MRG */
static uint64_t fake_ring_pa[2];
static uint32_t fake_guest_features;
static uint16_t fake_sel;
static uint8_t fake_isr;
static int fake_status_fail;
static int fake_tx_notify_count;
static uint16_t fake_rx_consumed;
static uint8_t fake_tx_frame[2048];
static uint32_t fake_tx_len;
static uint8_t fake_tx_hdr[16];
static uint32_t fake_tx_hdr_len;
static uint32_t fake_tx_chain;

static void fake_process_tx(void)
{
    fake_tx_notify_count++;
    uint8_t *ring = (uint8_t *)(uintptr_t)fake_ring_pa[1];
    /* layout derived from the SPEC, independently of the driver */
    uint32_t avail_off = 16u * fake_qsize;
    uint32_t used_off = (avail_off + 6u + 2u * fake_qsize + 4095u) & ~4095u;
    wubu_vq_desc_t *desc = (wubu_vq_desc_t *)ring;
    wubu_vq_avail_t *av = (wubu_vq_avail_t *)(ring + avail_off);
    wubu_vq_used_t *us = (wubu_vq_used_t *)(ring + used_off);

    uint16_t head = av->ring[(uint16_t)(av->idx - 1) % fake_qsize];
    wubu_vq_desc_t *d0 = &desc[head];
    fake_tx_hdr_len = d0->len;
    memcpy(fake_tx_hdr, (void *)(uintptr_t)d0->addr,
           d0->len < sizeof(fake_tx_hdr) ? d0->len : sizeof(fake_tx_hdr));
    fake_tx_chain = 1;
    wubu_vq_desc_t *cur = d0;
    while (cur->flags & WUBU_VQ_DESC_F_NEXT) {
        cur = &desc[cur->next];
        fake_tx_chain++;
        fake_tx_len = cur->len;
        memcpy(fake_tx_frame, (void *)(uintptr_t)cur->addr,
               cur->len < sizeof(fake_tx_frame) ? cur->len : sizeof(fake_tx_frame));
    }
    us->ring[us->idx % fake_qsize].id = head;
    us->ring[us->idx % fake_qsize].len = fake_tx_hdr_len + fake_tx_len;
    us->idx = (uint16_t)(us->idx + 1);
    fake_isr = 1;
}

/* inject one frame into the next posted RX buffer (device side) */
static void fake_rx_inject(const uint8_t *frame, uint32_t len)
{
    uint8_t *ring = (uint8_t *)(uintptr_t)fake_ring_pa[0];
    uint32_t avail_off = 16u * fake_qsize;
    uint32_t used_off = (avail_off + 6u + 2u * fake_qsize + 4095u) & ~4095u;
    wubu_vq_desc_t *desc = (wubu_vq_desc_t *)ring;
    wubu_vq_avail_t *av = (wubu_vq_avail_t *)(ring + avail_off);
    wubu_vq_used_t *us = (wubu_vq_used_t *)(ring + used_off);
    uint16_t head = av->ring[fake_rx_consumed % fake_qsize];
    fake_rx_consumed++;
    wubu_vq_desc_t *d0 = &desc[head];
    uint8_t *buf = (uint8_t *)(uintptr_t)d0->addr;
    memset(buf, 0, 10);                     /* legacy hdr: no offloads */
    memcpy(buf + 10, frame, len);
    us->ring[us->idx % fake_qsize].id = head;
    us->ring[us->idx % fake_qsize].len = 10 + len;
    us->idx = (uint16_t)(us->idx + 1);
    fake_isr = 1;
}

/* inject a used entry with a raw (possibly malformed) length */
static void fake_rx_inject_short(void)
{
    uint8_t *ring = (uint8_t *)(uintptr_t)fake_ring_pa[0];
    uint32_t avail_off = 16u * fake_qsize;
    uint32_t used_off = (avail_off + 6u + 2u * fake_qsize + 4095u) & ~4095u;
    wubu_vq_avail_t *av = (wubu_vq_avail_t *)(ring + avail_off);
    wubu_vq_used_t *us = (wubu_vq_used_t *)(ring + used_off);
    uint16_t head = av->ring[fake_rx_consumed % fake_qsize];
    fake_rx_consumed++;
    us->ring[us->idx % fake_qsize].id = head;
    us->ring[us->idx % fake_qsize].len = 5;   /* < hdr: malformed */
    us->idx = (uint16_t)(us->idx + 1);
}

static uint8_t f_inb(uint16_t p)
{
    uint16_t o = (uint16_t)(p - fake_iobase);
    if (o == WUBU_VQ_REG_ISR) { uint8_t v = fake_isr; fake_isr = 0; return v; }
    if (o == WUBU_VQ_REG_STATUS && fake_status_fail)
        return (uint8_t)(fake_regs[o] | WUBU_VQ_S_FAILED);
    if (o >= WUBU_VQ_REG_CONFIG && o < WUBU_VQ_REG_CONFIG + 6)
        return fake_mac[o - WUBU_VQ_REG_CONFIG];
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
    if (o == WUBU_VQ_REG_QUEUE_NOTIFY) {
        if (v == 1) fake_process_tx();
        return;                             /* RX kick: buffers posted */
    }
    if (o == WUBU_VQ_REG_QUEUE_SEL) { fake_sel = v; }
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
    if (o == WUBU_VQ_REG_QUEUE_PFN) {
        fake_ring_pa[fake_sel & 1] = (uint64_t)v << 12;
        return;
    }
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
static void arena_free(void *p) { (void)p; }

static void fake_reset(void)
{
    memset(fake_regs, 0, sizeof(fake_regs));
    fake_arena_used = 0;
    wubu_vq_mem.alloc = arena_alloc;
    wubu_vq_mem.free = arena_free;
    fake_ring_pa[0] = fake_ring_pa[1] = 0;
    fake_guest_features = 0xFFFFFFFFu;
    fake_sel = 0;
    fake_isr = 0;
    fake_status_fail = 0;
    fake_tx_notify_count = 0;
    fake_rx_consumed = 0;
    fake_tx_len = 0;
    g_registry_present = 1;
    fake_class = 0x02;
    wubu_vq_io.inb = f_inb; wubu_vq_io.outb = f_outb;
    wubu_vq_io.inw = f_inw; wubu_vq_io.outw = f_outw;
    wubu_vq_io.inl = f_inl; wubu_vq_io.outl = f_outl;
}

int main(void)
{
    printf("test_virtio_net: L5 legacy virtio-net unit gate\n");

    /* 1. probe: negotiation, both rings, MAC, RX posting */
    fake_reset();
    wubu_virtio_net_t *n = wubu_virtio_net_probe();
    CHECK(n != NULL);
    if (!n) { printf("test_virtio_net: probe died, aborting\n"); return 1; }
    uint8_t mac[6];
    CHECK(wubu_virtio_net_get_mac(n, mac) == 0);
    CHECK(memcmp(mac, fake_mac, 6) == 0);
    CHECK(fake_guest_features == (1u << 5));   /* only MAC accepted */
    CHECK(n->hdr_len == 10);                   /* no MRG_RXBUF */
    CHECK(n->rx.qsize == 128 && n->tx.qsize == 128);
    CHECK(fake_ring_pa[0] == (uint64_t)(uintptr_t)n->rx.ring);
    CHECK(fake_ring_pa[1] == (uint64_t)(uintptr_t)n->tx.ring);
    CHECK(((uintptr_t)n->rx.ring & 4095) == 0);
    CHECK(((uintptr_t)n->tx.ring & 4095) == 0);
    CHECK(fake_regs[WUBU_VQ_REG_STATUS] ==
          (WUBU_VQ_S_ACK | WUBU_VQ_S_DRIVER | WUBU_VQ_S_DRIVER_OK));
    CHECK(n->rx.avail->idx == VNET_RX_BUFS);   /* every buffer posted */
    CHECK(n->rx_posted == ((1u << VNET_RX_BUFS) - 1));
    for (uint16_t i = 0; i < VNET_RX_BUFS; i++) {
        CHECK(n->rx.desc[i].flags == WUBU_VQ_DESC_F_WRITE);
        CHECK(n->rx.desc[i].len == VNET_RX_BUF_SZ);
    }

    /* 2. TX: hdr prepend + chain encoding + frame capture */
    uint8_t frame[64];
    for (int i = 0; i < 64; i++) frame[i] = (uint8_t)(i * 3 + 1);
    CHECK(wubu_virtio_net_send(n, frame, 64) == 0);
    CHECK(fake_tx_notify_count == 1);
    CHECK(fake_tx_chain == 2);                 /* hdr -> frame */
    CHECK(fake_tx_hdr_len == 10);
    int hdr_zero = 1;
    for (int i = 0; i < 10; i++) if (fake_tx_hdr[i]) hdr_zero = 0;
    CHECK(hdr_zero);                           /* no offloads claimed */
    CHECK(fake_tx_len == 64);
    CHECK(memcmp(fake_tx_frame, frame, 64) == 0);

    /* 3. TX bounds: refused BEFORE any doorbell */
    int nc = fake_tx_notify_count;
    CHECK(wubu_virtio_net_send(n, frame, 0) == -1);
    CHECK(wubu_virtio_net_send(n, frame, WUBU_VNET_MAX_FRAME + 1) == -1);
    CHECK(wubu_virtio_net_send(n, NULL, 10) == -1);
    CHECK(fake_tx_notify_count == nc);

    /* 4. RX: injected frame -> hdr stripped, payload out, re-post */
    uint8_t rxbuf[2048];
    uint32_t rxlen = 0;
    uint16_t posted_before = n->rx.avail->idx;
    fake_rx_inject(frame, 64);
    CHECK(wubu_virtio_net_recv(n, rxbuf, sizeof(rxbuf), &rxlen) == 0);
    CHECK(rxlen == 64);
    CHECK(memcmp(rxbuf, frame, 64) == 0);
    CHECK(n->rx.avail->idx == (uint16_t)(posted_before + 1)); /* re-posted */
    CHECK(n->rx_posted == ((1u << VNET_RX_BUFS) - 1));

    /* 5. RX empty: bounded probe returns -1, nothing consumed */
    CHECK(wubu_virtio_net_recv(n, rxbuf, sizeof(rxbuf), &rxlen) == -1);

    /* 6. RX ordering: three frames come back in injection order */
    uint8_t f1[10] = {1}, f2[20] = {2}, f3[30] = {3};
    fake_rx_inject(f1, 10); fake_rx_inject(f2, 20); fake_rx_inject(f3, 30);
    CHECK(wubu_virtio_net_recv(n, rxbuf, sizeof(rxbuf), &rxlen) == 0 && rxlen == 10);
    CHECK(wubu_virtio_net_recv(n, rxbuf, sizeof(rxbuf), &rxlen) == 0 && rxlen == 20);
    CHECK(wubu_virtio_net_recv(n, rxbuf, sizeof(rxbuf), &rxlen) == 0 && rxlen == 30);
    CHECK(rxbuf[0] == 3);

    /* 7. small caller cap: payload reported in full, copy truncated */
    fake_rx_inject(frame, 64);
    CHECK(wubu_virtio_net_recv(n, rxbuf, 4, &rxlen) == 0);
    CHECK(rxlen == 64);
    CHECK(memcmp(rxbuf, frame, 4) == 0);

    /* 8. malformed used length (< hdr): dropped, buffer re-posted */
    posted_before = n->rx.avail->idx;
    fake_rx_inject_short();
    CHECK(wubu_virtio_net_recv(n, rxbuf, sizeof(rxbuf), &rxlen) == -1);
    CHECK(n->rx.avail->idx == (uint16_t)(posted_before + 1));

    /* 9. max-size frame round-trip */
    uint8_t *big = malloc(WUBU_VNET_MAX_FRAME);
    CHECK(big != NULL);
    for (uint32_t i = 0; i < WUBU_VNET_MAX_FRAME; i++) big[i] = (uint8_t)(i ^ 0x5A);
    CHECK(wubu_virtio_net_send(n, big, WUBU_VNET_MAX_FRAME) == 0);
    CHECK(fake_tx_len == WUBU_VNET_MAX_FRAME);
    CHECK(memcmp(fake_tx_frame, big, WUBU_VNET_MAX_FRAME) == 0);
    fake_rx_inject(big, WUBU_VNET_MAX_FRAME);
    uint8_t *big2 = malloc(WUBU_VNET_MAX_FRAME);
    CHECK(big2 != NULL);
    CHECK(wubu_virtio_net_recv(n, big2, WUBU_VNET_MAX_FRAME, &rxlen) == 0);
    CHECK(rxlen == WUBU_VNET_MAX_FRAME);
    CHECK(memcmp(big2, big, WUBU_VNET_MAX_FRAME) == 0);
    free(big); free(big2);

    /* 10. absent device / wrong class / refused DRIVER_OK -> NULL */
    vnet_teardown(n);
    g_registry_present = 0;
    CHECK(wubu_virtio_net_probe() == NULL);
    g_registry_present = 1;
    fake_class = 0x01;                         /* a blk function, not net */
    CHECK(wubu_virtio_net_probe() == NULL);
    fake_class = 0x02;
    /* D5b: an MMIO-marked BAR is not a port window; probe must stay absent. */
    g_registry_io_space = 0;
    CHECK(wubu_virtio_net_probe() == NULL);
    g_registry_io_space = 1;
    fake_reset();
    fake_status_fail = 1;
    CHECK(wubu_virtio_net_probe() == NULL);
    fake_status_fail = 0;
    n = wubu_virtio_net_probe();
    CHECK(n != NULL);
    if (n) vnet_teardown(n);

    /* MSI-X left enabled by firmware shifts the config window (0x14->0x18). */
    wubu_drv_dev_t cfgdev;
    memset(&cfgdev, 0, sizeof(cfgdev));
    CHECK(wubu_vq_config_offset(NULL) == 0x14);
    g_msix_state = 0;
    CHECK(wubu_vq_config_offset(&cfgdev) == 0x14);
    g_msix_state = 2;
    CHECK(wubu_vq_config_offset(&cfgdev) == 0x14);
    g_msix_state = 1;
    CHECK(wubu_vq_config_offset(&cfgdev) == 0x18);
    g_msix_state = 0;

    if (failures == 0) printf("test_virtio_net: ALL PASS\n");
    else printf("test_virtio_net: %d FAILURES\n", failures);
    return failures ? 1 : 0;
}