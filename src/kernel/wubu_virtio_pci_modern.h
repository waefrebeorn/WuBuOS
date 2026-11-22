#ifndef WUBU_VIRTIO_PCI_MODERN_H
#define WUBU_VIRTIO_PCI_MODERN_H

#include <stdint.h>

/* OASIS VIRTIO 1.0 4.1.4: vendor-specific PCI capabilities.
 * cfg_type values are part of the spec; cap_vndr is always 0x09. */
#define WUBU_VPCI_CAP_COMMON_CFG   1u
#define WUBU_VPCI_CAP_NOTIFY_CFG   2u
#define WUBU_VPCI_CAP_ISR_CFG      3u
#define WUBU_VPCI_CAP_DEVICE_CFG   4u
#define WUBU_VPCI_CAP_PCI_CFG      5u

/* OASIS VIRTIO 1.0 2.1 device_status bits. */
#define WUBU_VPCI_STATUS_ACKNOWLEDGE 0x01u
#define WUBU_VPCI_STATUS_DRIVER      0x02u
#define WUBU_VPCI_STATUS_FEATURES_OK 0x08u
#define WUBU_VPCI_STATUS_DRIVER_OK   0x04u
#define WUBU_VPCI_STATUS_FAILED      0x80u
#define WUBU_VPCI_F_VERSION_1        (1u << 0)  /* feature word 1 = bit 32 */

/* Common-config offsets (4.1.4.3), relative to the mapped BAR+offset. */
#define WUBU_VPCI_COMMON_DEV_FEAT_SEL 0x00u
#define WUBU_VPCI_COMMON_DEV_FEAT     0x04u
#define WUBU_VPCI_COMMON_DRV_FEAT_SEL 0x08u
#define WUBU_VPCI_COMMON_DRV_FEAT     0x0Cu
#define WUBU_VPCI_COMMON_STATUS       0x14u
#define WUBU_VPCI_COMMON_QUEUE_SEL    0x16u
#define WUBU_VPCI_COMMON_QUEUE_SIZE   0x18u
#define WUBU_VPCI_COMMON_QUEUE_ENABLE 0x1Cu
#define WUBU_VPCI_COMMON_QUEUE_NOTIFY 0x1Eu
#define WUBU_VPCI_COMMON_QUEUE_DESC   0x20u
#define WUBU_VPCI_COMMON_QUEUE_DRIVER 0x28u
#define WUBU_VPCI_COMMON_QUEUE_DEVICE 0x30u

typedef struct {
    uint8_t  bar;       /* PCI BAR index the capability lives in */
    uint32_t offset;    /* offset inside that BAR */
    uint32_t length;    /* length of the mapped structure */
} wubu_vpci_cap_t;

typedef struct wubu_vpci_caps {
    wubu_vpci_cap_t common;
    wubu_vpci_cap_t notify;
    wubu_vpci_cap_t isr;
    wubu_vpci_cap_t device;
    uint32_t notify_off_multiplier;  /* 0 or an even power of two */
    uint8_t  have_common;
    uint8_t  have_notify;
    uint8_t  have_isr;
    uint8_t  have_device;
} wubu_vpci_caps_t;

/* Walk the PCI capability list (config 0x34) and collect the virtio
 * structures. Returns 0 when COMMON is present, -1 otherwise. This is
 * discovery only: no BAR is mapped and no register is touched here. */
int wubu_vpci_caps_read(uint8_t bus, uint8_t dev, uint8_t fn,
                        wubu_vpci_caps_t *out);

/* Feature handshake on a mapped common config (OASIS 2.1 + 3.1).
 * wanted_low is the driver's low-32 feature mask; VERSION_1 is accepted
 * whenever the device offers it. Sets FEATURES_OK and re-reads status as
 * the spec requires. Does NOT set DRIVER_OK (queues not ready yet). */
int wubu_vpci_feature_handshake(uintptr_t common_base, uint32_t wanted_low);

/* Program queue `idx` and return its notify offset. queue_size is the
 * desired size; the device's advertised maximum is respected. */
int wubu_vpci_queue_setup(uintptr_t common_base, uint16_t idx,
                          uint16_t desired_size,
                          uint64_t desc_gpa, uint64_t avail_gpa,
                          uint64_t used_gpa, uint16_t *notify_off_out);

/* Queue 0 doorbell address; mult must be 0 or an even power of two. */
uintptr_t wubu_vpci_notify_addr(uintptr_t notify_base, uint16_t notify_off,
                                uint32_t multiplier);

/* Metal-only boot diagnostic: read a modern-only virtio disk's capability
 * list and common config; logs one line, binds no device. */
void wubu_vpci_modern_debug_probe(void);

/* Extra register helpers used by the modern block probe. */
void wubu_vpci_features_read(uintptr_t common_base, uint32_t *lo, uint32_t *hi);
int  wubu_vpci_queue_max(uintptr_t common_base, uint16_t idx, uint16_t *max_out);
int  wubu_vpci_config_read64(uintptr_t common_base, uintptr_t devcfg,
                             uint64_t *out);
int  wubu_vpci_driver_ok(uintptr_t common_base);

#endif
