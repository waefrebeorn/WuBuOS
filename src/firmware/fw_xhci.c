/*
 * fw_xhci.c  --  WuBuFW USB 3 (xHCI) host controller driver.
 *
 * Firmware-scope USB: bring the controller out of reset, set up the device
 * context base array and the command ring, and report attached root-hub
 * ports. That is what a boot environment needs (keyboard presence, boot
 * device detection); full device enumeration and HID transfers belong to the
 * OS driver, and pretending otherwise here would be theatre.
 */

#include "fw.h"
#include "fw_pci.h"

#define XHCI_CAPLENGTH  0x00
#define XHCI_HCSPARAMS1 0x04
#define XHCI_HCCPARAMS1 0x10
#define XHCI_DBOFF      0x14
#define XHCI_RTSOFF     0x18

#define XHCI_USBCMD     0x00
#define XHCI_USBSTS     0x04
#define XHCI_PAGESIZE   0x08
#define XHCI_DNCTRL     0x14
#define XHCI_CRCR       0x18
#define XHCI_DCBAAP     0x30
#define XHCI_CONFIG     0x38
#define XHCI_PORTSC(n)  (0x400 + (n) * 0x10)

#define CMD_RUN     0x0001
#define CMD_HCRST   0x0002
#define STS_HCH     0x0001
#define STS_CNR     0x0800

static volatile uint8_t *g_cap;
static volatile uint8_t *g_op;
static uint32_t g_maxports;
static int g_ports_connected;

static uint32_t rd(volatile uint8_t *b, uint32_t o) { return *(volatile uint32_t *)(b + o); }
static void     wr(volatile uint8_t *b, uint32_t o, uint32_t v) { *(volatile uint32_t *)(b + o) = v; }

int fw_xhci_ports_connected(void) { return g_ports_connected; }

/* Release the controller from BIOS/SMM ownership via the legacy support
 * extended capability, or the OS will get spurious SMIs. */
static void take_ownership(void) {
    uint32_t hcc = rd(g_cap, XHCI_HCCPARAMS1);
    uint32_t xecp = (hcc >> 16) & 0xFFFF;
    if (!xecp) return;
    volatile uint8_t *p = g_cap + xecp * 4;
    for (int guard = 0; guard < 64; guard++) {
        uint32_t cap = *(volatile uint32_t *)p;
        uint8_t id = (uint8_t)(cap & 0xFF);
        uint8_t next = (uint8_t)((cap >> 8) & 0xFF);
        if (id == 1) {                              /* USB legacy support */
            *(volatile uint32_t *)p = cap | (1u << 24);   /* OS owned */
            for (int i = 0; i < 100000; i++) {
                if (!(*(volatile uint32_t *)p & (1u << 16))) break;
                fw_stall_us(10);
            }
            /* Disable all SMI sources in USBLEGCTLSTS. */
            *(volatile uint32_t *)(p + 4) = 0xE0000000u;
            return;
        }
        if (!next) return;
        p += next * 4;
    }
}

int fw_xhci_init(fw_pci_dev *d) {
    if (!d || !d->bar[0].addr) return -1;
    g_cap = (volatile uint8_t *)(uintptr_t)d->bar[0].addr;
    uint8_t caplen = *(volatile uint8_t *)(g_cap + XHCI_CAPLENGTH);
    if (!caplen || caplen > 0x80) return -1;
    g_op = g_cap + caplen;

    take_ownership();

    /* Halt then reset. */
    uint32_t cmd = rd(g_op, XHCI_USBCMD);
    wr(g_op, XHCI_USBCMD, cmd & ~CMD_RUN);
    for (int i = 0; i < 200000; i++) {
        if (rd(g_op, XHCI_USBSTS) & STS_HCH) break;
        fw_stall_us(10);
    }
    wr(g_op, XHCI_USBCMD, CMD_HCRST);
    for (int i = 0; i < 500000; i++) {
        if (!(rd(g_op, XHCI_USBCMD) & CMD_HCRST) &&
            !(rd(g_op, XHCI_USBSTS) & STS_CNR)) break;
        fw_stall_us(10);
    }
    if (rd(g_op, XHCI_USBCMD) & CMD_HCRST) return -1;

    uint32_t hcs1 = rd(g_cap, XHCI_HCSPARAMS1);
    g_maxports = (hcs1 >> 24) & 0xFF;
    uint32_t maxslots = hcs1 & 0xFF;

    /* Device context base address array + scratchpad-free minimal config. */
    void *dcbaa = fw_alloc_pages_aligned(1, 4096);
    void *cring = fw_alloc_pages_aligned(1, 4096);
    if (!dcbaa || !cring) return -1;
    fw_memset(dcbaa, 0, 4096);
    fw_memset(cring, 0, 4096);

    wr(g_op, XHCI_CONFIG, maxslots);
    *(volatile uint64_t *)(g_op + XHCI_DCBAAP) = (uint64_t)(uintptr_t)dcbaa;
    *(volatile uint64_t *)(g_op + XHCI_CRCR) = (uint64_t)(uintptr_t)cring | 1;  /* RCS */

    wr(g_op, XHCI_USBCMD, rd(g_op, XHCI_USBCMD) | CMD_RUN);
    for (int i = 0; i < 200000; i++) {
        if (!(rd(g_op, XHCI_USBSTS) & STS_HCH)) break;
        fw_stall_us(10);
    }
    if (rd(g_op, XHCI_USBSTS) & STS_HCH) return -1;

    g_ports_connected = 0;
    for (uint32_t i = 0; i < g_maxports; i++) {
        uint32_t sc = rd(g_op, XHCI_PORTSC(i));
        if (sc & 1) {                                /* CCS: device attached */
            g_ports_connected++;
            fw_printf("[xhci] port %u: device connected (speed %u)\n",
                      i + 1, (sc >> 10) & 0xF);
        }
    }
    fw_printf("[xhci] running: %u ports, %u slots, %d attached\n",
              g_maxports, maxslots, g_ports_connected);
    return 0;
}
