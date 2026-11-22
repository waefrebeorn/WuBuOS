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

#ifdef MYSEED_METAL
#include "klog.h"
#include "wubu_drv.h"
#endif

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

/* ---- common-config accessors (little-endian byte assembly) ---------- */
static uint8_t mw_read8(uintptr_t base, uint32_t off)
{
    return *(volatile uint8_t *)(base + off);
}

static uint16_t mw_read16(uintptr_t base, uint32_t off)
{
    return *(volatile uint16_t *)(base + off);
}

static uint32_t mw_read32(uintptr_t base, uint32_t off)
{
    return *(volatile uint32_t *)(base + off);
}

static void mw_write8(uintptr_t base, uint32_t off, uint8_t v)
{
    *(volatile uint8_t *)(base + off) = v;
}

static void mw_write16(uintptr_t base, uint32_t off, uint16_t v)
{
    *(volatile uint16_t *)(base + off) = v;
}

static void mw_write32(uintptr_t base, uint32_t off, uint32_t v)
{
    *(volatile uint32_t *)(base + off) = v;
}

static void mw_write64(uintptr_t base, uint32_t off, uint64_t v)
{
    mw_write32(base, off, (uint32_t)v);
    mw_write32(base, off + 4u, (uint32_t)(v >> 32));
}

int wubu_vpci_feature_handshake(uintptr_t common_base, uint32_t wanted_low)
{
    uint32_t dev_low, dev_high;
    uint8_t st;

    if (!common_base) return -1;
    /* OASIS 2.1: writing 0 resets the device. */
    mw_write8(common_base, WUBU_VPCI_COMMON_STATUS, 0);
    mw_write8(common_base, WUBU_VPCI_COMMON_STATUS,
              (uint8_t)(WUBU_VPCI_STATUS_ACKNOWLEDGE | WUBU_VPCI_STATUS_DRIVER));

    mw_write32(common_base, WUBU_VPCI_COMMON_DEV_FEAT_SEL, 0);
    dev_low = mw_read32(common_base, WUBU_VPCI_COMMON_DEV_FEAT);
    mw_write32(common_base, WUBU_VPCI_COMMON_DEV_FEAT_SEL, 1);
    dev_high = mw_read32(common_base, WUBU_VPCI_COMMON_DEV_FEAT);

    mw_write32(common_base, WUBU_VPCI_COMMON_DRV_FEAT_SEL, 0);
    mw_write32(common_base, WUBU_VPCI_COMMON_DRV_FEAT, dev_low & wanted_low);
    mw_write32(common_base, WUBU_VPCI_COMMON_DRV_FEAT_SEL, 1);
    mw_write32(common_base, WUBU_VPCI_COMMON_DRV_FEAT,
              dev_high & WUBU_VPCI_F_VERSION_1);

    st = mw_read8(common_base, WUBU_VPCI_COMMON_STATUS);
    mw_write8(common_base, WUBU_VPCI_COMMON_STATUS,
              (uint8_t)(st | WUBU_VPCI_STATUS_FEATURES_OK));
    st = mw_read8(common_base, WUBU_VPCI_COMMON_STATUS);
    if (st & WUBU_VPCI_STATUS_FAILED) return -1;
    if (!(st & WUBU_VPCI_STATUS_FEATURES_OK)) return -1;
    return 0;
}

int wubu_vpci_queue_setup(uintptr_t common_base, uint16_t idx,
                          uint16_t desired_size,
                          uint64_t desc_gpa, uint64_t avail_gpa,
                          uint64_t used_gpa, uint16_t *notify_off_out)
{
    uint16_t max_size, size;

    if (!common_base) return -1;
    mw_write16(common_base, WUBU_VPCI_COMMON_QUEUE_SEL, idx);
    max_size = mw_read16(common_base, WUBU_VPCI_COMMON_QUEUE_SIZE);
    if (!max_size) return -1;
    size = (desired_size && desired_size <= max_size) ? desired_size : max_size;
    mw_write16(common_base, WUBU_VPCI_COMMON_QUEUE_SIZE, size);
    mw_write64(common_base, WUBU_VPCI_COMMON_QUEUE_DESC, desc_gpa);
    mw_write64(common_base, WUBU_VPCI_COMMON_QUEUE_DRIVER, avail_gpa);
    mw_write64(common_base, WUBU_VPCI_COMMON_QUEUE_DEVICE, used_gpa);
    mw_write16(common_base, WUBU_VPCI_COMMON_QUEUE_ENABLE, 1u);
    if (notify_off_out)
        *notify_off_out = mw_read16(common_base, WUBU_VPCI_COMMON_QUEUE_NOTIFY);
    return 0;
}

uintptr_t wubu_vpci_notify_addr(uintptr_t notify_base, uint16_t notify_off,
                                uint32_t multiplier)
{
    /* 4.1.5.2: multiplier is 0 or an even power of two. */
    if (multiplier && (multiplier < 2u || (multiplier & (multiplier - 1u))))
        return 0;
    return notify_base + (uintptr_t)notify_off * (uintptr_t)multiplier;
}

