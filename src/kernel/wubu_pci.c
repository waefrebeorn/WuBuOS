/*
 * wubu_pci.c -- minimal PCI config-space access (0xCF8/0xCFC).
 *
 * The metal kernel previously had no PCI access at all (the AHCI module is
 * a hosted simulation).  This gives the kernel real bus discovery: scan
 * bus 0 for every device/function and expose class/subclass + BARs so the
 * USB stack can find the xHCI controller and the console can list hardware.
 */
#include "wubu_pci.h"
#include <stdint.h>

#ifdef WUBU_HOSTED
/* Hosted Linux leg: the AGI runs on real hardware under Linux (Steam Deck,
 * laptop).  Real port I/O (outl/inl) is privileged and cannot be used in a
 * userspace process, so we enumerate the PCI bus through the Linux sysfs
 * interface (sysfs devices under /sys/bus/pci/devices).  This is how
 * wubu_hw_detect() discovers the real GPU for the driver self-install
 * arm.  Includes the hosted libc (stdio/dirent) -- this branch never
 * links the kernel libc.
 * The test-stub build defines WUBU_PCI_TEST_STUBS to override config I/O;
 * skip the hosted headers there. */
#ifndef WUBU_PCI_TEST_STUBS
#include <stdio.h>
#include <dirent.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

/* read one small text attribute from a sysfs dir into buf; returns 0 ok */
static int sysfs_read_attr(const char *devdir, const char *attr,
                           char *buf, size_t cap) {
    char path[256];
    snprintf(path, sizeof(path), "%s/%s", devdir, attr);
    FILE *f = fopen(path, "r");
    if (!f) return -1;
    if (!fgets(buf, (int)cap, f)) { fclose(f); return -1; }
    fclose(f);
    return 0;
}
#endif
#else
#include "libc.h"
extern void *memset(void *s, int c, size_t n);   /* kernel libc */
#endif

#define PCI_CONFIG_ADDR 0xCF8
#define PCI_CONFIG_DATA 0xCFC

/* Config-space I/O.  Provider selected by backend, independent of the
 * scan backend so that a hosted build (which pulls in <stdio.h>) can
 * still link the test-stub fake.
 *  - WUBU_PCI_TEST_STUBS: the selftest injects a fake config space.
 *  - metal: real privileged port I/O to 0xCF8/0xCFC.
 * The read32/write32 API always exists so callers (wubu_hw_detect,
 * bar_of) are backend-agnostic. */
#if defined(WUBU_PCI_TEST_STUBS)
uint32_t wubu_pci_read_config(uint8_t bus, uint8_t dev, uint8_t fn, uint8_t off);
void     wubu_pci_write_config(uint8_t bus, uint8_t dev, uint8_t fn, uint8_t off, uint32_t val);
#elif !defined(WUBU_HOSTED)
static inline void outl_(uint16_t port, uint32_t val) {
    __asm__ __volatile__("outl %0, %1" : : "a"(val), "Nd"(port));
}
static inline uint32_t inl_(uint16_t port) {
    uint32_t v;
    __asm__ __volatile__("inl %1, %0" : "=a"(v) : "Nd"(port));
    return v;
}
static inline uint32_t wubu_pci_read_config(uint8_t bus, uint8_t dev, uint8_t fn, uint8_t off) {
    uint32_t addr = 0x80000000u | ((uint32_t)bus << 16) |
                    ((uint32_t)dev << 11) | ((uint32_t)fn << 8) |
                    ((uint32_t)off & 0xFCu);
    outl_(PCI_CONFIG_ADDR, addr);
    return inl_(PCI_CONFIG_DATA);
}
static inline void wubu_pci_write_config(uint8_t bus, uint8_t dev, uint8_t fn, uint8_t off, uint32_t val) {
    uint32_t addr = 0x80000000u | ((uint32_t)bus << 16) |
                    ((uint32_t)dev << 11) | ((uint32_t)fn << 8) |
                    ((uint32_t)off & 0xFCu);
    outl_(PCI_CONFIG_ADDR, addr);
    outl_(PCI_CONFIG_DATA, val);
}
#endif

/* read32/write32/bar_of/metal-scan are only meaningful with real config
 * I/O (metal or test stubs); the hosted sysfs scan implements wubu_pci_scan
 * directly below. */
#if !defined(WUBU_HOSTED) || defined(WUBU_PCI_TEST_STUBS)
/* --- metal / test-stub backend: real config I/O + bus scan --- */
uint32_t wubu_pci_read32(uint8_t bus, uint8_t dev, uint8_t fn, uint8_t off)
{
    return wubu_pci_read_config(bus, dev, fn, off);
}

