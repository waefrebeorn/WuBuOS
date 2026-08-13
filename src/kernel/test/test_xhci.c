/* test_xhci.c -- host tests for the xHCI controller driver (gap E1).
 * The PCI scan + the BAR0 read are metal-only; the capability parsing
 * and the start sequence are tested against a synthetic MMIO image. */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <sys/mman.h>

#include "wubu_xhci.h"
#include "wubu_xhci.c"

static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { \
    printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); failures++; \
} } while (0)

/* the PCI path is metal-only: stub the real symbols to "no controller"
 * so probe() returns cleanly */
int wubu_pci_scan(wubu_pci_dev_t *d, int n) { (void)d; (void)n; return 0; }
int wubu_pci_find_class(wubu_pci_dev_t *d, int n, uint8_t c, uint8_t s)
{ (void)d; (void)n; (void)c; (void)s; return -1; }
uint32_t wubu_pci_read32(uint8_t b, uint8_t d, uint8_t f, uint8_t o)
{ (void)b; (void)d; (void)f; (void)o; return 0; }

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
    *(uint32_t *)(mm + 0x0C) = 0x00000001;    /* 64-bit addressing */
    *(uint32_t *)(mm + 0x14) = 0x00001000;    /* doorbell at +0x1000 */
    *(uint32_t *)(mm + 0x18) = 0x00000100;    /* runtime at +0x2000 */

    wubu_xhci_t x;
    CHECK(wubu_xhci_probe_regs((uint64_t)(uintptr_t)mm, &x) == 0);
    CHECK(x.present == 1);
    CHECK(x.cap_length == 32);
    CHECK(x.slot_count == 8);
    CHECK(x.port_count == 4);
    CHECK(x.op_base == (uint64_t)(uintptr_t)mm + 32);
    CHECK(x.db_off == 0x1000);
    CHECK(x.rt_off == 0x2000);

    /* the start sequence runs against the synthetic op registers */
    CHECK(wubu_xhci_start(&x) == 0);
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

    /* NULL is rejected */
    CHECK(wubu_xhci_probe_regs(0, NULL) == -1);

    munmap(mm, 0x3000);
    if (failures == 0) printf("test_xhci: ALL PASS\n");
    else printf("test_xhci: %d FAILURES\n", failures);
    return failures ? 1 : 0;
}
