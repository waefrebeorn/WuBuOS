/*
 * wubu_pci_selftest.c -- SELFTEST for the PCI bus enumerator.
 *
 * PROVEN: wubu_pci_scan() filters empty (0xFFFFFFFF) config reads,
 * honours the PCIe multifunction bit (bit 7 of header byte @0x0C),
 * and extracts vendor/device/class/BAR0 from the raw 32-bit config
 * words.  The scan logic is exercised against a seeded in-memory
 * config-space table (no real port I/O) so it runs anywhere.
 *
 * Closed the gap in driver_self_install.md §2: the enumerator had no
 * self-contained verification without real hardware.
 */
#include "wubu_pci.h"
#include "libc.h"      /* memset + kernel printf (no host <stdio.h>) */
#include <stdint.h>

/* ------------------------------------------------------------------ */
/* Fake PCI config-space.  When WUBU_PCI_TEST_STUBS is defined,
 * wubu_pci.c compiles out the real port-I/O outl_/inl_ and instead
 * calls this function, which we define here as a memory-backed table.
 * A real device returns its 32-bit word; an empty slot returns
 * 0xFFFFFFFF.                                                  */
/* ------------------------------------------------------------------ */

#define FAKE_NDEV 6

static struct {
    uint8_t  dev, fn;
    uint32_t ids;    /* offset 0x00: vendor|device<<16; 0xFFFFFFFF = empty */
    uint32_t cls;    /* offset 0x08: class/sub/progif */
    uint32_t cmd;    /* offset 0x0C: bit7 (0x80) = multifunction */
    uint32_t bar0;   /* offset 0x10 */
    uint32_t bar1;   /* offset 0x14: separate BAR unless BAR0 is 64-bit */
    uint32_t mask0, mask1;       /* implemented address bits (write-ones readback) */
    uint32_t probe0, probe1;     /* last value written by the sizing probe */
} g_fake[FAKE_NDEV];
static int g_fake_n = 0;

/* seed a fake device into the table */
static void pci_test_seed(uint8_t dev, uint8_t fn, uint32_t ids,
                          uint32_t cls, uint32_t cmd, uint32_t bar0,
                          uint32_t bar1) {
    if (g_fake_n >= FAKE_NDEV) return;
    g_fake[g_fake_n].dev = dev;
    g_fake[g_fake_n].fn = fn;
    g_fake[g_fake_n].ids = ids;
    g_fake[g_fake_n].cls = cls;
    g_fake[g_fake_n].cmd = cmd;
    g_fake[g_fake_n].bar0 = bar0;
    g_fake[g_fake_n].bar1 = bar1;
    if (bar0 & 1u) {
        g_fake[g_fake_n].mask0 = 0xFFFFFFE0u;   /* 32 I/O ports */
        g_fake[g_fake_n].mask1 = 0;
    } else if ((bar0 & 0x6u) == 0x4u) {
        g_fake[g_fake_n].mask0 = 0xFFFFC000u;   /* 64-bit, 16 KB */
        g_fake[g_fake_n].mask1 = 0xFFFFFFFFu;
    } else {
        g_fake[g_fake_n].mask0 = 0xFFFFF000u;   /* 32-bit, 4 KB */
        g_fake[g_fake_n].mask1 = 0;
    }
    g_fake_n++;
}

/* the hook wubu_pci.c calls when WUBU_PCI_TEST_STUBS is set */
uint32_t wubu_pci_read_config(uint8_t bus, uint8_t dev, uint8_t fn, uint8_t off) {
    (void)bus;
    for (int i = 0; i < g_fake_n; i++) {
        if (g_fake[i].dev == dev && g_fake[i].fn == fn) {
            switch (off) {
            case 0x00: return g_fake[i].ids;
            case 0x08: return g_fake[i].cls;
            case 0x0C: return g_fake[i].cmd;
            case 0x10: return g_fake[i].probe0 == 0xFFFFFFFFu
                                 ? g_fake[i].mask0 : g_fake[i].bar0;
            case 0x14: return g_fake[i].probe1 == 0xFFFFFFFFu
                                 ? g_fake[i].mask1 : g_fake[i].bar1;
            default:   return 0;
            }
        }
    }
    return 0xFFFFFFFFu;
}

/* write_config models the write-all-ones sizing probe and restores BAR0. */
void wubu_pci_write_config(uint8_t bus, uint8_t dev, uint8_t fn, uint8_t off, uint32_t val) {
    (void)bus;
    for (int i = 0; i < g_fake_n; i++) {
        if (g_fake[i].dev != dev || g_fake[i].fn != fn) continue;
        if (off == 0x10) g_fake[i].probe0 = val;
        if (off == 0x14) g_fake[i].probe1 = val;
    }
}

/* ------------------------------------------------------------------ */
/* Self-test driver.
 * ------------------------------------------------------------------ */

static int s_pass = 0, s_fail = 0;
#define OK(c) do { if (c) { s_pass++; } else { s_fail++; printf("  FAIL: %s (line %d)\n", #c, __LINE__); } } while (0)