/* ---- helpers for the modern block probe --------------------------- */
void wubu_vpci_features_read(uintptr_t common_base, uint32_t *lo, uint32_t *hi)
{
    if (!common_base) return;
    mw_write32(common_base, WUBU_VPCI_COMMON_DEV_FEAT_SEL, 0);
    if (lo) *lo = mw_read32(common_base, WUBU_VPCI_COMMON_DEV_FEAT);
    mw_write32(common_base, WUBU_VPCI_COMMON_DEV_FEAT_SEL, 1);
    if (hi) *hi = mw_read32(common_base, WUBU_VPCI_COMMON_DEV_FEAT);
}

int wubu_vpci_queue_max(uintptr_t common_base, uint16_t idx, uint16_t *max_out)
{
    uint16_t q;
    if (!common_base || !max_out) return -1;
    mw_write16(common_base, WUBU_VPCI_COMMON_QUEUE_SEL, idx);
    q = mw_read16(common_base, WUBU_VPCI_COMMON_QUEUE_SIZE);
    if (!q) return -1;
    *max_out = q;
    return 0;
}

int wubu_vpci_config_read64(uintptr_t common_base, uintptr_t devcfg,
                            uint64_t *out)
{
    uint8_t b[8];
    uint8_t g1, g2;
    int tries;

    if (!common_base || !devcfg || !out) return -1;
    for (tries = 0; tries < 8; tries++) {
        g1 = mw_read8(common_base, 0x15u);        /* config_generation */
        for (int i = 0; i < 8; i++) b[i] = mw_read8(devcfg, (uint32_t)i);
        g2 = mw_read8(common_base, 0x15u);
        if (g1 == g2 && !(g1 & 1u)) break;
    }
    if (tries == 8) return -1;
    uint64_t v = 0;
    for (int i = 7; i >= 0; i--) v = (v << 8) | b[i];
    *out = v;
    return 0;
}

int wubu_vpci_driver_ok(uintptr_t common_base)
{
    uint8_t st;
    if (!common_base) return -1;
    st = mw_read8(common_base, WUBU_VPCI_COMMON_STATUS);
    if (st & (WUBU_VPCI_STATUS_FAILED | 0x40u)) return -1;
    mw_write8(common_base, WUBU_VPCI_COMMON_STATUS,
              (uint8_t)(st | WUBU_VPCI_STATUS_DRIVER_OK));
    st = mw_read8(common_base, WUBU_VPCI_COMMON_STATUS);
    if (!(st & WUBU_VPCI_STATUS_DRIVER_OK) ||
        (st & WUBU_VPCI_STATUS_FAILED)) return -1;
    return 0;
}

/* ------------------------------------------------------------------ */

#ifdef MYSEED_METAL
void wubu_vpci_modern_debug_probe(void)
{
    const wubu_drv_dev_t *d =
        wubu_drv_find_pci_id(0x1AF4u, 0x1042u, 0x01u);
    wubu_vpci_caps_t caps;
    uint64_t va = 0;
    uint32_t lo, hi;
    uint8_t st;

    if (!d || !d->bar4) return;
    if (wubu_vpci_caps_read(d->bus, d->pci_dev, d->pci_fn, &caps) != 0) {
        klog_printf("WuBuOS: modern virtio-blk cap walk FAILED\n");
        return;
    }
    if (wubu_drv_map_bar_range(d, 4u, WUBU_DRV_MMIO_VA_BASE + 0x100000ull,
                               1u, &va) != 0) {
        klog_printf("WuBuOS: modern virtio-blk BAR4 map FAILED\n");
        return;
    }
    uintptr_t c = (uintptr_t)va + caps.common.offset;
    mw_write32(c, WUBU_VPCI_COMMON_DEV_FEAT_SEL, 0);
    lo = mw_read32(c, WUBU_VPCI_COMMON_DEV_FEAT);
    mw_write32(c, WUBU_VPCI_COMMON_DEV_FEAT_SEL, 1);
    hi = mw_read32(c, WUBU_VPCI_COMMON_DEV_FEAT);
    st = mw_read8(c, WUBU_VPCI_COMMON_STATUS);
    uint16_t dbg_q = 0;
    (void)wubu_vpci_queue_max(c, 0, &dbg_q);
    klog_printf("WuBuOS: modern virtio-blk 1af4:1042 caps=%u common=%u "
                "notify=%u feat_lo=%x feat_hi=%x status=%x qsize=%u\n",
                (unsigned)(caps.have_common + caps.have_notify +
                           caps.have_isr + caps.have_device),
                (unsigned)caps.common.offset, (unsigned)caps.notify.offset,
                (unsigned)lo, (unsigned)hi, (unsigned)st, (unsigned)dbg_q);
}
#endif
