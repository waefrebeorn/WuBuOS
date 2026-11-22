/* test_virtio_pci_modern.c -- gate the OASIS VIRTIO 1.0 4.1.4 capability
 * walker against a synthetic config space shaped like a real QEMU
 * virtio-blk-pci,disable-legacy=on device (MSI-X at 0x98 first in the
 * chain, not at the start of the virtio capabilities).
 *
 * House pattern: include the production TU, stub the single PCI config
 * boundary, assert on discovery results. No MMIO or device state here.
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "wubu_virtio_pci_modern.c"

static int failures = 0;
#define CHECK(c) do { if (!(c)) { \
    printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); failures++; \
} } while (0)

static uint8_t cfg[256];
static int read_calls;

uint32_t wubu_pci_read32(uint8_t bus, uint8_t dev, uint8_t fn, uint8_t off)
{
    (void)bus; (void)dev; (void)fn;
    uint32_t at = off & 0xFCu;
    read_calls++;
    if (at + 4u > sizeof(cfg)) return 0;
    return (uint32_t)cfg[at] | ((uint32_t)cfg[at + 1u] << 8) |
           ((uint32_t)cfg[at + 2u] << 16) | ((uint32_t)cfg[at + 3u] << 24);
}

static void put16(uint16_t off, uint16_t v)
{
    cfg[off]     = (uint8_t)v;
    cfg[off + 1] = (uint8_t)(v >> 8);
}

static void put32(uint16_t off, uint32_t v)
{
    put16(off, (uint16_t)v);
    put16((uint16_t)(off + 2), (uint16_t)(v >> 16));
}

/* cfg_type at +3, bar +4, offset +8, length +12, multiplier +16 */
static void cap(uint8_t at, uint8_t vndr, uint8_t next, uint8_t type,
                uint8_t bar, uint32_t off, uint32_t len, uint32_t mult)
{
    cfg[at] = vndr;
    cfg[at + 1] = next;
    cfg[at + 2] = (uint8_t)(type == 0 ? 12u : 20u);
    cfg[at + 3] = type;
    cfg[at + 4] = bar;
    put32((uint16_t)(at + 8), off);
    put32((uint16_t)(at + 12), len);
    if (type == WUBU_VPCI_CAP_NOTIFY_CFG)
        put32((uint16_t)(at + 16), mult);
}

static void qemu_chain(void)
{
    memset(cfg, 0, sizeof(cfg));
    cfg[6] |= 0x10u;                 /* Command/Status bit 20: caps present */
    put32(0x34, 0x98u);              /* cap list pointer */
    cap(0x98, 0x11, 0x84, 0, 4, 0, 0, 0);     /* MSI-X: non-virtio cap */
    cap(0x84, 0x09, 0x70, WUBU_VPCI_CAP_PCI_CFG, 4, 0, 0, 0);
    cap(0x70, 0x09, 0x60, WUBU_VPCI_CAP_NOTIFY_CFG, 4, 0x3000, 0x1000, 4);
    cap(0x60, 0x09, 0x50, WUBU_VPCI_CAP_DEVICE_CFG, 4, 0x2000, 0x1000, 0);
    cap(0x50, 0x09, 0x40, WUBU_VPCI_CAP_ISR_CFG, 4, 0x1000, 1, 0);
    cap(0x40, 0x09, 0,    WUBU_VPCI_CAP_COMMON_CFG, 4, 0, 0x1000, 0);
}

/* ---- flat fake common-config register file ------------------------- */
static uint8_t common_regs[0x100];

static void cp16(uint16_t off, uint16_t v)
{
    common_regs[off] = (uint8_t)v;
    common_regs[off + 1] = (uint8_t)(v >> 8);
}

static void cp32(uint16_t off, uint32_t v)
{
    cp16(off, (uint16_t)v);
    cp16((uint16_t)(off + 2), (uint16_t)(v >> 16));
}

static uint16_t cg16(uint16_t off)
{
    return (uint16_t)((uint16_t)common_regs[off] |
                      ((uint16_t)common_regs[off + 1] << 8));
}

static uint32_t cg32(uint16_t off)
{
    return (uint32_t)common_regs[off] |
           ((uint32_t)common_regs[off + 1] << 8) |
           ((uint32_t)common_regs[off + 2] << 16) |
           ((uint32_t)common_regs[off + 3] << 24);
}

static void test_handshake(void)
{
    /* Flat register model: select windows are written in order, but both
     * feature reads see the same cell. The gate checks the spec's write
     * sequence and status bits, not select-dependent device emulation. */
    memset(common_regs, 0, sizeof(common_regs));
    cp32(WUBU_VPCI_COMMON_DEV_FEAT, (1u << 9) | 1u); /* FLUSH + VERSION_1 */
    CHECK(wubu_vpci_feature_handshake((uintptr_t)common_regs,
                                      (1u << 9) | (1u << 1)) == 0);
    CHECK(common_regs[WUBU_VPCI_COMMON_STATUS] ==
          (WUBU_VPCI_STATUS_ACKNOWLEDGE | WUBU_VPCI_STATUS_DRIVER |
           WUBU_VPCI_STATUS_FEATURES_OK));
    CHECK(cg32(WUBU_VPCI_COMMON_DRV_FEAT_SEL) == 1u);  /* both windows written */
    CHECK(cg32(WUBU_VPCI_COMMON_DRV_FEAT) & WUBU_VPCI_F_VERSION_1);
    CHECK(wubu_vpci_feature_handshake(0, 0) == -1);
}

