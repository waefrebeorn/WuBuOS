/*
 * wubu_xhci.c  --  the xHCI controller driver (gap E1)
 *
 * xHCI MMIO: the capability registers (CA PLENGTH, HCSPARAMS1,
 * HCCPARAMS1), then the operational registers at cap_base+cap_length
 * (USBSTS, USBCMD, CRCR), the doorbell array at db_off, and the
 * runtime registers at rt_off. The controller is found through PCI
 * class 0x0C0330; its BAR0 is the MMIO base.
 *
 * The start sequence: reset (USBSTS.HCH + USBCMD.RS=0), zero the
 * command ring, point CRCR, set USBCMD.RS=1 (run). A slot is
 * allocated with the Address Device command on the command ring.
 *
 * The ring buffers are static (no heap): one command ring + one event
 * ring, 64 TRBs each.
 */
#include "wubu_xhci.h"
#include "wubu_drv.h"   /* DRV10: the single BAR->VMM mapping owner */

/* xHCI MMIO window. The registry maps the device's BAR0 here (post-vmm-
 * init); the driver dereferences the VA, never the raw physical BAR. */
#define WUBU_XHCI_MMIO_VA 0xffffffff8d000000ull

#define XHCI_TRB_SIZE 16
#define XHCI_RING_TRBS 64

/* command TRB types (xHCI 1.2b 6.4.3.2: TRB Type lives in DW3[15:10]) */
#define TRB_CMD_ADDRESS_DEVICE 11u
#define TRB_CMD_ENABLE_SLOT    9u

/* operational registers (offset from op_base) */
#define OP_USBCMD 0x0u
#define OP_USBSTS 0x4u
#define OP_CRCR   0x18u

/* USBSTS bits */
#define STS_HCH  (1u << 0)
#define STS_CNR  (1u << 11)
/* USBCMD bits */
#define CMD_RUN  (1u << 0)
#define CMD_RESET (1u << 1)

/* xHCI 1.2b 4.2/5.4.1: no Operational or Runtime register may be written
 * while HCRST is 1, and no doorbell/op register other than USBSTS until
 * CNR=0. The wait is behind a hook so a synthetic controller can emulate
 * clearing those bits; the default polls real MMIO with a bounded limit. */
typedef struct {
    int (*reset_wait)(uint64_t op_base);
} wubu_xhci_hw_t;
extern wubu_xhci_hw_t wubu_xhci_hw;

static uint32_t xhci_read32(uint64_t a)
{
    return *(volatile uint32_t *)(uintptr_t)a;
}

static void xhci_write32(uint64_t a, uint32_t v)
{
    *(volatile uint32_t *)(uintptr_t)a = v;
}

static void xhci_write64(uint64_t a, uint64_t v)
{
    *(volatile uint64_t *)(uintptr_t)a = v;
}

static uint64_t xhci_read64(uint64_t a)
{
    return *(volatile uint64_t *)(uintptr_t)a;
}

static int xhci_default_reset_wait(uint64_t op)
{
    for (uint32_t t = 0; t < 1000000u; t++) {
        uint32_t cmd = xhci_read32(op + OP_USBCMD);
        uint32_t sts = xhci_read32(op + OP_USBSTS);
        if (!(cmd & CMD_RESET) && !(sts & STS_CNR)) return 0;
    }
    return -1;                       /* controller never became ready */
}

wubu_xhci_hw_t wubu_xhci_hw = { xhci_default_reset_wait };

/* the static command ring (in low memory so the controller can DMA) */
static uint8_t g_cmd_ring[XHCI_RING_TRBS * XHCI_TRB_SIZE] __attribute__((aligned(64)));
static uint32_t g_cmd_dequeue;   /* TRB index */

/* the static event ring (16 TRBs) + its ERST segment -- without these
 * the controller DMAs completion events to garbage once a command
 * completes (a real crash on hardware). */
#define XHCI_EV_TRBS 16
static uint8_t g_ev_ring[XHCI_EV_TRBS * XHCI_TRB_SIZE] __attribute__((aligned(64)));
static uint8_t g_erst[16] __attribute__((aligned(64)));

/* D2: find the xHCI device in the registry and take its MAPPED MMIO VA.
 * The old path read BAR0 from config space and dereferenced the raw
 * physical address as a VA (broken once paging is on) -- and its
 * `bar0 & 1` test was inverted: bit 0 SET means I/O space, not MMIO.
 * No second PCI scan here; the registry owns discovery and mapping. */
/* xHCI BAR0 is typically 16 KB: capability/operational at +0x0000,
 * runtime at +0x1000, doorbells at +0x2000 and MSI-X at +0x3000/+0x3800
 * (Intel xHCI spec 1.2b, sections 5.3-5.6). Request the full four-page
 * aperture through the range owner so none of those windows can cross a
 * page boundary. The driver is polled and never enables MSI-X. Four pages
 * is the FALLBACK when the PCI scan could not size BAR0; otherwise the
 * registry's measured size is used. */
#define WUBU_XHCI_MMIO_PAGES 4u

