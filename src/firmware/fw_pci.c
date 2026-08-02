/*
 * fw_pci.c  --  WuBuFW PCI/PCIe enumeration and resource access.
 *
 * Every real driver (AHCI, NVMe, XHCI, GOP) needs to find its device and its
 * BARs, so enumeration is the foundation of the driver layer. Uses legacy
 * CF8/CFC config access, which QEMU and all x86 chipsets implement; ECAM is
 * used when an MCFG table is present (see fw_acpi.c) for extended config
 * space beyond offset 0xFF.
 */

#include "fw.h"
#include "fw_pci.h"

#define CFG_ADDR 0xCF8
#define CFG_DATA 0xCFC

static fw_pci_dev g_devs[FW_PCI_MAX_DEV];
static int g_ndev;
static uint64_t g_ecam_base;
static uint8_t  g_ecam_start_bus, g_ecam_end_bus;

static uint32_t cf8(uint8_t bus, uint8_t dev, uint8_t fn, uint8_t off) {
    return 0x80000000u | ((uint32_t)bus << 16) | ((uint32_t)dev << 11) |
           ((uint32_t)fn << 8) | (off & 0xFC);
}

void fw_pci_set_ecam(uint64_t base, uint8_t start_bus, uint8_t end_bus) {
    g_ecam_base = base;
    g_ecam_start_bus = start_bus;
    g_ecam_end_bus = end_bus;
}

static volatile uint8_t *ecam_ptr(uint8_t bus, uint8_t dev, uint8_t fn, uint16_t off) {
    if (!g_ecam_base || bus < g_ecam_start_bus || bus > g_ecam_end_bus) return NULL;
    uint64_t a = g_ecam_base +
                 (((uint64_t)(bus - g_ecam_start_bus) << 20) |
                  ((uint64_t)dev << 15) | ((uint64_t)fn << 12) | off);
    return (volatile uint8_t *)(uintptr_t)a;
}

uint32_t fw_pci_read32(uint8_t bus, uint8_t dev, uint8_t fn, uint16_t off) {
    if (off > 0xFF) {
        volatile uint8_t *p = ecam_ptr(bus, dev, fn, off & ~3u);
        return p ? *(volatile uint32_t *)p : 0xFFFFFFFFu;
    }
    outl(CFG_ADDR, cf8(bus, dev, fn, (uint8_t)off));
    return inl(CFG_DATA);
}

uint16_t fw_pci_read16(uint8_t bus, uint8_t dev, uint8_t fn, uint16_t off) {
    return (uint16_t)(fw_pci_read32(bus, dev, fn, off) >> ((off & 2) * 8));
}

uint8_t fw_pci_read8(uint8_t bus, uint8_t dev, uint8_t fn, uint16_t off) {
    return (uint8_t)(fw_pci_read32(bus, dev, fn, off) >> ((off & 3) * 8));
}

void fw_pci_write32(uint8_t bus, uint8_t dev, uint8_t fn, uint16_t off, uint32_t v) {
    if (off > 0xFF) {
        volatile uint8_t *p = ecam_ptr(bus, dev, fn, off & ~3u);
        if (p) *(volatile uint32_t *)p = v;
        return;
    }
    outl(CFG_ADDR, cf8(bus, dev, fn, (uint8_t)off));
    outl(CFG_DATA, v);
}

void fw_pci_write16(uint8_t bus, uint8_t dev, uint8_t fn, uint16_t off, uint16_t v) {
    uint32_t cur = fw_pci_read32(bus, dev, fn, off & ~3u);
    int shift = (off & 2) * 8;
    cur = (cur & ~(0xFFFFu << shift)) | ((uint32_t)v << shift);
    fw_pci_write32(bus, dev, fn, off & ~3u, cur);
}

/* -- BAR decoding ---------------------------------------------------- */

