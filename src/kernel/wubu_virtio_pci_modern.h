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

typedef struct {
    uint8_t  bar;       /* PCI BAR index the capability lives in */
    uint32_t offset;    /* offset inside that BAR */
    uint32_t length;    /* length of the mapped structure */
} wubu_vpci_cap_t;

typedef struct {
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

#endif