void wubu_pci_write32(uint8_t bus, uint8_t dev, uint8_t fn, uint8_t off,
                      uint32_t val)
{
    wubu_pci_write_config(bus, dev, fn, off, val);
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

#elif defined(WUBU_HOSTED) && !defined(WUBU_PCI_TEST_STUBS)
/* --- Linux-hosted backend: enumerate /sys/bus/pci, no port I/O --- */
/* Parse "0x1002" or "0x1002\n" -> 0x1002 */
static uint32_t hexval(const char *s)
{
    unsigned long v = strtoul(s, NULL, 0);
    return (uint32_t)v;
}

/* Parse a sysfs resource file line like
 * 00000000f0000000 00000000f0ffffff 02040000 ... */
static uint64_t resource_bar(const char *resline)
{
    unsigned long lo = strtoul(resline, NULL, 16);
    return (uint64_t)lo;
}

int wubu_pci_scan(wubu_pci_dev_t *out, int max)
{
    DIR *d = opendir("/sys/bus/pci/devices");
    if (!d) return 0;
    struct dirent *de;
    int n = 0;
    while ((de = readdir(d)) && n < max) {
        /* sysfs names are "0000:00:00.0" (domain:bus:dev.fn) */
        const char *p = strchr(de->d_name, ':');
        if (!p || !p[1]) continue;
        p++;                                   /* -> bus */
        int bus  = (int)strtoul(p, NULL, 16);
        p = strchr(p, ':'); if (!p) continue;  /* -> dev.fn */
        p++;
        /* dev.fn : everything before '.' */
        char *dot = strchr(p, '.'); if (!dot) continue;
        *dot = '\0';
        int dev = (int)strtoul(p, NULL, 16);
        int fn  = (int)strtoul(dot + 1, NULL, 16);
        *dot = '.';

        char vpath[256], dpath[256], cpath[256];
        snprintf(vpath, sizeof(vpath), "/sys/bus/pci/devices/%s/vendor", de->d_name);
        snprintf(dpath, sizeof(dpath), "/sys/bus/pci/devices/%s/device", de->d_name);
        snprintf(cpath, sizeof(cpath), "/sys/bus/pci/devices/%s/class", de->d_name);
        FILE *fv = fopen(vpath, "r");
        FILE *fd = fopen(dpath, "r");
        FILE *fc = fopen(cpath, "r");
        if (!fv || !fd || !fc) { if (fv) fclose(fv); if (fd) fclose(fd); if (fc) fclose(fc); continue; }

        char buf[64];
        uint32_t vendor=0xffffffff, device=0xffffffff, cls=0;
        if (fgets(buf, sizeof(buf), fv)) vendor = hexval(buf);
        if (fgets(buf, sizeof(buf), fd)) device = hexval(buf);
        if (fgets(buf, sizeof(buf), fc)) cls = hexval(buf);
        fclose(fv); fclose(fd); fclose(fc);
        if (vendor == 0xffffffff || device == 0xffffffff) continue;

        wubu_pci_dev_t *dp = &out[n];
        memset(dp, 0, sizeof(*dp));
        dp->bus = (uint8_t)bus;
        dp->dev = (uint8_t)dev;
        dp->fn  = (uint8_t)fn;
        dp->vendor     = (uint16_t)(vendor & 0xFFFFu);
        dp->device     = (uint16_t)(device & 0xFFFFu);
        dp->class_code = (uint8_t)(cls >> 16);
        dp->subclass   = (uint8_t)(cls >> 8);
        dp->prog_if    = (uint8_t)(cls);

        /* BAR0 (MMIO) from /sys/.../resource line 0 */
        char rpath[256];
        snprintf(rpath, sizeof(rpath), "/sys/bus/pci/devices/%s/resource", de->d_name);
        FILE *fr = fopen(rpath, "r");
        if (fr) {
            if (fgets(buf, sizeof(buf), fr)) dp->bar0 = resource_bar(buf);
            fclose(fr);
        }
        n++;
    }
    closedir(d);
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

/* Stub read32/write32 for hosted tests that need the symbol but run
 * purely on the sysfs-scan path (no real config I/O). Drivers like
 * wubu_xhci call this to read BARs; on hosted we already captured
 * bar0/bar1 during scan, so we return 0 here — the test never
 * reaches hardware. */
uint32_t wubu_pci_read32(uint8_t bus, uint8_t dev, uint8_t fn, uint8_t off)
{
    (void)bus; (void)dev; (void)fn; (void)off;
    return 0;
}

void wubu_pci_write32(uint8_t bus, uint8_t dev, uint8_t fn, uint8_t off,
                      uint32_t val)
{
    (void)bus; (void)dev; (void)fn; (void)off; (void)val;
}
#endif
