/*
 * fw_pcires.c  --  PCI resource (BAR) assignment.
 *
 * On a real boot there is nobody to assign BARs: QEMU (like real chipsets)
 * powers up with every BAR at zero and expects the firmware to program the
 * MMIO/IO windows itself. Without this pass every driver sees bar.addr == 0
 * and binds nothing, which is exactly what happened before this module
 * existed.
 *
 * Strategy: two bump allocators (32-bit MMIO below 4GB in the PCI hole, and
 * port IO), assigning largest-first so natural-alignment requirements are
 * satisfied without gaps. 64-bit BARs are placed in the same 32-bit window
 * when they fit, which keeps them reachable from our identity map.
 */

#include "fw.h"
#include "fw_pci.h"

/* The classic PCI hole below 4GB: above the framebuffer aperture QEMU uses
 * and below the LAPIC/HPET/flash region at 0xFEC00000. */
#define MMIO_BASE 0xC0000000ULL
#define MMIO_LIMIT 0xFEBFFFFFULL
#define IO_BASE   0xC000
#define IO_LIMIT  0xFFFF

static uint64_t g_mmio_next = MMIO_BASE;
static uint32_t g_io_next   = IO_BASE;

static uint64_t alloc_mmio(uint64_t size) {
    if (!size) return 0;
    uint64_t a = (g_mmio_next + size - 1) & ~(size - 1);   /* natural align */
    if (a + size > MMIO_LIMIT) return 0;
    g_mmio_next = a + size;
    return a;
}

static uint32_t alloc_io(uint32_t size) {
    if (!size) return 0;
    uint32_t a = (g_io_next + size - 1) & ~(size - 1);
    if (a + size > IO_LIMIT) return 0;
    g_io_next = a + size;
    return a;
}

static void assign_dev(fw_pci_dev *d) {
    if ((d->header_type & 0x7F) != 0) return;      /* bridges: not our job */

    for (int i = 0; i < 6; i++) {
        uint64_t size = d->bar[i].size;
        if (!size) continue;
        uint16_t off = (uint16_t)(0x10 + i * 4);

        if (d->bar[i].is_io) {
            uint32_t a = alloc_io((uint32_t)size);
            if (!a) continue;
            fw_pci_write32(d->bus, d->dev, d->fn, off, a | 1);
            d->bar[i].addr = a;
        } else {
            uint64_t a = alloc_mmio(size);
            if (!a) continue;
            uint32_t lo = fw_pci_read32(d->bus, d->dev, d->fn, off);
            fw_pci_write32(d->bus, d->dev, d->fn, off,
                           (uint32_t)a | (lo & 0xF));
            if (d->bar[i].is_64)
                fw_pci_write32(d->bus, d->dev, d->fn, (uint16_t)(off + 4),
                               (uint32_t)(a >> 32));
            d->bar[i].addr = a;
        }
        fw_printf("[pcires] %d:%d.%d BAR%d %s 0x%lx size 0x%lx\n",
                  d->bus, d->dev, d->fn, i,
                  d->bar[i].is_io ? "io " : "mem", d->bar[i].addr, size);
        if (d->bar[i].is_64) i++;
    }
    fw_pci_enable(d, 1);
}

/*
 * Assign in descending size order across all devices so that a large BAR
 * never has to squeeze past small ones and fail its alignment.
 */
void fw_pci_assign_resources(void) {
    int n = fw_pci_count();

    for (;;) {
        uint64_t best = 0;
        fw_pci_dev *bd = NULL;
        int bi = -1;

        for (int i = 0; i < n; i++) {
            fw_pci_dev *d = fw_pci_get(i);
            if (!d || (d->header_type & 0x7F) != 0) continue;
            for (int b = 0; b < 6; b++) {
                if (!d->bar[b].size || d->bar[b].addr) continue;
                if (d->bar[b].size > best) { best = d->bar[b].size; bd = d; bi = b; }
            }
        }
        if (!bd) break;

        uint16_t off = (uint16_t)(0x10 + bi * 4);
        if (bd->bar[bi].is_io) {
            uint32_t a = alloc_io((uint32_t)best);
            if (!a) { bd->bar[bi].size = 0; continue; }
            fw_pci_write32(bd->bus, bd->dev, bd->fn, off, a | 1);
            bd->bar[bi].addr = a;
        } else {
            uint64_t a = alloc_mmio(best);
            if (!a) { bd->bar[bi].size = 0; continue; }
            uint32_t lo = fw_pci_read32(bd->bus, bd->dev, bd->fn, off);
            fw_pci_write32(bd->bus, bd->dev, bd->fn, off, (uint32_t)a | (lo & 0xF));
            if (bd->bar[bi].is_64)
                fw_pci_write32(bd->bus, bd->dev, bd->fn, (uint16_t)(off + 4),
                               (uint32_t)(a >> 32));
            bd->bar[bi].addr = a;
        }
        fw_printf("[pcires] %d:%d.%d BAR%d %s 0x%lx size 0x%lx\n",
                  bd->bus, bd->dev, bd->fn, bi,
                  bd->bar[bi].is_io ? "io " : "mem", bd->bar[bi].addr, best);
    }

    for (int i = 0; i < n; i++) {
        fw_pci_dev *d = fw_pci_get(i);
        if (d && (d->header_type & 0x7F) == 0) fw_pci_enable(d, 1);
    }
    fw_printf("[pcires] MMIO used to 0x%lx, IO to 0x%x\n", g_mmio_next, g_io_next);
}

/* Kept for callers that want to (re)program a single device. */
void fw_pci_assign_one(fw_pci_dev *d) { if (d) assign_dev(d); }
