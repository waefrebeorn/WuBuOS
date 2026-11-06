/*
 * wubu_zx.h  --  the module-runner primitive (ZXE-style loader core).
 *
 * wubu_zx_run runs a raw executable image (a PE the kernel loader understands)
 * through the full pipeline: parse -> walk sections -> map exec memory ->
 * copy sections -> relocate -> snap IAT via the symbol registry -> call the
 * entry. It is the ONE reusable call a higher layer (the /n namespace loader,
 * a .ZXE reader, the boot shell) needs in order to "run a module from bytes"
 * without re-implementing the loader each time.
 *
 * The IAT resolver (wubu_pe_resolve_fn) is the seam to the runtime symbol
 * registry (wubu_modsym_resolve) — which the NT personality publishes its
 * 296 handlers into via vsl_nt_export_modsym — so a loaded image's imports
 * resolve against the kernel's export table. Pass NULL to use the registry.
 *
 * Memory: the primitive does no allocation of its own. `ops.map(base,span)`
 * must return a span of RWX/zeroed memory at (or near) `base` and is supplied
 * by the CALLER: the kernel path uses its fixed allocator, the host test uses
 * mmap. `ops.unmap` (optional) balances it. This keeps the module kernel-clean
 * (no libc / no mmap dependency).
 *
 * Returns the entry's result (int, from eax) on success; on failure returns a
 * negative WUBU_ZX_ERR_* and (if out_err) stores the stage code. Caller owns
 * `bytes`.
 */
#ifndef WUBU_ZX_H
#define WUBU_ZX_H

#include "wubu_pe.h"
#include <stdint.h>
#include <stddef.h>

typedef void *(*wubu_zx_map_fn)(uintptr_t base, size_t span);
typedef void  (*wubu_zx_unmap_fn)(void *image, uintptr_t base, size_t span);

typedef struct {
    wubu_zx_map_fn   map;
    wubu_zx_unmap_fn unmap;   /* may be NULL */
} wubu_zx_ops;

/* stage error codes (negative, distinct from any user return) */
#define WUBU_ZX_ERR_PARAM    (-1)
#define WUBU_ZX_ERR_PARSE    (-2)
#define WUBU_ZX_ERR_SECTIONS (-3)
#define WUBU_ZX_ERR_MAP      (-4)
#define WUBU_ZX_ERR_RELOC    (-5)
#define WUBU_ZX_ERR_IAT      (-6)

/* Run `bytes` (a PE) at load base `base`. `resolve` NULL => wubu_modsym_resolve.
 * On success returns the entry result and sets *out_err=0; on failure returns a
 * WUBU_ZX_ERR_* (in *out_err too, if out_err). */
int wubu_zx_run(const wubu_zx_ops *ops, const uint8_t *bytes, size_t size,
                uintptr_t base, wubu_pe_resolve_fn resolve, int *out_err);

/* Convenience: run at base with ops.map set (unmap NULL). */
int wubu_zx_run_simple(wubu_zx_map_fn map,
                       const uint8_t *bytes, size_t size, uintptr_t base,
                       wubu_pe_resolve_fn resolve, int *out_err);

#endif /* WUBU_ZX_H */
