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
    /* For PCI devices this is the PCI bus NUMBER (0..255), including
     * secondary buses behind a bridge. Capability walkers and config-space
     * readers use it directly; do not confuse it with bus_type below. */
    int      bus;
    uint8_t  bus_type;               /* WUBU_DRV_BUS_PCI / USB / ACPI / CPU */
    uint8_t  pci_dev, pci_fn;        /* PCI BDF, for capability checks */
    uint16_t vendor;
    uint16_t device;
    uint8_t  class_code;
    uint8_t  subclass;
    /* the PCI BARs (the REAL hardware MMIO windows — wubu_pci reads
     * them from the config space; the drivers map the probe MMIO from
     * these on real hardware) */
    uint64_t bar0;
    uint64_t bar1;
    /* Authority tokens. Granted at normal registration from ROOT:
     * DEV_OPEN gates probe/bind, MMIO_MAP gates BAR mapping. */
    uint64_t dev_open_cap;
    uint64_t mmio_cap;
    /* size of BAR0 (0 if unknown), propagated from the PCI scan. */
    uint64_t bar0_size;
    /* BAR4/BAR5 capability window (modern virtio). Captured only. */
    uint64_t bar4;
    uint64_t bar4_size;
    uint8_t  bar4_is_io;
    /* BAR0 address space carried from the PCI scan (1 = I/O ports).
     * The MMIO owner rejects these; the I/O owner consumes them. */
    uint8_t  bar0_is_io;
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
/* Deliberately unprivileged registration for sandbox/negative gates: no
 * capabilities are minted, so probe and MMIO mapping must both refuse. */
int wubu_drv_add_device_noauth(const wubu_drv_dev_t *dev);
/* Test/sandbox control over a registered device's DEV_OPEN authority. */
int wubu_drv_revoke_dev_open(uint16_t vendor, uint16_t device, uint8_t class_code);
int wubu_drv_grant_dev_open(uint16_t vendor, uint16_t device, uint8_t class_code);

/* DRV5: enumerate the real PCI bus. */
int wubu_drv_pci_scan(void);

/* DRV5b: the BAR accessor — the real-hardware MMIO base. */
int wubu_drv_dev_bar(const wubu_drv_dev_t *dev, uint64_t *bar0,
                     uint64_t *bar1);

/* DRV6: probe every device. */
int wubu_drv_probe(void);

/* DRV7: find a bound device by driver name. */
const wubu_drv_dev_t *wubu_drv_find(const char *drv_name);

/* DRV7b: find a bound device by its PCI class/subclass (e.g. 0x0C/0x03
 * for xHCI). Lets a driver obtain its own device from the one registry
 * instead of running a second PCI scan. */
const wubu_drv_dev_t *wubu_drv_find_class(uint8_t class_code, uint8_t subclass);

/* DRV12: find a PCI device in the ONE registry by vendor/device/class.
 * class_code 0 means "any". Callers loop alternate device IDs themselves. */
const wubu_drv_dev_t *wubu_drv_find_pci_id(uint16_t vendor, uint16_t device,
                                            uint8_t class_code);

/* DRV13: the single I/O-BAR owner. The PCI scan masks BAR type bits, so
 * the registry carries bar0_is_io; this is the ONLY place that turns an
 * I/O BAR base into a 16-bit port number. Rejects MMIO BARs, zero bases
 * and 64-bit-ish values. Returns 0 and writes *io_out on success. */
int wubu_drv_io_base(const wubu_drv_dev_t *dev, uint16_t *io_out);

/* DRV14: the one-line registry report used by the boot log and the live
 * `drv` console command (no second formatter). Returns bytes written. */
int wubu_drv_registry_line(char *out, size_t cap);

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

/* Map `pages` 4K pages from BAR index `bar_index` (0 or 4) at `va_page`.
 * The single BAR -> VMM owner; BAR4 is the 64-bit capability window of
 * a modern virtio function. Existing mmio/mmio_range calls use BAR0. */
int wubu_drv_map_bar_range(const wubu_drv_dev_t *dev, unsigned bar_index,
                           uint64_t va_page, uint32_t pages, uint64_t *va_out);

/* DRV10b: the range form -- one owner for a multi-page MMIO aperture.
 * Maps `pages` consecutive 4K pages from BAR0 at `va_page`; 1 <= pages <=
 * WUBU_DRV_MMIO_MAX_PAGES, va_page 4K-aligned. Returns 0 and writes the
 * first VA on success. A failed page rolls back the pages already mapped
 * (through wubu_vmm_unmap, weak-seamed) and returns -1 with no fabricated VA. */
#define WUBU_DRV_MMIO_MAX_PAGES 16u
int wubu_drv_map_mmio_range(const wubu_drv_dev_t *dev, uint64_t va_page,
                            uint32_t pages, uint64_t *va_out);

/* DRV11: the late pass over the bound device table. Call only AFTER
 * wubu_vmm_init() -- the mapper allocates page-table pages from the VMM
 * pool, which init resets. One 4K slot per device in the window below.
 * Idempotent. Returns the number of BARs mapped this call. */
int wubu_drv_map_all_mmio(void);

/* 32 device slots x 4K, in the free high-half gap between the kernel
 * (ends < 0xffffffff80300000) and the N9 demand region (0xffffffff90000000). */
#define WUBU_DRV_MMIO_VA_BASE 0xffffffff8c000000ull

#endif
