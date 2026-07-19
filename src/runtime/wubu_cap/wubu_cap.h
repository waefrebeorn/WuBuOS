/*
 * wubu_cap.h -- WuBuOS Capability Core (ported from GrahaOS kernel/cap).
 *
 * This is the AGI-friendly authority substrate WuBuOS was missing: no ambient
 * rights, no uid/root. Every resource (file, channel, container, snapshot,
 * transaction, system op) is a cap_object_t addressed by an opaque 64-bit
 * cap_token_t that packs {generation, idx, flags}. A stale or revoked token
 * fails resolution because its generation no longer matches.
 *
 * Ported mechanically from GrahaOS (C11, freestanding -> host portable):
 *   - kmem_cache/kmalloc/kfree  -> malloc/free
 *   - spinlock_t                -> host pthread once-lock shim (wubu_cap_lock.h)
 *   - kpanic/klog/audit         -> dropped (no host kernel to log to)
 * The object layout, token packing, generation-counted revoke, audience set,
 * rights bitmap, and derive/revoke semantics are preserved verbatim.
 *
 * Opaque API. No god headers. Self-contained C11 module.
 */
#ifndef WUBU_CAP_H
#define WUBU_CAP_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* ---- Capability kinds (what the object protects) ---- */
#define WUBU_CAP_KIND_NONE          0
#define WUBU_CAP_KIND_FILE          1
#define WUBU_CAP_KIND_PROC          2
#define WUBU_CAP_KIND_CHANNEL       3
#define WUBU_CAP_KIND_VMO           4
#define WUBU_CAP_KIND_SNAPSHOT      5
#define WUBU_CAP_KIND_WASM_INSTANCE 6
#define WUBU_CAP_KIND_STREAM        7
#define WUBU_CAP_KIND_TRANSACTION   8
#define WUBU_CAP_KIND_SYSTEM        9
#define WUBU_CAP_KIND_CONSOLE       10
#define WUBU_CAP_KIND_CONTAINER     11   /* WuBuOS: a sandbox/container handle */
#define WUBU_CAP_KIND_STYX_NODE     12   /* WuBuOS: a 9P namespace node */

/* ---- Rights bitmap (subset of the underlying object's rights) ---- */
#define WUBU_RIGHT_READ     0x0000000000000001ULL
#define WUBU_RIGHT_WRITE    0x0000000000000002ULL
#define WUBU_RIGHT_EXEC     0x0000000000000004ULL
#define WUBU_RIGHT_DERIVE   0x0000000000000008ULL
#define WUBU_RIGHT_REVOKE   0x0000000000000010ULL
#define WUBU_RIGHT_INSPECT   0x0000000000000020ULL
#define WUBU_RIGHT_GRANT     0x0000000000000040ULL
#define WUBU_RIGHT_SEND     0x0000000000000080ULL
#define WUBU_RIGHT_RECV     0x0000000000000100ULL
#define WUBU_RIGHT_RESTORE  0x0000000000000200ULL
#define WUBU_RIGHT_DELETE   0x0000000000000400ULL
#define WUBU_RIGHT_COMMIT   0x0000000000000800ULL
#define WUBU_RIGHT_ABORT    0x0000000000001000ULL
#define WUBU_RIGHT_TERMINATE 0x0000000000002000ULL
#define WUBU_RIGHT_INVOKE   0x0000000000004000ULL
#define WUBU_RIGHT_ATTACH   0x0000000000008000ULL
#define WUBU_RIGHT_OBSERVE  0x0000000000010000ULL
#define WUBU_RIGHTS_ALL     0xFFFFFFFFFFFFFFFFULL

/* ---- Flags ---- */
#define WUBU_CAP_FLAG_PUBLIC           0x01
#define WUBU_CAP_FLAG_EAGER_REVOKE     0x02
#define WUBU_CAP_FLAG_IMMORTAL         0x08
#define WUBU_CAP_FLAG_INHERITABLE      0x10
#define WUBU_CAP_FLAG_RECURSIVE_INHERIT 0x40
#define WUBU_CAP_FLAG_CASCADE_TRUNCATED 0x20

/* ---- Pid sentinels ---- */
#define WUBU_PID_PUBLIC  (0xFFFF)
#define WUBU_PID_KERNEL  (-1)
#define WUBU_PID_NONE    (-1)

/* ---- Error codes (match -errno convention) ---- */
#define WUBU_CAP_OK        0
#define WUBU_CAP_EPERM    -1
#define WUBU_CAP_EREVOKED -2
#define WUBU_CAP_ENOMEM   -3
#define WUBU_CAP_EFAULT   -4
#define WUBU_CAP_EINVAL   -5
#define WUBU_CAP_ENOSYS   -6
#define WUBU_CAP_EBADF    -9
#define WUBU_CAP_EAGAIN   -11
#define WUBU_CAP_EBUSY    -16
#define WUBU_CAP_EFROZEN  -128

/* ---- Registry limits ---- */
#define WUBU_CAP_OBJECT_CAPACITY  65536u
#define WUBU_CAP_AUDIENCE_MAX     8
#define WUBU_CAP_IDX_NULL         0u
#define WUBU_CAP_IDX_NONE         0xFFFFFFFFu

