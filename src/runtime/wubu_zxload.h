/*
 * wubu_zxload.h -- the /n NAMESPACE loader: run a PE read by name from the
 * namespace, through the module-runner primitive (wubu_zx_run).
 *
 *   wubu_zxload_run(ops, path, base, resolve, out_err)
 *   path: "/n/modules/hello.bin", "modules/hello.bin" or "/modules/hello.bin"
 *         (a leading root or "/n/" is normalized to g_ns_root).
 *   Everything else is wubu_zx_run's contract: returns the entry's result on
 *   success, a negative WUBU_ZX_ERR_* on failure (also stored in *out_err).
 *
 * This is the loader the boot/session/shell call to "run a module from the
 * namespace by name". Serves the P2 goal: modules drop into /n as PE bytes and
 * resolve their imports against the registry (default wubu_modsym_resolve).
 */
#ifndef WUBU_ZXLOAD_H
#define WUBU_ZXLOAD_H

#include "wubu_zx.h"

#ifdef __cplusplus
extern "C" {
#endif

int wubu_zxload_run(const wubu_zx_ops *ops, const char *path, uintptr_t base,
                    wubu_pe_resolve_fn resolve, int *out_err);

/* Read the whole file at `/n`<path> (or <path>) into a new buffer (*out, caller
 * frees); returns byte count or <0. */
int wubu_zxload_read(const char *path, uint8_t **_out, size_t *_sz);

#ifdef __cplusplus
}
#endif

#endif /* WUBU_ZXLOAD_H */
