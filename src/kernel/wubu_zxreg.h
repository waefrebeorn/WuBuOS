/*
 * wubu_zxreg.h  --  the module registry (ZXE-style loader bookkeeping).
 *
 * Sits on top of wubu_zx_run and gives a layer the classic ZealOS/.ZXE module
 * semantics the plan calls for:
 *   - LOAD BY NAME:   wubu_zxreg_load(ops, path, base, resolve) reads the PE
 *     from the /n namespace (via wubu_zxload's path resolution) and runs it
 *     through the module-runner primitive; the entry result comes back.
 *   - DEDUP:          loading the same path/bytes twice does NOT re-map a
 *     second copy — the module is registered once (by SHA-256 of its bytes)
 *     and the loader just bumps the refcount and returns the same handle.
 *   - REFCOUNT:       unload decrements; at zero the mapped image is released.
 *   - RESOLVE SEAM:   imports resolve against the registry (wubu_modsym) by
 *     default, i.e. the same export table the NT personality publishes into.
 *
 * This is the glue the session/boot runs to "load a module from the
 * namespace", deduping repeated loads (e.g. two apps importing a shared .ZXE)
 * and giving unload a real invariant instead of a second copy.
 *
 * The registry is a fixed, allocator-free table (kernel-clean). The mapped
 * image lifecycle is owned by wubu_zx_run's ops (host mmap / kernel allocator).
 */
#ifndef WUBU_ZXREG_H
#define WUBU_ZXREG_H

#include "wubu_zx.h"
#include <stdint.h>
#include <stddef.h>

#define WUBU_ZXREG_MAX 16

typedef struct {
    unsigned char sha[32];
    char path[96];
    uintptr_t base;
    int refcount;
    int valid;
    int last_result;   /* cached entry result (entry may only be run once) */
} wubu_zxreg_entry_t;

/* Run the module at `/n`<path> (or <path>), establishing a registry slot.
 * Returns the entry result on first load, a cache-ref (same as first load's
 * result) on dedup; returns WUBU_ZX_ERR_* (<0) on failure. `out` (optional)
 * receives a pointer to the registry slot. */
int wubu_zxreg_load(const wubu_zx_ops *ops, const char *path, uintptr_t base,
                    wubu_pe_resolve_fn resolve, wubu_zxreg_entry_t **out);

/* Drop a reference; frees the slot when the count reaches zero. */
void wubu_zxreg_unload(wubu_zxreg_entry_t *e);

/* row count / lookup helpers */
int  wubu_zxreg_count(void);
void wubu_zxreg_clear(void);
wubu_zxreg_entry_t *wubu_zxreg_entry(int idx);   /* idx 0..count-1 */

#endif /* WUBU_ZXREG_H */
