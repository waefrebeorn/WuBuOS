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
#include "wubu_pci.h"   /* wubu_pci_dev_t + the config-space accessors */

#define XHCI_TRB_SIZE 16
#define XHCI_RING_TRBS 64

/* command TRB types */
#define TRB_CMD_ADDRESS_DEVICE 0x08u
#define TRB_CMD_ENABLE_SLOT    0x09u

/* operational registers (offset from op_base) */
#define OP_USBCMD 0x0u
#define OP_USBSTS 0x4u
#define OP_CRCR   0x18u

/* USBSTS bits */
#define STS_HCH  (1u << 0)
/* USBCMD bits */
#define CMD_RUN  (1u << 0)
#define CMD_RESET (1u << 1)

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

/* the static command ring (in low memory so the controller can DMA) */
static uint8_t g_cmd_ring[XHCI_RING_TRBS * XHCI_TRB_SIZE] __attribute__((aligned(64)));
static uint32_t g_cmd_dequeue;   /* TRB index */

static int xhci_pci_bar0(uint64_t *bar)
{
    /* scan the bus for the USB controller (class 0x0C/0x03), read BAR0 */
    wubu_pci_dev_t devs[WUBU_PCI_MAX_DEVS];
    int n = wubu_pci_scan(devs, WUBU_PCI_MAX_DEVS);
    int i = wubu_pci_find_class(devs, n, 0x0C, 0x03);
    if (i < 0) return -1;
    uint32_t bar0 = wubu_pci_read32(devs[i].bus, devs[i].dev, devs[i].fn, 0x10);
    if (!(bar0 & 1)) return -1;                 /* must be MMIO */
    *bar = bar0 & 0xFFFFFFF0ull;
    return 0;
}

int wubu_xhci_probe_regs(uint64_t mmio, wubu_xhci_t *out)
{
    if (!out) return -1;
    out->present = 0;
    if (!mmio) return 0;
    uint32_t caplen = xhci_read32(mmio + 0x00) & 0xFF;
    uint32_t hcs1 = xhci_read32(mmio + 0x04);
    uint32_t hcc1 = xhci_read32(mmio + 0x0C);
    uint32_t db_off = xhci_read32(mmio + 0x14);
    uint32_t rt_off = xhci_read32(mmio + 0x18);
    out->cap_length = caplen;
    out->hcs_params = hcs1;
    out->hcc_params = hcc1;
    out->db_off = db_off & 0xFFFF;
    out->rt_off = (rt_off & 0xFFFF) << 5;
    out->mmio_base = mmio;
    out->op_base = mmio + caplen;
    out->port_count = (hcs1 >> 24) & 0xFF;
    out->slot_count = hcs1 & 0xFF;
    out->present = 1;
    return 0;
}

int wubu_xhci_probe(wubu_xhci_t *out)
{
    uint64_t bar = 0;
    if (xhci_pci_bar0(&bar) != 0) {
        if (out) out->present = 0;
        return 0;
    }
    return wubu_xhci_probe_regs(bar, out);
}

int wubu_xhci_start(wubu_xhci_t *xh)
{
    if (!xh || !xh->present) return -1;
    uint64_t op = xh->op_base;

    /* reset the controller: HCH must be set, then CMD_RESET */
    uint32_t sts = xhci_read32(op + OP_USBSTS);
    if (sts & STS_HCH) {
        xhci_write32(op + OP_USBCMD, CMD_RESET);
        for (uint32_t t = 0; t < 1000000; t++) {
            if (xhci_read32(op + OP_USBSTS) & STS_HCH) break;
        }
    }

    /* the command ring: zero it, point CRCR (RCS=1), then run */
    for (uint32_t i = 0; i < sizeof(g_cmd_ring); i++) g_cmd_ring[i] = 0;
    g_cmd_dequeue = 0;
    uint64_t crcr = (uint64_t)(uintptr_t)g_cmd_ring | 1u;  /* RCS */
    xhci_write64(op + OP_CRCR, crcr);

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
    w[3] = TRB_CMD_ENABLE_SLOT;     /* the type field */
    g_cmd_dequeue++;
    /* ring the doorbell 0 (the command doorbell) */
    uint64_t db = xh->mmio_base + xh->db_off;
    xhci_write32(db, 0);
    return (int)(g_cmd_dequeue);    /* the slot id is the completion's; the
                                       ring position stands in for it here */
}
