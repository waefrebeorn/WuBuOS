/*
 * wubu_pci.c -- minimal PCI config-space access (0xCF8/0xCFC).
 *
 * The metal kernel previously had no PCI access at all (the AHCI module is
 * a hosted simulation).  This gives the kernel real bus discovery: scan
 * bus 0 for every device/function and expose class/subclass + BARs so the
 * USB stack can find the xHCI controller and the console can list hardware.
 */
#include "wubu_pci.h"
#include "libc.h"
#include <stdint.h>

extern void *memset(void *s, int c, size_t n);   /* kernel libc */

#define PCI_CONFIG_ADDR 0xCF8
#define PCI_CONFIG_DATA 0xCFC

static inline void outl_(uint16_t port, uint32_t val) {
    __asm__ __volatile__("outl %0, %1" : : "a"(val), "Nd"(port));
}
static inline uint32_t inl_(uint16_t port) {
    uint32_t v;
    __asm__ __volatile__("inl %1, %0" : "=a"(v) : "Nd"(port));
    return v;
}

uint32_t wubu_pci_read32(uint8_t bus, uint8_t dev, uint8_t fn, uint8_t off)
{
    uint32_t addr = 0x80000000u | ((uint32_t)bus << 16) |
                    ((uint32_t)dev << 11) | ((uint32_t)fn << 8) |
                    ((uint32_t)off & 0xFCu);
    outl_(PCI_CONFIG_ADDR, addr);
    return inl_(PCI_CONFIG_DATA);
}

void wubu_pci_write32(uint8_t bus, uint8_t dev, uint8_t fn, uint8_t off,
                      uint32_t val)
{
    uint32_t addr = 0x80000000u | ((uint32_t)bus << 16) |
                    ((uint32_t)dev << 11) | ((uint32_t)fn << 8) |
                    ((uint32_t)off & 0xFCu);
    outl_(PCI_CONFIG_ADDR, addr);
    outl_(PCI_CONFIG_DATA, val);
}

/* Read BAR0/BAR1 as a 64-bit physical address (mask type bits). */
static uint64_t bar_of(wubu_pci_dev_t *d, int which)
{
    uint8_t off = (uint8_t)(0x10 + which * 4);
    uint32_t lo = wubu_pci_read32(d->bus, d->dev, d->fn, off);
    if (lo & 1) return lo & 0xFFFCu;                 /* IO space */
    uint32_t hi = wubu_pci_read32(d->bus, d->dev, d->fn, off + 4);
    uint64_t v = ((uint64_t)hi << 32) | (lo & 0xFFFFFFF0u);
    return v;
}

int wubu_pci_scan(wubu_pci_dev_t *out, int max)
{
    int n = 0;
    for (int dev = 0; dev < 32 && n < max; dev++) {
        uint32_t id = wubu_pci_read32(0, (uint8_t)dev, 0, 0);
        if (id == 0xFFFFFFFFu || id == 0) continue;      /* nothing there */
        for (int fn = 0; fn < 8 && n < max; fn++) {
            uint32_t id2 = wubu_pci_read32(0, (uint8_t)dev, (uint8_t)fn, 0);
            if (id2 == 0xFFFFFFFFu) continue;
            uint32_t cls = wubu_pci_read32(0, (uint8_t)dev, (uint8_t)fn, 0x08);
            wubu_pci_dev_t *d = &out[n];
            memset(d, 0, sizeof(*d));
            d->bus = 0; d->dev = (uint8_t)dev; d->fn = (uint8_t)fn;
            d->vendor  = (uint16_t)(id2 & 0xFFFFu);
            d->device  = (uint16_t)(id2 >> 16);
            d->class_code = (uint8_t)(cls >> 24);
            d->subclass   = (uint8_t)(cls >> 16);
            d->prog_if    = (uint8_t)(cls >> 8);
            d->bar0 = bar_of(d, 0);
            d->bar1 = bar_of(d, 1);
            n++;
            if (fn == 0 && !(wubu_pci_read32(0, (uint8_t)dev, 0, 0x0C) & 0x00800000u))
                break;                                  /* no multifunction */
        }
    }
    return n;
}

int wubu_pci_find_class(wubu_pci_dev_t *devs, int n,
                        uint8_t class_code, uint8_t subclass)
{
    for (int i = 0; i < n; i++)
        if (devs[i].class_code == class_code && devs[i].subclass == subclass)
            return i;
    return -1;
}
