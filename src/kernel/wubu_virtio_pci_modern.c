/* wubu_virtio_pci_modern.c -- PCI capability discovery for modern virtio.
 *
 * OASIS VIRTIO 1.0 4.1.4 / 4.1.4.1-4: the capability list starts at PCI
 * config 0x34 (when Command bit 20 is set). Each vendor capability has:
 *   +0 cap_vndr (0x09), +1 cap_next, +2 cap_len, +3 cfg_type, +4 bar,
 *   +8 offset (u32), +12 length (u32); NOTIFY adds +16 multiplier (u32).
 *
 * Deliberately narrow: config reads only, no MMIO access and no device
 * state changes. The legacy transport and registry are untouched.
 */
#include "wubu_virtio_pci_modern.h"
#include "wubu_pci.h"

#define VPCI_STATUS_CAPS   (1u << 20)
#define VPCI_MAX_CAPS      48u

static uint8_t cfg_byte(uint8_t bus, uint8_t dev, uint8_t fn, uint8_t off)
{
    uint32_t d = wubu_pci_read32(bus, dev, fn, (uint8_t)(off & 0xFCu));
    return (uint8_t)(d >> ((off & 3u) * 8u));
}

static uint32_t cfg_dword(uint8_t bus, uint8_t dev, uint8_t fn, uint8_t off)
{
    return wubu_pci_read32(bus, dev, fn, (uint8_t)(off & 0xFCu));
}

static void take_cap(uint8_t bus, uint8_t dev, uint8_t fn, uint8_t at,
                     uint8_t type, uint8_t cap_len, wubu_vpci_caps_t *out)
{
    wubu_vpci_cap_t *c;

    switch (type) {
    case WUBU_VPCI_CAP_COMMON_CFG: c = &out->common;  out->have_common = 1; break;
    case WUBU_VPCI_CAP_NOTIFY_CFG: c = &out->notify;  out->have_notify = 1; break;
    case WUBU_VPCI_CAP_ISR_CFG:    c = &out->isr;     out->have_isr = 1;    break;
    case WUBU_VPCI_CAP_DEVICE_CFG: c = &out->device;  out->have_device = 1; break;
    default: return;                 /* MSI-X, PCI_CFG and vendor extras */
    }
    c->bar    = cfg_byte(bus, dev, fn, (uint8_t)(at + 4u));
    c->offset = cfg_dword(bus, dev, fn, (uint8_t)(at + 8u));
    c->length = cfg_dword(bus, dev, fn, (uint8_t)(at + 12u));
    if (type == WUBU_VPCI_CAP_NOTIFY_CFG && cap_len >= 20u)
        out->notify_off_multiplier = cfg_dword(bus, dev, fn,
                                               (uint8_t)(at + 16u));
}

int wubu_vpci_caps_read(uint8_t bus, uint8_t dev, uint8_t fn,
                        wubu_vpci_caps_t *out)
{
    uint8_t at, guard = 0;

    if (!out) return -1;
    for (int i = 0; i < (int)sizeof(*out); i++)
        ((uint8_t *)out)[i] = 0;

    if (!(cfg_dword(bus, dev, fn, 0x04) & VPCI_STATUS_CAPS)) return -1;
    at = cfg_byte(bus, dev, fn, 0x34);
    while (at >= 0x40u && at <= 0xF4u && (at & 3u) == 0u && guard++ < VPCI_MAX_CAPS) {
        uint8_t vndr = cfg_byte(bus, dev, fn, at);
        uint8_t next = cfg_byte(bus, dev, fn, (uint8_t)(at + 1u));
        uint8_t len  = cfg_byte(bus, dev, fn, (uint8_t)(at + 2u));
        uint8_t type = cfg_byte(bus, dev, fn, (uint8_t)(at + 3u));
        /* MSI-X and other non-virtio caps are in the same chain; cap_next
         * at +1 is common to all of them, so skip rather than stop. */
        if (vndr == 0x09u && len >= 16u && type != WUBU_VPCI_CAP_PCI_CFG)
            take_cap(bus, dev, fn, at, type, len, out);
        if (!next || next == at) break;
        at = next;
    }
    return out->have_common ? 0 : -1;
}
