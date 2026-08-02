/*
 * fw_pci.h  --  WuBuFW PCI enumeration interface.
 */

#ifndef WUBUFW_PCI_H
#define WUBUFW_PCI_H

#include <stdint.h>

#define FW_PCI_MAX_DEV 64

/* PCI capability IDs used by the driver layer. */
#define PCI_CAP_MSI    0x05
#define PCI_CAP_PCIE   0x10
#define PCI_CAP_MSIX   0x11

typedef struct {
    uint64_t addr;
    uint64_t size;
    uint8_t  is_io;
    uint8_t  is_64;
} fw_pci_bar;

typedef struct {
    uint8_t  bus, dev, fn;
    uint16_t vendor_id, device_id;
    uint8_t  class_code, subclass, prog_if, revision;
    uint8_t  header_type;
    uint8_t  irq_line;
    fw_pci_bar bar[6];
} fw_pci_dev;

int          fw_pci_init(void);
int          fw_pci_count(void);
fw_pci_dev  *fw_pci_get(int i);
fw_pci_dev  *fw_pci_find_class(uint8_t cls, uint8_t sub, int8_t progif, int nth);
fw_pci_dev  *fw_pci_find_id(uint16_t vid, uint16_t did);
void         fw_pci_enable(fw_pci_dev *d, int bus_master);
uint16_t     fw_pci_find_cap(fw_pci_dev *d, uint8_t cap_id);
void         fw_pci_set_ecam(uint64_t base, uint8_t start_bus, uint8_t end_bus);
const char  *fw_pci_class_name(uint8_t cls, uint8_t sub);
void         fw_pci_dump(void);
void         fw_pci_assign_resources(void);
void         fw_pci_assign_one(fw_pci_dev *d);

uint32_t fw_pci_read32(uint8_t bus, uint8_t dev, uint8_t fn, uint16_t off);
uint16_t fw_pci_read16(uint8_t bus, uint8_t dev, uint8_t fn, uint16_t off);
uint8_t  fw_pci_read8(uint8_t bus, uint8_t dev, uint8_t fn, uint16_t off);
void     fw_pci_write32(uint8_t bus, uint8_t dev, uint8_t fn, uint16_t off, uint32_t v);
void     fw_pci_write16(uint8_t bus, uint8_t dev, uint8_t fn, uint16_t off, uint16_t v);

#endif
