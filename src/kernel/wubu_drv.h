/*
 * wubu_drv.h -- the DRIVER REGISTRY (the device model).
 */
#ifndef WUBU_DRV_H
#define WUBU_DRV_H

#include <stddef.h>
#include <stdint.h>

/* the buses */
enum {
    WUBU_DRV_BUS_PCI = 0,
    WUBU_DRV_BUS_USB,
    WUBU_DRV_BUS_ACPI,
    WUBU_DRV_BUS_CPU,
};

#define WUBU_DRV_ANY 0xFFFFu

/* one ID-table entry (vendor/device from PCI, or the class) */
typedef struct {
    uint16_t vendor;
    uint16_t device;
    uint8_t  class_code;
    uint8_t  subclass;
} wubu_drv_id_t;

/* a driver: an ID table + a probe */
typedef struct wubu_drv {
    const char       *name;
    const wubu_drv_id_t *id_table;
    int               n_ids;
    int            (*probe)(struct wubu_drv_dev *dev);
} wubu_drv_t;

/* a device on a bus */
typedef struct wubu_drv_dev {
    int      bus;
    uint16_t vendor;
    uint16_t device;
    uint8_t  class_code;
    uint8_t  subclass;
    /* the PCI BARs (the REAL hardware MMIO windows — wubu_pci reads
     * them from the config space; the drivers map the probe MMIO from
     * these on real hardware) */
    uint64_t bar0;
    uint64_t bar1;
    /* bound state */
    int             bound;
    int             probe_rc;
    const wubu_drv_t *drv;
} wubu_drv_dev_t;

/* DRV1: register a driver. */
int wubu_drv_register(const wubu_drv_t *drv);

/* DRV2: init the built-in table. */
void wubu_drv_init(void);

/* DRV3: add a device to the bus table. */
int wubu_drv_add_device(const wubu_drv_dev_t *dev);

/* DRV5: enumerate the real PCI bus. */
int wubu_drv_pci_scan(void);

/* DRV5b: the BAR accessor — the real-hardware MMIO base. */
int wubu_drv_dev_bar(const wubu_drv_dev_t *dev, uint64_t *bar0,
                     uint64_t *bar1);

/* DRV6: probe every device. */
int wubu_drv_probe(void);

/* DRV7: find a bound device by driver name. */
const wubu_drv_dev_t *wubu_drv_find(const char *drv_name);

/* DRV8: the boot-log summary. */
int wubu_drv_summary(char *out, size_t cap);

/* DRV9: the counts (tests). */
int wubu_drv_device_count(void);
int wubu_drv_driver_count(void);

#endif
