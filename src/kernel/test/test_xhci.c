/* test_xhci.c -- host tests for the xHCI controller driver (gap E1).
 * The PCI scan + the BAR0 read are metal-only; the capability parsing
 * and the start sequence are tested against a synthetic MMIO image. */
#include "../../egojar.h"
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "wubu_xhci.h"
#include "wubu_drv.h"
#include "wubu_xhci.c"

static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { \
    printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); failures++; \
} } while (0)

/* D2: stub the registry owner. probe() must go through these, never
 * dereference a raw physical BAR itself. */
static wubu_drv_dev_t g_fake_xhci_dev;
static int g_fake_xhci_present;
static uint64_t g_fake_map_va;
static int g_fake_map_calls;

const wubu_drv_dev_t *wubu_drv_find_class(uint8_t c, uint8_t s)
{
    (void)c; (void)s;
    return g_fake_xhci_present ? &g_fake_xhci_dev : NULL;
}

static uint32_t g_fake_map_pages;
int wubu_drv_map_mmio_range(const wubu_drv_dev_t *d, uint64_t va,
                            uint32_t pages, uint64_t *out)
{
    (void)d; (void)va;
    g_fake_map_calls++;
    g_fake_map_pages = pages;
    if (!out) return -1;
    *out = g_fake_map_va;
    return 0;
}

/* Synthetic xHCI controller: the hardware clears HCRST and CNR when the
 * reset completes. The hook also checks that HCRST was actually written. */
static int g_reset_wait_calls, g_reset_wait_fail;
static int fake_reset_wait(uint64_t op)
{
    g_reset_wait_calls++;
    if (g_reset_wait_fail) return -1;
    volatile uint32_t *cmd = (uint32_t *)(uintptr_t)(op + OP_USBCMD);
    if (!(*cmd & CMD_RESET)) return -1;
    *cmd = 0;
    volatile uint32_t *sts = (uint32_t *)(uintptr_t)(op + OP_USBSTS);
    *sts &= ~STS_CNR;
    return 0;
}