static void read_bars(fw_pci_dev *d) {
    /*
     * Sizing writes all-ones into a BAR, which briefly makes the device
     * decode a bogus range. The spec requires IO+MEM decode to be OFF while
     * probing; leaving it on is also why some controllers (ICH9 AHCI) return
     * a zero size for their ABAR and then bind nothing.
     */
    uint16_t cmd = fw_pci_read16(d->bus, d->dev, d->fn, 0x04);
    fw_pci_write16(d->bus, d->dev, d->fn, 0x04, (uint16_t)(cmd & ~0x0003u));

    for (int i = 0; i < 6; i++) {
        uint16_t off = (uint16_t)(0x10 + i * 4);
        uint32_t lo = fw_pci_read32(d->bus, d->dev, d->fn, off);

        int is_io = lo & 1;
        int is_64 = !is_io && ((lo >> 1) & 3) == 2;

        /* Size probe: write all-ones, read back the writable mask. */
        fw_pci_write32(d->bus, d->dev, d->fn, off, 0xFFFFFFFFu);
        uint32_t mask = fw_pci_read32(d->bus, d->dev, d->fn, off);
        fw_pci_write32(d->bus, d->dev, d->fn, off, lo);

        uint64_t addr, size;
        if (is_io) {
            addr = lo & ~0x3ull;
            size = (~(mask & ~0x3u) + 1) & 0xFFFF;
        } else {
            addr = lo & ~0xFull;
            /* For a 32-bit BAR the inversion must stay 32-bit: doing
             * ~m + 1 in 64-bit width sign-extends the high half and yields
             * a nonsense multi-terabyte size, which then fails allocation
             * and silently drops the BAR (this is what hid every AHCI
             * ABAR). */
            uint64_t m = (uint64_t)(uint32_t)(mask & ~0xFu);
            if (!is_64) m |= 0xFFFFFFFF00000000ull;
            if (is_64) {
                uint32_t hi = fw_pci_read32(d->bus, d->dev, d->fn, (uint16_t)(off + 4));
                fw_pci_write32(d->bus, d->dev, d->fn, (uint16_t)(off + 4), 0xFFFFFFFFu);
                uint32_t hmask = fw_pci_read32(d->bus, d->dev, d->fn, (uint16_t)(off + 4));
                fw_pci_write32(d->bus, d->dev, d->fn, (uint16_t)(off + 4), hi);
                addr |= (uint64_t)hi << 32;
                m |= (uint64_t)hmask << 32;
            }
            size = m ? (~m + 1) : 0;
        }
        d->bar[i].addr   = addr;
        d->bar[i].size   = size;
        d->bar[i].is_io  = (uint8_t)is_io;
        d->bar[i].is_64  = (uint8_t)is_64;
        if (is_64) { i++; d->bar[i].addr = 0; d->bar[i].size = 0; }
    }

    fw_pci_write16(d->bus, d->dev, d->fn, 0x04, cmd);

}

static void probe_fn(uint8_t bus, uint8_t dev, uint8_t fn) {
    uint32_t id = fw_pci_read32(bus, dev, fn, 0);
    fw_printf("[pci] probe %d:%d.%d id=0x%lx\n", bus, dev, fn, (unsigned long)id);
    if ((id & 0xFFFF) == 0xFFFF) return;
    if (g_ndev >= FW_PCI_MAX_DEV) return;

    fw_pci_dev *d = &g_devs[g_ndev];
    fw_memset(d, 0, sizeof(*d));
    d->bus = bus; d->dev = dev; d->fn = fn;
    d->vendor_id = (uint16_t)(id & 0xFFFF);
    d->device_id = (uint16_t)(id >> 16);

    uint32_t cls = fw_pci_read32(bus, dev, fn, 0x08);
    d->revision  = (uint8_t)(cls & 0xFF);
    d->prog_if   = (uint8_t)((cls >> 8) & 0xFF);
    d->subclass  = (uint8_t)((cls >> 16) & 0xFF);
    d->class_code= (uint8_t)((cls >> 24) & 0xFF);
    d->header_type = fw_pci_read8(bus, dev, fn, 0x0E);

    if ((d->header_type & 0x7F) == 0) read_bars(d);

    d->irq_line = fw_pci_read8(bus, dev, fn, 0x3C);
    g_ndev++;

    /* Recurse behind bridges. */
    if ((d->header_type & 0x7F) == 1) {
        uint8_t sec = fw_pci_read8(bus, dev, fn, 0x19);
        if (sec && sec != bus) {
            for (uint8_t sd = 0; sd < 32; sd++) {
                probe_fn(sec, sd, 0);
                if (fw_pci_read8(sec, sd, 0, 0x0E) & 0x80)
                    for (uint8_t sf = 1; sf < 8; sf++) probe_fn(sec, sd, sf);
            }
        }
    }
}