static void test_queue_and_notify(void)
{
    uint16_t noff = 0xFFFFu;

    memset(common_regs, 0, sizeof(common_regs));
    cp16(WUBU_VPCI_COMMON_QUEUE_SIZE, 128);
    cp16(WUBU_VPCI_COMMON_QUEUE_NOTIFY, 3);
    CHECK(wubu_vpci_queue_setup((uintptr_t)common_regs, 2, 64,
                                0x1000u, 0x2000u, 0x3000u, &noff) == 0);
    CHECK(cg16(WUBU_VPCI_COMMON_QUEUE_SEL) == 2u);
    CHECK(cg16(WUBU_VPCI_COMMON_QUEUE_SIZE) == 64u);    /* desired <= max */
    CHECK(cg32(WUBU_VPCI_COMMON_QUEUE_DESC) == 0x1000u);
    CHECK(cg32(WUBU_VPCI_COMMON_QUEUE_DRIVER) == 0x2000u);
    CHECK(cg32(WUBU_VPCI_COMMON_QUEUE_DEVICE) == 0x3000u);
    CHECK(cg16(WUBU_VPCI_COMMON_QUEUE_ENABLE) == 1u);
    CHECK(noff == 3);

    memset(common_regs, 0, sizeof(common_regs));
    cp16(WUBU_VPCI_COMMON_QUEUE_SIZE, 16);
    CHECK(wubu_vpci_queue_setup((uintptr_t)common_regs, 0, 64, 0, 0, 0,
                                &noff) == 0);
    CHECK(cg16(WUBU_VPCI_COMMON_QUEUE_SIZE) == 16u);    /* clamp to device max */

    /* 4.1.5.2: doorbell = cap offset + notify_off * multiplier */
    CHECK(wubu_vpci_notify_addr(0xFE003000u, 3, 4) == 0xFE00300Cu);
    CHECK(wubu_vpci_notify_addr(0xFE003000u, 3, 0) == 0xFE003000u);
    CHECK(wubu_vpci_notify_addr(0xFE003000u, 3, 3) == 0);  /* not a power of 2 */

    /* probe helpers: feature words, queue max, config generation, DRIVER_OK */
    memset(common_regs, 0, sizeof(common_regs));
    cp32(WUBU_VPCI_COMMON_DEV_FEAT, (1u << 9) | 1u);
    uint32_t flo = 0, fhi = 0;
    wubu_vpci_features_read((uintptr_t)common_regs, &flo, &fhi);
    CHECK(cg32(WUBU_VPCI_COMMON_DEV_FEAT_SEL) == 1u);
    CHECK(fhi & WUBU_VPCI_F_VERSION_1);
    cp16(WUBU_VPCI_COMMON_QUEUE_SIZE, 64);
    uint16_t qmax = 0;
    CHECK(wubu_vpci_queue_max((uintptr_t)common_regs, 0, &qmax) == 0 &&
          qmax == 64);
    uint64_t v64 = 0;
    uint8_t cfgdev[8] = { 0x88, 0x77, 0x66, 0x55, 0x44, 0x33, 0x22, 0x11 };
    common_regs[0x15] = 0;
    CHECK(wubu_vpci_config_read64((uintptr_t)common_regs,
                                  (uintptr_t)cfgdev, &v64) == 0);
    CHECK(v64 == 0x1122334455667788ull);
    common_regs[WUBU_VPCI_COMMON_STATUS] = WUBU_VPCI_STATUS_ACKNOWLEDGE |
                                           WUBU_VPCI_STATUS_DRIVER;
    CHECK(wubu_vpci_driver_ok((uintptr_t)common_regs) == 0);
    CHECK(common_regs[WUBU_VPCI_COMMON_STATUS] & WUBU_VPCI_STATUS_DRIVER_OK);
    common_regs[WUBU_VPCI_COMMON_STATUS] = WUBU_VPCI_STATUS_FAILED;
    CHECK(wubu_vpci_driver_ok((uintptr_t)common_regs) == -1);
}

int main(void)
{
    wubu_vpci_caps_t caps;

    test_handshake();
    test_queue_and_notify();
    qemu_chain();
    read_calls = 0;
    CHECK(wubu_vpci_caps_read(0, 4, 0, &caps) == 0);
    CHECK(read_calls < 200);        /* bounded walk, no unbounded loop */
    CHECK(caps.have_common && caps.have_notify && caps.have_isr &&
          caps.have_device);
    CHECK(caps.common.bar == 4 && caps.common.offset == 0 &&
          caps.common.length == 0x1000);
    CHECK(caps.notify.bar == 4 && caps.notify.offset == 0x3000 &&
          caps.notify.length == 0x1000 && caps.notify_off_multiplier == 4);
    CHECK(caps.isr.bar == 4 && caps.isr.offset == 0x1000);
    CHECK(caps.device.bar == 4 && caps.device.offset == 0x2000);

    /* no capability list -> refuse */
    cfg[6] &= (uint8_t)~0x10u;
    CHECK(wubu_vpci_caps_read(0, 4, 0, &caps) == -1);
    cfg[6] |= 0x10u;

    /* malformed cap shorter than the generic header -> no common cfg */
    memset(cfg, 0, sizeof(cfg));
    cfg[6] |= 0x10u;
    put32(0x34, 0x40);
    cfg[0x40] = 0x09; cfg[0x41] = 0; cfg[0x42] = 8;
    CHECK(wubu_vpci_caps_read(0, 4, 0, &caps) == -1);

    /* a self-pointing cap_next must not hang; still finds common first */
    qemu_chain();
    cfg[0x40 + 1] = 0x40;
    CHECK(wubu_vpci_caps_read(0, 4, 0, &caps) == 0);

    if (failures) {
        printf("test_virtio_pci_modern: %d FAILURES\n", failures);
        return 1;
    }
    printf("test_virtio_pci_modern: ALL PASS (QEMU-shaped cap chain, "
           "non-virtio caps skipped, bounded walk)\n");
    return 0;
}
