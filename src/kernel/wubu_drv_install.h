/*
 * wubu_drv_install.h -- the DRIVER SELF-INSTALLATION arm.
 *
 * WuBuOS doctrine: "we run everything and run on everything." The device
 * model (wubu_drv.c) DISCOVERS hardware and BINDS known drivers, but a
 * device with no registered driver was left "unbound" with no arm to
 * install one. This module is that arm: the AGI operating system must
 * NUMERATE its own drivers and INSTALL ITSELF — fetch / build / load a
 * driver module for a device it does not yet know.
 *
 * Pipeline (numerate -> decide -> acquire -> build -> load -> re-probe):
 *   1. MODALIAS   wubu_drv_modalias()      -- pci:vXXXXdXXXX / usb:vXXXXpXXXX
 *   2. DECIDE     wubu_drv_manifest_lookup()-- is there a manifest entry?
 *   3. ACQUIRE    wubu_drv_acquire()       -- local tree / pkg / git ref
 *   4. BUILD      wubu_drv_build()         -- gcc -c to an ET_REL .o
 *   5. LOAD       wubu_drv_elf_load()      -- OUR OWN ELF relocatable loader
 *   6. RE-PROBE   wubu_drv_register()      -- binds the device live
 *
 * The loader (step 5) is the "own entire system" crown piece: we read a
 * normal gcc-produced ELF ET_REL object, copy its PROGBITS sections,
 * resolve R_X86_64 relocations against the kernel export table, and call
 * the module's entry point -- a real in-memory driver install, no host
 * modprobe/apt/kext.
 *
 * C11, freestanding-safe (no libc dependencies in the loader).
 */
#ifndef WUBU_DRV_INSTALL_H
#define WUBU_DRV_INSTALL_H

#include <stddef.h>
#include <stdint.h>

#include "wubu_drv.h"

/* outcome of a self-install attempt */
enum {
    WUBU_DI_OK          = 0,  /* driver present & bound */
    WUBU_DI_NO_MANIFEST = 1,  /* no manifest entry for this modalias */
    WUBU_DI_NO_SOURCE   = 2,  /* manifest exists but source unavailable */
    WUBU_DI_BUILD_FAIL  = 3,  /* build failed */
    WUBU_DI_LOAD_FAIL   = 4,  /* module load / relocation failed */
    WUBU_DI_NOMEM       = 5,  /* out of memory */
    WUBU_DI_BUSY        = 6,  /* already installed */
};

/* DI1: synthesize the Linux-style modalias for a discovered device.
 *      pci:v000010DEd00002504sv... / usb:vXXXXpXXXXdXXXXdcXXdscXXdpXX.
 * Returns the length written (excluding NUL), or -1. */
int wubu_drv_modalias(const wubu_drv_dev_t *dev, char *out, size_t cap);

/* DI2: is this device ALREADY bound? (the numerate-first check) */
int wubu_drv_is_bound(const wubu_drv_dev_t *dev);

/* DI3: a manifest entry — the AGI's mutable knowledge mapping a device
 * (by modalias family) to where its driver lives. */
typedef struct {
    const char *modalias_prefix; /* e.g. "pci:v000010DE" (NVIDIA) */
    const char *driver_name;     /* e.g. "nv_gpu" */
    const char *source;          /* "local" | "pkg" | "git" | "fw_container" */
    const char *path;            /* local src tree / pkg name / git ref */
} wubu_drv_manifest_t;

/* DI4: look up the manifest for a modalias. Returns the entry or NULL. */
const wubu_drv_manifest_t *wubu_drv_manifest_lookup(const char *modalias);

/* DI5: the export table symbol lookup used by the module loader. A loaded
 * driver module can call back into the kernel via these (e.g.
 * wubu_drv_register). Returns the function address or NULL. */
const void *wubu_drv_export_lookup(const char *sym);

/* DI3 extended: wubu_drv_acquire() -- fetch source for git/pkg sources.
 *   For git sources: clone into ~/opt/wubu_drivers/<driver_name>/
 *   For fw_container: map firmware to driver interface.
 *   Returns 0 on success, WUBU_DI_NO_SOURCE on failure. */
int wubu_drv_acquire(const wubu_drv_manifest_t *m);

/* DI6: load a compiled ET_REL .o in memory. `obj`/`obj_len` is the object
 * bytes. Reads ELF64 headers, copies PROGBITS, relocates against the
 * export table, calls the `wubu_mod_entry` symbol, which returns a
 * wubu_drv_t* that is registered. Returns 0 on success. */
int wubu_drv_elf_load(const void *obj, size_t obj_len);

/* DI7: the full self-install pipeline for one unbound device. Returns the
 * WUBU_DI_* outcome. */
int wubu_drv_install(const wubu_drv_dev_t *dev);

/* DI8: build step -- compile a driver source tree to an ET_REL .o in
 * memory (hosted leg shells to cc; metal leg uses the in-kernel HC
 * compiler). Returns a malloc'd object + length, or NULL. */
void *wubu_drv_build(const wubu_drv_manifest_t *m, size_t *obj_len);

/* DI9: report unbound devices with their modalias + manifest status, so
 * the AGI/user can author new manifest entries offline. Returns lines
 * written. */
int wubu_drv_install_report(char *out, size_t cap);

#endif /* WUBU_DRV_INSTALL_H */