int fw_pci_init(void) {
    g_ndev = 0;
    for (uint8_t dev = 0; dev < 32; dev++) {
        probe_fn(0, dev, 0);
        if (fw_pci_read8(0, dev, 0, 0x0E) & 0x80)
            for (uint8_t fn = 1; fn < 8; fn++) probe_fn(0, dev, fn);
    }
    return g_ndev;
}

int fw_pci_count(void) { return g_ndev; }

fw_pci_dev *fw_pci_get(int i) {
    if (i < 0 || i >= g_ndev) return NULL;
    return &g_devs[i];
}

fw_pci_dev *fw_pci_find_class(uint8_t cls, uint8_t sub, int8_t progif, int nth) {
    int seen = 0;
    for (int i = 0; i < g_ndev; i++) {
        fw_pci_dev *d = &g_devs[i];
        if (d->class_code != cls || d->subclass != sub) continue;
        if (progif >= 0 && d->prog_if != (uint8_t)progif) continue;
        if (seen++ == nth) return d;
    }
    return NULL;
}

fw_pci_dev *fw_pci_find_id(uint16_t vid, uint16_t did) {
    for (int i = 0; i < g_ndev; i++)
        if (g_devs[i].vendor_id == vid && g_devs[i].device_id == did) return &g_devs[i];
    return NULL;
}

void fw_pci_enable(fw_pci_dev *d, int bus_master) {
    if (!d) return;
    uint16_t cmd = fw_pci_read16(d->bus, d->dev, d->fn, 0x04);
    cmd |= 0x0002 | 0x0001;                       /* MEM + IO space */
    if (bus_master) cmd |= 0x0004;
    fw_pci_write16(d->bus, d->dev, d->fn, 0x04, cmd);
}

/* Walk the capability list looking for `cap_id`; returns config offset or 0. */
uint16_t fw_pci_find_cap(fw_pci_dev *d, uint8_t cap_id) {
    if (!d) return 0;
    uint16_t status = fw_pci_read16(d->bus, d->dev, d->fn, 0x06);
    if (!(status & 0x10)) return 0;               /* no capability list */
    uint8_t off = fw_pci_read8(d->bus, d->dev, d->fn, 0x34) & 0xFC;
    for (int guard = 0; off && guard < 48; guard++) {
        uint8_t id   = fw_pci_read8(d->bus, d->dev, d->fn, off);
        uint8_t next = fw_pci_read8(d->bus, d->dev, d->fn, (uint16_t)(off + 1)) & 0xFC;
        if (id == cap_id) return off;
        off = next;
    }
    return 0;
}

const char *fw_pci_class_name(uint8_t cls, uint8_t sub) {
    switch (cls) {
    case 0x01:
        switch (sub) {
        case 0x01: return "IDE controller";
        case 0x06: return "SATA/AHCI controller";
        case 0x08: return "NVMe controller";
        default:   return "mass storage";
        }
    case 0x02: return "network controller";
    case 0x03: return "display controller";
    case 0x04: return "multimedia";
    case 0x06: return sub == 0x04 ? "PCI-to-PCI bridge" : "bridge";
    case 0x0C:
        switch (sub) {
        case 0x03: return "USB controller";
        default:   return "serial bus";
        }
    default: return "device";
    }
}

void fw_pci_dump(void) {
    for (int i = 0; i < g_ndev; i++) {
        fw_pci_dev *d = &g_devs[i];
        fw_printf("[pci] %d:%d.%d %x:%x  %s (class %x.%x.%x)\n",
                  d->bus, d->dev, d->fn, d->vendor_id, d->device_id,
                  fw_pci_class_name(d->class_code, d->subclass),
                  d->class_code, d->subclass, d->prog_if);
    }
}