int main(void)
{
    printf("wubu_xhci tests (gap E1)\n");

    /* a synthetic MMIO image: caps + op regs + doorbells (+0x1000) +
     * runtime (+0x2000). Map 0x3000 so every write stays inside. */
    uint8_t *mm = mmap(NULL, 0x3000, PROT_READ | PROT_WRITE,
                       MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    CHECK(mm != MAP_FAILED);
    memset(mm, 0, 0x3000);
    *(uint32_t *)(mm + 0x00) = 0x20;          /* cap length 32 */
    *(uint32_t *)(mm + 0x04) = 0x04080008;    /* 8 slots, 8 intrs, 4 ports */
    *(uint32_t *)(mm + 0x0C) = 0x00000001;    /* HCSPARAMS3 (not HCCPARAMS1) */
    *(uint32_t *)(mm + 0x10) = 0xDEADBEEFu;   /* HCCPARAMS1 lives at 0x10 */
    *(uint32_t *)(mm + 0x14) = 0x00001002u;   /* DBOFF +0x1000, reserved low bits */
    *(uint32_t *)(mm + 0x18) = 0x00002000u;   /* RTSOFF is already a byte offset */

    wubu_xhci_t x;
    CHECK(wubu_xhci_probe_regs((uint64_t)(uintptr_t)mm, &x) == 0);
    CHECK(x.present == 1);
    CHECK(x.cap_length == 32);
    CHECK(x.slot_count == 8);
    CHECK(x.port_count == 4);
    CHECK(x.op_base == (uint64_t)(uintptr_t)mm + 32);
    CHECK(x.hcc_params == 0xDEADBEEFu);        /* 0x10, not HCSPARAMS3 */
    CHECK(x.db_off == 0x1000);
    CHECK(x.rt_off == 0x2000);

    /* xHCI 1.2b: DBOFF/RTSOFF are 32-bit byte offsets. A high-half value
     * must survive; the old 0xFFFF mask + <<5 scaling lost both. */
    *(uint32_t *)(mm + 0x14) = 0x00011002u;
    *(uint32_t *)(mm + 0x18) = 0x00012000u;
    wubu_xhci_t hi;
    CHECK(wubu_xhci_probe_regs((uint64_t)(uintptr_t)mm, &hi) == 0);
    CHECK(hi.db_off == 0x11000);
    CHECK(hi.rt_off == 0x12000);
    *(uint32_t *)(mm + 0x14) = 0x00001002u;
    *(uint32_t *)(mm + 0x18) = 0x00002000u;

    /* D2: probe() must obtain its MMIO VA from the registry owner. */
    memset(&g_fake_xhci_dev, 0, sizeof(g_fake_xhci_dev));
    g_fake_xhci_dev.class_code = 0x0C;
    g_fake_xhci_dev.subclass = 0x03;
    g_fake_xhci_dev.bar0 = (uint64_t)(uintptr_t)mm;  /* raw phys fixture */
    g_fake_map_va = (uint64_t)(uintptr_t)mm;         /* mapped VA fixture */
    g_fake_xhci_present = 1;
    g_fake_map_calls = 0;
    wubu_xhci_t px;
    CHECK(wubu_xhci_probe(&px) == 0);
    CHECK(px.present == 1);
    CHECK(g_fake_map_calls == 1);
    CHECK(g_fake_map_pages == 4);   /* full 16KB xHCI BAR aperture */
    CHECK(px.mmio_base == (uint64_t)(uintptr_t)mm);
    CHECK(px.op_base == (uint64_t)(uintptr_t)mm + 32);
    /* negative: no registry device -> clean absent, mapper never called */
    g_fake_xhci_present = 0;
    g_fake_map_calls = 0;
    memset(&px, 0, sizeof(px));
    CHECK(wubu_xhci_probe(&px) == 0);
    CHECK(px.present == 0);
    CHECK(g_fake_map_calls == 0);
    printf("test_xhci: D2 registry->mapped-VA probe PASS\n");

    /* the start sequence runs against the synthetic op registers */
    wubu_xhci_hw.reset_wait = fake_reset_wait;
    g_reset_wait_calls = 0;
    g_reset_wait_fail = 0;
    volatile uint32_t *opsts = (uint32_t *)(uintptr_t)(x.op_base + OP_USBSTS);
    *opsts = STS_HCH | STS_CNR;       /* halted, controller not ready yet */
    CHECK(wubu_xhci_start(&x) == 0);
    CHECK(g_reset_wait_calls == 1);
    CHECK((*opsts & STS_CNR) == 0);   /* waited for CNR before programming */
    CHECK(xhci_read32(x.op_base + OP_USBCMD) & CMD_RUN);
    /* the event ring is programmed: ERSTBA/ERDP point at the static
     * buffers, ERSTSZ = 1 segment */
    uint64_t rt = (uint64_t)(uintptr_t)mm + x.rt_off;
    CHECK(xhci_read32(rt + 0x28) == 1);
    CHECK(xhci_read64(rt + 0x30) == (uint64_t)(uintptr_t)g_erst);
    CHECK(xhci_read64(rt + 0x38) == (uint64_t)(uintptr_t)g_ev_ring);

    /* slot allocation walks the command ring */
    int slot = wubu_xhci_slot_alloc(&x);
    CHECK(slot >= 0);
    /* xHCI 1.2b 6.4.3.2: TRB Type is DW3[15:10], C is DW3 bit 0. */
    CHECK(((uint32_t *)g_cmd_ring)[3] == ((9u << 10) | 1u));

    /* xHCI 1.2b 5.4.1: no HCRST while running, no op/runtime writes until
     * HCRST and CNR are clear. Both failures must abort before CRCR. */
    volatile uint32_t *opcmd = (uint32_t *)(uintptr_t)(x.op_base + OP_USBCMD);
    *opcmd = CMD_RUN;
    *opsts = 0;                       /* not halted */
    g_reset_wait_calls = 0;
    xhci_write64(x.op_base + OP_CRCR, 0);
    CHECK(wubu_xhci_start(&x) == -1);
    CHECK(g_reset_wait_calls == 0);
    CHECK(xhci_read64(x.op_base + OP_CRCR) == 0);
    *opsts = STS_HCH | STS_CNR;
    g_reset_wait_fail = 1;
    CHECK(wubu_xhci_start(&x) == -1);
    CHECK(xhci_read64(x.op_base + OP_CRCR) == 0);
    g_reset_wait_fail = 0;

    /* NULL is rejected */
    CHECK(wubu_xhci_probe_regs(0, NULL) == -1);

    munmap(mm, 0x3000);
    if (failures == 0) printf("test_xhci: ALL PASS\n");
    else printf("test_xhci: %d FAILURES\n", failures);
    return failures ? 1 : 0;
}
