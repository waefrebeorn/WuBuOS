/*
 * wubu_pci.h -- minimal PCI config-space access for the metal kernel.
 * Config mechanism: 0xCF8/0xCFC (x86 legacy).  Used by the USB stack
 * (xHCI discovery) and the live console's `pci` command.
 */
#ifndef WUBU_PCI_H
#define WUBU_PCI_H

#include <stdint.h>

typedef struct {
    uint8_t  bus, dev, fn;
    uint16_t vendor, device;
    uint8_t  class_code, subclass, prog_if;
    uint64_t bar0;          /* BAR0 base (type bits masked off) */
    uint64_t bar1;
    /* PCI 3.0 6.2.5.1 write-all-ones size of BAR0, or 0 if unknown.
     * Hosted sysfs derives it from the resource extent; metal probes it. */
    uint64_t bar0_size;
    /* Modern virtio PCI puts its capability BARs in BAR4/BAR5 (a 64-bit
     * pair). Captured but not yet consumed: the driver stays legacy. */
    uint64_t bar4;
    uint64_t bar4_size;
    uint8_t  bar4_is_io;
    /* BAR0 address space: 1 = I/O ports, 0 = MMIO. The base in bar0 has
     * its type bits masked, so without this flag an I/O BAR is
     * indistinguishable from an MMIO BAR. The registry carries it through
     * so the one owner can reject I/O BARs from the MMIO mapper. */
    uint8_t  bar0_is_io;
} wubu_pci_dev_t;

#define WUBU_PCI_MAX_DEVS 32

/* Parse "start end flags" from a sysfs resource line. Exported so the
 * hosted path can be gated with synthetic lines without real hardware. */
uint64_t wubu_pci_resource_extent(const char *line, uint64_t *base,
                                  uint64_t *size);

/* Full bus-0 scan; returns the number of devices found. */
int wubu_pci_scan(wubu_pci_dev_t *out, int max);

/* Find the first device matching a class/subclass (e.g. 0x0C/0x03 = USB).
 * Returns the index into the scan result, or -1. */
int wubu_pci_find_class(wubu_pci_dev_t *devs, int n,
                        uint8_t class_code, uint8_t subclass);

uint32_t wubu_pci_read32(uint8_t bus, uint8_t dev, uint8_t fn, uint8_t off);
void     wubu_pci_write32(uint8_t bus, uint8_t dev, uint8_t fn, uint8_t off,
                          uint32_t val);

#endif /* WUBU_PCI_H */