/* ---- Opaque token (passed by value, never dereferenced by holder) ---- */
typedef struct wubu_cap_token { uint64_t raw; } wubu_cap_token_t;
#define WUBU_CAP_TOKEN_NULL ((wubu_cap_token_t){.raw = 0})

/* ---- Inspect result (filled by wubu_cap_inspect) ---- */
typedef struct wubu_cap_inspect {
    uint16_t  kind;
    uint8_t   flags;
    uint8_t   reserved1;
    uint32_t  generation;
    uint64_t  rights;
    int32_t   audience[WUBU_CAP_AUDIENCE_MAX];
    int32_t   owner_pid;
    uint8_t   reserved2[4];
} wubu_cap_inspect_t;

/* ---- Public object type (opaque to holders; full struct in _internal.h) ---- */
typedef struct wubu_cap_object wubu_cap_object_t;

/* ---- Lifecycle ---- */
void    wubu_cap_init(void);
int     wubu_cap_create(uint16_t kind, uint64_t rights,
                        const int32_t *audience, uint8_t flags,
                        uintptr_t kind_data, int32_t owner_pid,
                        uint32_t parent_idx, wubu_cap_token_t *out);
int     wubu_cap_derive(wubu_cap_token_t parent, int32_t caller_pid,
                        uint64_t rights_subset, const int32_t *audience_subset,
                        uint8_t flags_subset, wubu_cap_token_t *out);
int     wubu_cap_revoke(wubu_cap_token_t tok);
void    wubu_cap_destroy(uint32_t idx);
int     wubu_cap_inspect(wubu_cap_token_t tok, int32_t caller_pid,
                         wubu_cap_inspect_t *out);

/* ---- Hot-path resolve: returns object* on success, NULL on any failure.
 *      Caller must hold the required rights. Returns NULL if not in audience,
 *      revoked, or rights missing. ---- */
wubu_cap_object_t *wubu_cap_resolve(int32_t calling_pid, wubu_cap_token_t tok,
                                   uint64_t required_rights);

/* ---- Token helpers ---- */
static inline wubu_cap_token_t wubu_cap_token_pack(uint32_t gen, uint32_t idx,
                                                   uint8_t flags) {
    wubu_cap_token_t t;
    t.raw = ((uint64_t)gen << 32) | (((uint64_t)idx & 0xFFFFFFULL) << 8) | flags;
    return t;
}
static inline uint32_t wubu_cap_token_gen(wubu_cap_token_t t)  { return (uint32_t)(t.raw >> 32); }
static inline uint32_t wubu_cap_token_idx(wubu_cap_token_t t)  { return (uint32_t)((t.raw >> 8) & 0xFFFFFFULL); }
static inline uint8_t  wubu_cap_token_flags(wubu_cap_token_t t) { return (uint8_t)(t.raw & 0xFFu); }
static inline bool     wubu_cap_token_is_null(wubu_cap_token_t t) { return wubu_cap_token_idx(t) == 0; }

/* ---- Per-process handle table (maps a slot to an object idx) ---- */
typedef struct wubu_cap_handle_table wubu_cap_handle_table_t;

wubu_cap_handle_table_t *wubu_cap_handle_table_create(void);
void     wubu_cap_handle_table_free(wubu_cap_handle_table_t *t);
/* Insert object_idx; returns a token (idx=slot, gen=local gen) on success,
 * NULL token on failure. */
wubu_cap_token_t wubu_cap_handle_insert(wubu_cap_handle_table_t *t,
                                       uint32_t object_idx, uint8_t token_flags);
/* Resolve a handle token to its object, checking rights. */
wubu_cap_object_t *wubu_cap_handle_resolve(wubu_cap_handle_table_t *t,
                                          int32_t caller_pid,
                                          wubu_cap_token_t tok,
                                          uint64_t required_rights);
/* Close a handle: bumps local gen so stale tokens fail. */
int      wubu_cap_handle_close(wubu_cap_handle_table_t *t, wubu_cap_token_t tok);
/* Scan the table for any live cap of `kind` covering `required_rights`.
 * Returns WUBU_CAP_OK on match, WUBU_CAP_EPERM otherwise. Used for
 * capability-kind gates (e.g. CAP_KIND_SYSTEM) where the table is authority. */
int      wubu_cap_handle_resolve_kind(wubu_cap_handle_table_t *t, int32_t pid,
                                     uint16_t kind, uint64_t required_rights);

/* ---- Bootcap cascade root (GrahaOS CAP_KIND_SYSTEM) ---- */
void     wubu_cap_system_init(void);
uint32_t wubu_cap_system_bootcap_idx(void);
int      wubu_cap_system_install_to_pid(wubu_cap_handle_table_t *t, int32_t pid,
                                     uint64_t rights_subset);
int      wubu_cap_system_resolve(wubu_cap_handle_table_t *t, int32_t pid,
                                 uint64_t required_rights);

/* ---- Pretty-print for diagnostics ---- */
int wubu_cap_token_describe(wubu_cap_token_t tok, char *buf, int buflen);

#endif /* WUBU_CAP_H */