static int xhci_registry_mmio(uint64_t *va)
{
    const wubu_drv_dev_t *dev = wubu_drv_find_class(0x0C, 0x03);
    if (!dev) return -1;
    /* Use the PCI-sized BAR when the scan could determine it; four pages
     * is only the conservative fallback for an unknown size. Clamp to a
     * sane range so a corrupt configuration cannot map the entire VA space. */
    uint32_t pages = WUBU_XHCI_MMIO_PAGES;
    if (dev->bar0_size) {
        uint64_t need = (dev->bar0_size + 4095u) / 4096u;
        if (need >= 1u && need <= 64u) pages = (uint32_t)need;
    }
    return wubu_drv_map_mmio_range(dev, WUBU_XHCI_MMIO_VA, pages, va);
}

int wubu_xhci_probe_regs(uint64_t mmio, wubu_xhci_t *out)
{
    if (!out) return -1;
    out->present = 0;
    if (!mmio) return 0;
    uint32_t caplen = xhci_read32(mmio + 0x00) & 0xFF;
    uint32_t hcs1 = xhci_read32(mmio + 0x04);
    uint32_t hcc1 = xhci_read32(mmio + 0x10);  /* HCCPARAMS1 (0x0C is HCSPARAMS3) */
    uint32_t db_off = xhci_read32(mmio + 0x14);
    uint32_t rt_off = xhci_read32(mmio + 0x18);
    out->cap_length = caplen;
    out->hcs_params = hcs1;
    out->hcc_params = hcc1;
    /* xHCI 1.2b 5.3.7/5.3.8: DBOFF bits 31:2 and RTSOFF bits 31:5 are
     * byte offsets already. Mask reserved low bits; never scale them. */
    out->db_off = db_off & ~3u;
    out->rt_off = rt_off & ~0x1Fu;
    out->mmio_base = mmio;
    out->op_base = mmio + caplen;
    out->port_count = (hcs1 >> 24) & 0xFF;
    out->slot_count = hcs1 & 0xFF;
    out->present = 1;
    return 0;
}

int wubu_xhci_probe(wubu_xhci_t *out)
{
    uint64_t va = 0;
    if (out) out->present = 0;
    if (xhci_registry_mmio(&va) != 0) return 0;
    return wubu_xhci_probe_regs(va, out);
}

int wubu_xhci_start(wubu_xhci_t *xh)
{
    if (!xh || !xh->present) return -1;
    uint64_t op = xh->op_base;

    /* HCRST must be written only while halted, and nothing else may be
     * programmed until the controller has cleared HCRST and CNR. */
    xhci_write32(op + OP_USBCMD, xhci_read32(op + OP_USBCMD) & ~CMD_RUN);
    uint32_t t = 0;
    for (; t < 1000000u; t++)
        if (xhci_read32(op + OP_USBSTS) & STS_HCH) break;
    if (t == 1000000u) return -1;
    xhci_write32(op + OP_USBCMD, CMD_RESET);
    if (!wubu_xhci_hw.reset_wait ||
        wubu_xhci_hw.reset_wait(op) != 0) return -1;

    /* the command ring: zero it, point CRCR (RCS=1) */
    for (uint32_t i = 0; i < sizeof(g_cmd_ring); i++) g_cmd_ring[i] = 0;
    g_cmd_dequeue = 0;
    uint64_t crcr = (uint64_t)(uintptr_t)g_cmd_ring | 1u;  /* RCS */
    xhci_write64(op + OP_CRCR, crcr);

    /* the event ring: zero the ring + the ERST segment, program the
     * first interrupter's ERSTSZ/ERSTBA/ERDP so completions land in a
     * real buffer (the runtime registers at rt_base; interrupter 0 at
     * rt_base + 0x20). */
    uint64_t rt = xh->mmio_base + xh->rt_off;
    for (uint32_t i = 0; i < sizeof(g_ev_ring); i++) g_ev_ring[i] = 0;
    for (uint32_t i = 0; i < sizeof(g_erst); i++) g_erst[i] = 0;
    *(uint64_t *)&g_erst[0] = (uint64_t)(uintptr_t)g_ev_ring;
    *(uint32_t *)&g_erst[8] = XHCI_EV_TRBS;      /* the segment size */
    xhci_write32(rt + 0x28, 1);                  /* ERSTSZ = 1 segment */
    xhci_write64(rt + 0x30, (uint64_t)(uintptr_t)g_erst);  /* ERSTBA */
    xhci_write64(rt + 0x38, (uint64_t)(uintptr_t)g_ev_ring); /* ERDP */
    xhci_write32(rt + 0x20, 0);                  /* IMAN: clear pending */

    xhci_write32(op + OP_USBCMD, CMD_RUN);
    return 0;
}

int wubu_xhci_slot_alloc(wubu_xhci_t *xh)
{
    if (!xh || !xh->present) return -1;
    if (g_cmd_dequeue + 1 >= XHCI_RING_TRBS) return -1;
    /* an Enable Slot command TRB */
    uint8_t *trb = g_cmd_ring + (uint64_t)g_cmd_dequeue * XHCI_TRB_SIZE;
    uint32_t *w = (uint32_t *)trb;
    w[0] = 0;                       /* slot type = 0 (default) */
    w[1] = 0;
    w[2] = 0;
    w[3] = (TRB_CMD_ENABLE_SLOT << 10) | 1u;   /* type in DW3[15:10], C=1 */
    g_cmd_dequeue++;
    /* ring the doorbell 0 (the command doorbell) */
    uint64_t db = xh->mmio_base + xh->db_off;
    xhci_write32(db, 0);
    return (int)(g_cmd_dequeue);    /* the slot id is the completion's; the
                                       ring position stands in for it here */
}
