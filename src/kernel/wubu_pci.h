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
    uint64_t bar0;          /* BAR0 (MMIO or IO) */
    uint64_t bar1;
} wubu_pci_dev_t;

#define WUBU_PCI_MAX_DEVS 32

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
