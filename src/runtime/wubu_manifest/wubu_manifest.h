/* wubu_manifest.h -- public API for the unified WuBuOS syscall manifest.
 *
 * Adopted from GrahaOS etc/gcp.json: a SINGLE machine-readable source of
 * truth for the VSL dispatch table, the Styx9P op enum, and the HolyC FFI
 * stubs. The manifest is loaded once at boot; the VSL dispatcher resolves a
 * syscall number through it and enforces the required capability BEFORE
 * calling the handler (capability-only authority, replacing the old
 * uid/permission check).
 *
 * Opaque: callers see wubu_manifest_t* and query helpers. No god header. */
#ifndef WUBU_MANIFEST_H
#define WUBU_MANIFEST_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

typedef struct wubu_manifest wubu_manifest_t;

/* Load a manifest from a JSON file. Returns NULL on parse failure. */
wubu_manifest_t *wubu_manifest_load(const char *path);

/* Load from an in-memory JSON string (used by tests / embedded blobs). */
wubu_manifest_t *wubu_manifest_parse(const char *json, size_t len);

/* Get entry i (0..count-1): fills num/handler/right. Returns true on valid i. */
bool wubu_manifest_get(const wubu_manifest_t *m, int i,
                       uint64_t *out_num, const char **out_handler,
                       uint64_t *out_right, const char **out_cap);

void wubu_manifest_destroy(wubu_manifest_t *m);

/* Number of syscalls declared in the manifest. */
int  wubu_manifest_count(const wubu_manifest_t *m);

/* Resolve a syscall number to its handler name + required capability right
 * bit. Returns true and fills `out_handler` / `out_right` on success; false
 * if the number is unknown (=> -ENOSYS, no handler). */
bool wubu_manifest_resolve(const wubu_manifest_t *m, uint64_t num,
                            const char **out_handler, uint64_t *out_right);

/* Resolve and additionally gate on a capability: returns true only if the
 * caller's handle table holds a cap of the required kind covering
 * `out_right`. `cap_check` is supplied by the caller (the wubu_cap system);
 * we keep this module decoupled from wubu_cap via a function pointer. */
typedef bool (*wubu_manifest_cap_fn)(void *cap_ctx, uint64_t required_right);
bool wubu_manifest_resolve_gated(const wubu_manifest_t *m, uint64_t num,
                                  const char **out_handler, uint64_t *out_right,
                                  wubu_manifest_cap_fn cap_check, void *cap_ctx);

/* Emit generated C headers from the manifest (the GrahaOS gcp2wit.py step):
 *   - wubu_vsl_dispatch.h : VSL_SYS_* numbers + cap-gated dispatch decls
 *   - wubu_styx_ops.h     : Styx9P op enum
 *   - wubu_holyc_ffi.h    : HolyC FFI forward declarations
 * Returns 0 on success, -1 on write error. */
int wubu_manifest_emit(const wubu_manifest_t *m, const char *out_dir);

#endif /* WUBU_MANIFEST_H */