int main(void) {
    printf("[wubu_pci_selftest] PCI bus-0 scan logic\n");

    /* seed: one single-function AMD GPU (0x1002:0x1636, class 03/00/00,
     * BAR0=0xF0000000), one multifunction pair (fn0 cmd dword has bit 23
     * set = multifunction flag per PCI §2, fn1 audio), one empty slot. */
    /* BAR0 32-bit memory + a real BAR1: the old bar_of consumed BAR1 as
     * BAR0's 64-bit high half and corrupted both. */
    pci_test_seed(0x00, 0, 0x16361002u, 0x03000000u, 0x00000000u,
                  0xF0000000u, 0x0000D000u);
    pci_test_seed(0x01, 0, 0x56781002u, 0x04030000u, 0x00800000u,
                  0x00000000u, 0);
    pci_test_seed(0x01, 1, 0x9ABC1002u, 0x04030000u, 0x00000000u,
                  0x0000D000u, 0);
    pci_test_seed(0x02, 0, 0xFFFFFFFFu, 0, 0, 0, 0);
    /* type=10 memory BAR: bits 32:63 come from the next DWORD. */
    pci_test_seed(0x03, 0, 0x12345678u, 0x02000000u, 0,
                  0x0000000Cu, 0x00000001u);
    /* I/O BAR with bits above 15 set: full 32-bit decode must survive. */
    pci_test_seed(0x04, 0, 0x10011AF4u, 0x02000000u, 0,
                  0x00010101u, 0);

    wubu_pci_dev_t devs[WUBU_PCI_MAX_DEVS];
    int n = wubu_pci_scan(devs, WUBU_PCI_MAX_DEVS);

    printf("  scanned: %d devices (expect 5: single + multifn x2 + 64bit + IO)\n", n);
    OK(n == 5);

    /* dev0: AMD GPU, class 0x03 (display), BAR0 = 0xF0000000 */
    OK(devs[0].vendor == 0x1002);
    OK(devs[0].device == 0x1636);
    OK(devs[0].class_code == 0x03);
    OK(devs[0].bar0 == 0xF0000000u);
    OK(devs[0].bar1 == 0x0000D000u);   /* BAR1 is separate, not BAR0's high half */
    OK(devs[0].bar0_is_io == 0);
    OK(devs[0].bar0_size == 0x1000u);   /* 4 KB via write-all-ones */
    OK(devs[3].bar0_size == 0x4000u);   /* 64-bit, 16 KB */
    OK(devs[4].bar0_size == 0x20u);     /* 32 I/O ports */
    OK(devs[0].bus == 0 && devs[0].dev == 0x00 && devs[0].fn == 0);

    /* multifn: dev1 fn0 + fn1 bit 7 set on fn0 cmd byte */
    OK(devs[1].bus == 0 && devs[1].dev == 0x01 && devs[1].fn == 0);
    OK(devs[1].vendor == 0x1002 && devs[1].device == 0x5678);
    OK(devs[2].bus == 0 && devs[2].dev == 0x01 && devs[2].fn == 1);
    OK(devs[2].vendor == 0x1002 && devs[2].device == 0x9ABC);
    OK(devs[2].bar0 == 0x0000D000u);

    /* the 0xFFFFFFFF empty slot must be skipped */
    { int found_empty = 0;
      for (int i = 0; i < n; i++)
          if (devs[i].vendor == 0xFFFF) found_empty = 1;
      OK(!found_empty);
    }

    /* 64-bit and 32-bit I/O BAR decode per PCI 3.0 6.2.5.1. */
    OK(devs[3].bar0 == 0x0000000100000000ull);
    OK(devs[3].bar0_is_io == 0);
    OK(devs[4].bar0 == 0x00010100u);
    OK(devs[4].bar0_is_io == 1);

    /* find_class: locate display controller (class 0x03 subclass 0x00) */
    int idx = wubu_pci_find_class(devs, n, 0x03, 0x00);
    OK(idx == 0);

    /* find_class: missing class returns -1 */
    OK(wubu_pci_find_class(devs, n, 0x12, 0x34) == -1);

    /* hosted sysfs extent parsing: the same helper the scan uses */
    uint64_t base = 0, extent = 0;
    OK(wubu_pci_resource_extent(
        "00000000f0000000 00000000f0003fff 02040000 0 0 0 0",
        &base, &extent) == 0xF0000000ull);
    OK(base == 0xF0000000ull && extent == 0x4000u);
    OK(wubu_pci_resource_extent(
        "0000000000001000 000000000000101f 01010000 0 0 0 0",
        &base, &extent) == 0x1000u);
    OK(extent == 0x20u);
    OK(wubu_pci_resource_extent("garbage", &base, &extent) == 0);
    OK(extent == 1u);

    printf("[wubu_pci_selftest] %d passed, %d failed\n", s_pass, s_fail);
    return s_fail == 0 ? 0 : 1;
}
