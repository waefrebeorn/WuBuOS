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

#define FAKE_NDEV 4

static struct {
    uint8_t  dev, fn;
    uint32_t ids;    /* offset 0x00: vendor|device<<16; 0xFFFFFFFF = empty */
    uint32_t cls;    /* offset 0x08: class/sub/progif */
    uint32_t cmd;    /* offset 0x0C: bit7 (0x80) = multifunction */
    uint32_t bar0;   /* offset 0x10 */
} g_fake[FAKE_NDEV];
static int g_fake_n = 0;

/* seed a fake device into the table */
static void pci_test_seed(uint8_t dev, uint8_t fn, uint32_t ids,
                          uint32_t cls, uint32_t cmd, uint32_t bar0) {
    if (g_fake_n >= FAKE_NDEV) return;
    g_fake[g_fake_n].dev = dev;
    g_fake[g_fake_n].fn = fn;
    g_fake[g_fake_n].ids = ids;
    g_fake[g_fake_n].cls = cls;
    g_fake[g_fake_n].cmd = cmd;
    g_fake[g_fake_n].bar0 = bar0;
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
            case 0x10: return g_fake[i].bar0;
            default:   return 0;
            }
        }
    }
    return 0xFFFFFFFFu;
}

/* write_config: no-op in the fake config space (read-only test bus). */
void wubu_pci_write_config(uint8_t bus, uint8_t dev, uint8_t fn, uint8_t off, uint32_t val) {
    (void)bus; (void)dev; (void)fn; (void)off; (void)val;
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
    pci_test_seed(0x00, 0, 0x16361002u, 0x03000000u, 0x00000000u, 0xF0000000u);
    pci_test_seed(0x01, 0, 0x56781002u, 0x04030000u, 0x00800000u, 0x00000000u);
    pci_test_seed(0x01, 1, 0x9ABC1002u, 0x04030000u, 0x00000000u, 0x0000D000u);
    pci_test_seed(0x02, 0, 0xFFFFFFFFu, 0, 0, 0);  /* empty slot */

    wubu_pci_dev_t devs[WUBU_PCI_MAX_DEVS];
    int n = wubu_pci_scan(devs, WUBU_PCI_MAX_DEVS);

    printf("  scanned: %d devices (expect 3: single + multifn x2)\n", n);
    OK(n == 3);

    /* dev0: AMD GPU, class 0x03 (display), BAR0 = 0xF0000000 */
    OK(devs[0].vendor == 0x1002);
    OK(devs[0].device == 0x1636);
    OK(devs[0].class_code == 0x03);
    OK(devs[0].bar0 == 0xF0000000u);
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

    /* find_class: locate display controller (class 0x03 subclass 0x00) */
    int idx = wubu_pci_find_class(devs, n, 0x03, 0x00);
    OK(idx == 0);

    /* find_class: missing class returns -1 */
    OK(wubu_pci_find_class(devs, n, 0x12, 0x34) == -1);

    printf("[wubu_pci_selftest] %d passed, %d failed\n", s_pass, s_fail);
    return s_fail == 0 ? 0 : 1;
}
