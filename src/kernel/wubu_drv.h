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
struct wubu_drv_dev;  /* forward decl: without it, the probe prototype
                       * below declares a PROTOTYPE-SCOPE struct tag —
                       * a different type from the file-scope struct
                       * defined later (gcc14 errors at the call site). */
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
    /* DRV10: the VA of this device's mapped BAR0 page (0 = not mapped).
     * Written only by wubu_drv_map_mmio / wubu_drv_map_all_mmio, which
     * run AFTER wubu_vmm_init() -- never during early probe. */
    uint64_t        mmio_va;
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

/* DRV10: THE single BAR -> VMM mapping owner. Maps a device's memory
 * BAR0 page at an aligned va_page through wubu_vmm_map_page. Returns 0
 * and writes *va_out on success; -1 when there is no memory BAR, the BAR
 * is I/O-space, the mapper is absent, or the map fails. Fail-soft by
 * design: a mapping failure never blocks probe or routing. */
int wubu_drv_map_mmio(const wubu_drv_dev_t *dev, uint64_t va_page,
                      uint64_t *va_out);

/* DRV11: the late pass over the bound device table. Call only AFTER
 * wubu_vmm_init() -- the mapper allocates page-table pages from the VMM
 * pool, which init resets. One 4K slot per device in the window below.
 * Idempotent. Returns the number of BARs mapped this call. */
int wubu_drv_map_all_mmio(void);

/* 32 device slots x 4K, in the free high-half gap between the kernel
 * (ends < 0xffffffff80300000) and the N9 demand region (0xffffffff90000000). */
#define WUBU_DRV_MMIO_VA_BASE 0xffffffff8c000000ull

#endif
