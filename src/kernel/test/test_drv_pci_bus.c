/*
 * test_drv_pci_bus.c -- synthetic negative control for PCI bus propagation.
 *
 * A PCI-to-PCI bridge puts its children on a secondary bus. The registry
 * hands a device to the capability walkers, which read its config space at
 * (dev->bus, dev->pci_dev, dev->pci_fn). If dev->bus is the bus TYPE
 * (WUBU_DRV_BUS_PCI == 0) instead of the PCI bus NUMBER, the walk hits bus 0
 * and the modern device is invisible. This test seeds a bridge on bus 0 and
 * a modern virtio-blk on bus 1, scans through the REAL wubu_drv_pci_scan(),
 * then walks the caps through the REAL wubu_vpci_caps_read().
 */
#include "wubu_drv.h"
#include "wubu_pci.h"
#include "wubu_virtio_pci_modern.h"

#include <stdio.h>
#include <string.h>

/* Stub the 18 built-in drivers: this test exercises the registry/scan seam,
 * not any driver's probe. */
const wubu_drv_t wubu_drv_nvme = {0};
const wubu_drv_t wubu_drv_ahci = {0};
const wubu_drv_t wubu_drv_wifi = {0};
const wubu_drv_t wubu_drv_net = {0};
const wubu_drv_t wubu_drv_hda = {0};
const wubu_drv_t wubu_drv_gpu = {0};
const wubu_drv_t wubu_drv_battery = {0};
const wubu_drv_t wubu_drv_sd = {0};
const wubu_drv_t wubu_drv_usb_hid = {0};
const wubu_drv_t wubu_drv_usb_msc = {0};
const wubu_drv_t wubu_drv_usb_bt = {0};
const wubu_drv_t wubu_drv_thermal = {0};
const wubu_drv_t wubu_drv_virtio_blk = {0};
const wubu_drv_t wubu_drv_virtio_net = {0};
const wubu_drv_t wubu_drv_virtio_gpu = {0};
const wubu_drv_t wubu_drv_virtio_input = {0};
const wubu_drv_t wubu_drv_arm_platform = {0};
const wubu_drv_t wubu_drv_intel_platform = {0};

/* Fake config space: absent slots return all ones. */
#define CFG_SLOTS 4
typedef struct {
    uint8_t bus, dev, fn;
    uint32_t ids, cls, hdr, sec, bar0, mask0;
    uint32_t bar4, mask4, probe4;
    uint8_t status_caps, cap_ptr;
    uint8_t caps[64];
} fake_cfg_t;
static fake_cfg_t cfg[CFG_SLOTS];
static int ncfg;
static int bus_reads[256];

static fake_cfg_t *slot(uint8_t bus, uint8_t dev, uint8_t fn)
{
    for (int i = 0; i < ncfg; i++)
        if (cfg[i].bus == bus && cfg[i].dev == dev && cfg[i].fn == fn)
            return &cfg[i];
    return NULL;
}

static void seed(uint8_t bus, uint8_t dev, uint32_t ids, uint32_t cls)
{
    if (ncfg >= CFG_SLOTS) return;
    memset(&cfg[ncfg], 0, sizeof(cfg[0]));
    cfg[ncfg].bus = bus; cfg[ncfg].dev = dev; cfg[ncfg].fn = 0;
    cfg[ncfg].ids = ids; cfg[ncfg].cls = cls;
    cfg[ncfg].mask0 = 0xFFFFFFFFu;
    cfg[ncfg].mask4 = 0xFFFFC000u;
    cfg[ncfg].bar4 = 0xFE000000u;
    ncfg++;
}

uint32_t wubu_pci_read_config(uint8_t bus, uint8_t dev, uint8_t fn, uint8_t off)
{
    bus_reads[bus]++;
    fake_cfg_t *c = slot(bus, dev, fn);
    if (!c) return 0xFFFFFFFFu;
    switch (off) {
    case 0x00: return c->ids;
    case 0x04: return c->status_caps ? (1u << 20) : 0u;
    case 0x08: return c->cls;
    case 0x0C: return c->hdr;
    case 0x10: return c->bar0;
    case 0x18: return c->sec;
    case 0x20: return c->probe4 == 0xFFFFFFFFu ? c->mask4 : c->bar4;
    case 0x34: return c->cap_ptr;
    default:
        if (off >= 0x40 && off < 0x80) {
            const uint8_t *p = &c->caps[off - 0x40];
            return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
                   ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
        }
        return 0;
    }
}

void wubu_pci_write_config(uint8_t bus, uint8_t dev, uint8_t fn, uint8_t off,
                           uint32_t val)
{
    fake_cfg_t *c = slot(bus, dev, fn);
    if (c && off == 0x20) c->probe4 = val;
}

/* Two vendor caps at 0x40 (common) and 0x50 (notify). */
static void seed_caps(int idx)
{
    uint8_t *c = cfg[idx].caps;
    cfg[idx].status_caps = 1;
    cfg[idx].cap_ptr = 0x40;
    memset(c, 0, sizeof(cfg[idx].caps));
    c[0x40 - 0x40] = 0x09; c[0x41 - 0x40] = 0x50; c[0x42 - 0x40] = 16; c[0x43 - 0x40] = 1;
    c[0x44 - 0x40] = 4;
    c[0x48 - 0x40] = 0x00; c[0x49 - 0x40] = 0x10;   /* common offset 0x1000 */
    c[0x50 - 0x40] = 0x09; c[0x51 - 0x40] = 0; c[0x52 - 0x40] = 20; c[0x53 - 0x40] = 2;
    c[0x54 - 0x40] = 4;
    c[0x58 - 0x40] = 0x00; c[0x59 - 0x40] = 0x20;   /* notify offset 0x2000 */
}

int main(void)
{
    printf("=== test_drv_pci_bus (registry keeps PCI bus number) ===\n");
    /* bus 0 dev 5: PCI-to-PCI bridge -> secondary bus 1. */
    seed(0, 5, 0x00001000u, 0x06040000u);
    cfg[0].hdr = 0x00010000u;
    cfg[0].sec = 1u << 8;
    /* bus 1 dev 2: modern virtio-blk 1af4:1042, class 01/00. */
    seed(1, 2, 0x10421AF4u, 0x01000000u);
    seed_caps(1);

    wubu_drv_init();
    int n = wubu_drv_pci_scan();
    printf("  registry scanned %d devices\n", n);
    if (n < 2) { printf("  FAIL: bridge child missing from registry\n"); return 1; }

    const wubu_drv_dev_t *d =
        wubu_drv_find_pci_id(0x1AF4u, 0x1042u, 0x01u);
    if (!d) { printf("  FAIL: modern disk not in registry\n"); return 1; }

    wubu_vpci_caps_t caps;
    int rc = wubu_vpci_caps_read((uint8_t)d->bus, d->pci_dev, d->pci_fn, &caps);
    printf("  cap walk via registry bus=%u dev=%u: rc=%d common=%u notify=%u\n",
           (unsigned)d->bus, (unsigned)d->pci_dev, rc,
           (unsigned)caps.have_common, (unsigned)caps.have_notify);
    if (rc != 0 || !caps.have_common || !caps.have_notify) {
        printf("  FAIL: cap walk used the wrong PCI bus (bus 0 reads=%d, "
               "bus 1 reads=%d)\n", bus_reads[0], bus_reads[1]);
        return 1;
    }
    printf("  PASS: registry carried PCI bus 1 to the capability walker\n");
    return 0;
}
