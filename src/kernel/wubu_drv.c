/*
 * wubu_drv.c -- the DRIVER REGISTRY (the device model for the Steam
 * Deck + laptop hardware).
 *
 * The Linux-style binding: the buses (PCI, USB, ACPI, CPU) enumerate
 * devices; every driver carries an ID table; the registry matches and
 * PROBES — a device gets its driver, a driver gets its devices.
 *
 * The Steam Deck's bus topology (what this registry enumerates):
 *   PCI 0x1002/0x163F  -> the Van Gogh APU (display + audio)
 *   PCI NVMe class 0x01/0x08 -> the 2230 SSD
 *   PCI 0x14C3 (MT7921K)     -> the RZ616 Wi-Fi 6E
 *   PCI 0x1022/0x1457        -> the HDA audio controller
 *   ACPI _BIF/_BST           -> the battery
 *   ACPI thermal             -> the fan/thermal policy
 *
 * The registry:
 *   wubu_drv_init()          — the built-in driver table
 *   wubu_drv_pci_scan()      — enumerate the PCI bus (via wubu_pci)
 *   wubu_drv_probe()         — bind every device to its driver
 *   wubu_drv_find()          — a device by driver name
 *   wubu_drv_summary()       — the boot log line
 *
 * Each driver is an opaque struct with an ID table + probe + the
 * class ops (storage/net/audio/display/power). The tests inject a
 * FAKE PCI bus (a device table) so the binding logic is proven
 * without real hardware.
 *
 * C11.
 */
#include "wubu_drv.h"
#include "wubu_pci.h"

#include <stdio.h>
#include <string.h>

/* ---- the device table ---- */

#define WUBU_DRV_MAX_DEV  32
#define WUBU_DRV_MAX_DRV  12

static wubu_drv_dev_t    g_devs[WUBU_DRV_MAX_DEV];
static int               g_ndev;
static const wubu_drv_t *g_drivers[WUBU_DRV_MAX_DRV];
static int               g_ndrv;
static int               g_initialized;

/* DRV1: register a driver (the built-in table is populated at init). */
int wubu_drv_register(const wubu_drv_t *drv)
{
    if (!drv || g_ndrv >= WUBU_DRV_MAX_DRV) return -1;
    g_drivers[g_ndrv++] = drv;
    return 0;
}

/* DRV2: init — the built-in driver table. */
void wubu_drv_init(void)
{
    g_ndev = 0;
    g_ndrv = 0;
    g_initialized = 1;
    extern const wubu_drv_t wubu_drv_nvme;
    extern const wubu_drv_t wubu_drv_ahci;
    extern const wubu_drv_t wubu_drv_wifi;
    extern const wubu_drv_t wubu_drv_net;
    extern const wubu_drv_t wubu_drv_hda;
    extern const wubu_drv_t wubu_drv_gpu;
    extern const wubu_drv_t wubu_drv_battery;
    wubu_drv_register(&wubu_drv_nvme);
    wubu_drv_register(&wubu_drv_ahci);
    wubu_drv_register(&wubu_drv_wifi);
    wubu_drv_register(&wubu_drv_net);
    wubu_drv_register(&wubu_drv_hda);
    wubu_drv_register(&wubu_drv_gpu);
    wubu_drv_register(&wubu_drv_battery);
}

/* DRV3: add a device to the bus table. Returns the slot, -1 full. */
int wubu_drv_add_device(const wubu_drv_dev_t *dev)
{
    if (!dev || !g_initialized || g_ndev >= WUBU_DRV_MAX_DEV) return -1;
    g_devs[g_ndev] = *dev;
    return g_ndev++;
}

/* DRV4: does a driver's ID table match a device? */
static int drv_matches(const wubu_drv_t *drv, const wubu_drv_dev_t *dev)
{
    if (!drv->id_table || !drv->n_ids) return 0;
    for (int i = 0; i < drv->n_ids; i++) {
        const wubu_drv_id_t *id = &drv->id_table[i];
        if (id->vendor != dev->vendor && id->vendor != WUBU_DRV_ANY) continue;
        if (id->device != dev->device && id->device != WUBU_DRV_ANY) continue;
        if (id->class_code && id->class_code != dev->class_code) continue;
        if (id->subclass && id->subclass != dev->subclass) continue;
        return 1;
    }
    return 0;
}

/* DRV5: the PCI bus scan — enumerate the real bus (via wubu_pci) and
 * add every device to the table. Returns the device count. */
int wubu_drv_pci_scan(void)
{
    wubu_pci_dev_t pci[WUBU_DRV_MAX_DEV];
    int n = wubu_pci_scan(pci, WUBU_DRV_MAX_DEV);
    for (int i = 0; i < n && i < WUBU_DRV_MAX_DEV; i++) {
        wubu_drv_dev_t dev;
        memset(&dev, 0, sizeof(dev));
        dev.bus = WUBU_DRV_BUS_PCI;
        dev.vendor = pci[i].vendor;
        dev.device = pci[i].device;
        dev.class_code = pci[i].class_code;
        dev.subclass = pci[i].subclass;
        wubu_drv_add_device(&dev);
    }
    return g_ndev;
}

/* DRV6: probe — bind every device to its driver. Returns the number
 * of successful probes. */
int wubu_drv_probe(void)
{
    int probed = 0;
    for (int i = 0; i < g_ndev; i++) {
        if (g_devs[i].bound) continue;
        for (int d = 0; d < g_ndrv; d++) {
            if (!drv_matches(g_drivers[d], &g_devs[i])) continue;
            if (g_drivers[d]->probe) {
                g_devs[i].drv = g_drivers[d];
                g_devs[i].bound = 1;
                g_devs[i].probe_rc = g_drivers[d]->probe(&g_devs[i]);
                probed++;
            }
            break;
        }
    }
    return probed;
}

/* DRV7: find a bound device by driver name. */
const wubu_drv_dev_t *wubu_drv_find(const char *drv_name)
{
    for (int i = 0; i < g_ndev; i++) {
        if (g_devs[i].bound && g_devs[i].drv &&
            strcmp(g_devs[i].drv->name, drv_name) == 0)
            return &g_devs[i];
    }
    return NULL;
}

/* DRV8: the boot-log summary. */
int wubu_drv_summary(char *out, size_t cap)
{
    if (!out || cap == 0) return -1;
    size_t off = 0;
    int n = 0;
    for (int i = 0; i < g_ndev; i++) {
        const char *name = (g_devs[i].bound && g_devs[i].drv)
                           ? g_devs[i].drv->name : "unbound";
        int w = snprintf(out + off, cap - off,
                         "[drv] %04x:%04x class %02x/%02x -> %s\n",
                         g_devs[i].vendor, g_devs[i].device,
                         g_devs[i].class_code, g_devs[i].subclass, name);
        if (w < 0) break;
        off += (size_t)w;
        n++;
    }
    return n;
}

/* DRV9: the device count + probe count (for the tests). */
int wubu_drv_device_count(void) { return g_ndev; }
int wubu_drv_driver_count(void) { return g_ndrv; }
