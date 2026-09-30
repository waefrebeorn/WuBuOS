/*
 * vsl_nt_cap.c -- NT Object Manager authority bridge (backed by wubu_cap).
 *
 * Real implementation of the AGI authority substrate the NT kernel-level work
 * requires. Maps NT object types to capability kinds and NT access masks to
 * wubu_cap rights, then backs NT handles on the cap handle table. See the
 * design notes in vsl_nt_cap.h.
 */
#include "vsl_nt_cap.h"
#include "vsl_nt_bridge.h"
#include "vsl_nt_internal.h"   /* nt_token_entry_t, token resolver */
#include "wubu_cap/wubu_cap.h"

#include <stdlib.h>
#include <string.h>

/* ---- NT object type -> wubu_cap kind mapping ----
 * NT's typed object universe (File, Key, Section, ...) collapses onto the cap
 * kind lattice. The mapping is total and stable so NtOpen/NtCreate handlers
 * can route any object type through the same authority check. */
uint16_t vsl_nt_kind_to_cap(nt_object_type_t nt_type) {
    switch (nt_type) {
    case NT_OBJECT_TYPE_FILE:
    case NT_OBJECT_TYPE_DIRECTORY:
    case NT_OBJECT_TYPE_SYMBOLIC_LINK:
    case NT_OBJECT_TYPE_KEY:
        return WUBU_CAP_KIND_STYX_NODE; /* namespace nodes */
    case NT_OBJECT_TYPE_PROCESS:
    case NT_OBJECT_TYPE_THREAD:
        return WUBU_CAP_KIND_PROC;
    case NT_OBJECT_TYPE_SECTION:
    case NT_OBJECT_TYPE_DEBUG_OBJECT:
        return WUBU_CAP_KIND_VMO;
    case NT_OBJECT_TYPE_EVENT:
    case NT_OBJECT_TYPE_MUTANT:
    case NT_OBJECT_TYPE_SEMAPHORE:
    case NT_OBJECT_TYPE_TIMER:
    case NT_OBJECT_TYPE_KEYED_EVENT:
    case NT_OBJECT_TYPE_EVENT_PAIR:
    case NT_OBJECT_TYPE_IO_COMPLETION:
        return WUBU_CAP_KIND_STREAM; /* sync primitives as capability streams */
    case NT_OBJECT_TYPE_JOB:
    case NT_OBJECT_TYPE_TRANSACTION:
    case NT_OBJECT_TYPE_TRANSACTION_MANAGER:
    case NT_OBJECT_TYPE_RESOURCE_MANAGER:
    case NT_OBJECT_TYPE_ENLISTMENT:
    case NT_OBJECT_TYPE_TM:
    case NT_OBJECT_TYPE_WORK_ITEM:
    case NT_OBJECT_TYPE_PROFILE:
        return WUBU_CAP_KIND_TRANSACTION;
    case NT_OBJECT_TYPE_PORT:
    case NT_OBJECT_TYPE_WAITABLE_PORT:
        return WUBU_CAP_KIND_CHANNEL;
    case NT_OBJECT_TYPE_TOKEN:
        return WUBU_CAP_KIND_SYSTEM; /* authority over identity */
    default:
        return WUBU_CAP_KIND_STYX_NODE;
    }
}

/* ---- NT access mask -> wubu_cap rights ----
 * NT packs authority into a 32-bit desired_access with GENERIC_*, STANDARD_RIGHTS
 * and object-type-specific bits. We translate to the cap rights lattice so a
 * single resolve() enforces what NT would have checked via NtAccessCheck. */
uint64_t vsl_nt_desired_access_to_cap(uint32_t a) {
    uint64_t r = 0;
    /* Generic rights */
    if (a & 0x80000000u) r |= WUBU_RIGHT_READ | WUBU_RIGHT_INSPECT | WUBU_RIGHT_RECV; /* GENERIC_READ */
    if (a & 0x40000000u) r |= WUBU_RIGHT_WRITE | WUBU_RIGHT_SEND;                            /* GENERIC_WRITE */
    if (a & 0x20000000u) r |= WUBU_RIGHT_EXEC | WUBU_RIGHT_INVOKE;                           /* GENERIC_EXECUTE */
    if (a & 0x10000000u) r |= WUBU_RIGHTS_ALL;                                               /* GENERIC_ALL */
    /* Standard rights */
    if (a & 0x00010000u) r |= WUBU_RIGHT_DELETE;                                             /* DELETE */
    if (a & 0x00020000u) r |= WUBU_RIGHT_READ | WUBU_RIGHT_INSPECT;                         /* READ_CONTROL */
    if (a & 0x00040000u) r |= WUBU_RIGHT_WRITE | WUBU_RIGHT_GRANT;                           /* WRITE_DAC */
    if (a & 0x00080000u) r |= WUBU_RIGHT_WRITE | WUBU_RIGHT_GRANT;                           /* WRITE_OWNER */
    if (a & 0x00100000u) r |= WUBU_RIGHT_RECV;                                               /* SYNCHRONIZE */
    /* Key-specific access bits */
    if (a & 0x00000001u) r |= WUBU_RIGHT_READ;        /* KEY_QUERY_VALUE */
    if (a & 0x00000002u) r |= WUBU_RIGHT_WRITE;       /* KEY_SET_VALUE */
    if (a & 0x00000004u) r |= WUBU_RIGHT_WRITE | WUBU_RIGHT_DERIVE; /* KEY_CREATE_SUB_KEY */
    if (a & 0x00000008u) r |= WUBU_RIGHT_DERIVE;      /* KEY_CREATE_LINK */
    if (a & 0x00000010u) r |= WUBU_RIGHT_INSPECT;     /* KEY_NOTIFY */
    if (a & 0x00000020u) r |= WUBU_RIGHT_RECV;        /* KEY_NOTIFY(2) */
    /* File-specific access bits */
    if (a & 0x00000001u) r |= WUBU_RIGHT_READ;        /* FILE_READ_DATA */
    if (a & 0x00000002u) r |= WUBU_RIGHT_WRITE;       /* FILE_WRITE_DATA */
    if (a & 0x00000010u) r |= WUBU_RIGHT_RECV;        /* FILE_APPEND_DATA */
    if (a & 0x00000080u) r |= WUBU_RIGHT_INSPECT;     /* FILE_READ_ATTRIBUTES */
    if (a & 0x00000010u) r |= WUBU_RIGHT_WRITE;       /* FILE_WRITE_ATTRIBUTES */
    if (a & 0x00000200u) r |= WUBU_RIGHT_READ;        /* STANDARD_RIGHTS_READ / KEY_READ */
    if (a & 0x00000200u) r |= WUBU_RIGHT_READ;        /* (dedup) */
    /* KEY_READ = STANDARD_RIGHTS_READ(0x20000) | KEY_QUERY_VALUE(0x1) | KEY_ENUMERATE | KEY_NOTIFY(0x10) | SYNCHRONIZE(0x100000) */
    if (a & 0x00020010u) r |= WUBU_RIGHT_READ | WUBU_RIGHT_INSPECT;
    /* KEY_WRITE = STANDARD_RIGHTS_WRITE(0x20000)?? actually WRITE_DAC|...) */
    if (a & 0x00020007u) r |= WUBU_RIGHT_WRITE | WUBU_RIGHT_DERIVE;
    return r;
}

/* ---- Cap-backed NT handle lifecycle ---- */
int vsl_nt_cap_acquire(vsl_nt_bridge_ctx_t *ctx, nt_object_type_t nt_type,
                       uint32_t desired_access, int vsl_fd, uint64_t styx_fid,
                       uint64_t data, uint32_t *out_handle,
                       wubu_cap_token_t *out_obj_token) {
    if (!ctx || !ctx->cap_ht || !out_handle) return -1;
    uint64_t rights = vsl_nt_desired_access_to_cap(desired_access);
    if (rights == 0) return -1; /* NT: a handle must carry at least one right */

    nt_objrec_t *rec = (nt_objrec_t *)malloc(sizeof(*rec));
    if (!rec) return -1;
    rec->vsl_fd = vsl_fd;
    rec->styx_fid = styx_fid;
    rec->data = data;
    rec->type = nt_type;

    uint16_t kind = vsl_nt_kind_to_cap(nt_type);
    int32_t audience[WUBU_CAP_AUDIENCE_MAX];
    audience[0] = ctx->current_pid;
    for (int i = 1; i < WUBU_CAP_AUDIENCE_MAX; i++) audience[i] = WUBU_PID_NONE;

    wubu_cap_token_t objtok = WUBU_CAP_TOKEN_NULL;
    int rc = wubu_cap_create(kind, rights, audience, 0,
                             (uintptr_t)rec, ctx->current_pid,
                             WUBU_CAP_IDX_NONE, &objtok);
    if (rc <= 0 || wubu_cap_token_is_null(objtok)) {
        free(rec);
        return -1; /* cap object allocation failed */
    }
    uint32_t obj_idx = (uint32_t)wubu_cap_token_idx(objtok);

    /* An NT handle value IS the capability handle-table slot index: insert a
     * handle slot pointing at obj_idx; the resulting slot number is the NT
     * handle value back to the caller. */
    wubu_cap_token_t htok = wubu_cap_handle_insert(ctx->cap_ht, obj_idx, 0);
    if (wubu_cap_token_is_null(htok)) {
        wubu_cap_revoke(objtok);
        free(rec);
        return -1;
    }
    uint32_t slot = (uint32_t)wubu_cap_token_idx(htok);

    *out_handle = slot; /* NT handle == cap slot */
    if (out_obj_token) *out_obj_token = objtok;
    return 0;
}

/* Forward: resolve an NT handle (0x1000 + idx) to its backing cap-table slot.
 * Defined after index_of below; declared here so resolve_handle/revoke/release
 * (which precede it) can call it. */
static bool vsl_nt_cap_slot_of(vsl_nt_bridge_ctx_t *ctx, uint32_t nt_handle,
                               uint32_t *slot);
/* Forward: legacy record index for an NT handle; defined below. */
static int vsl_nt_cap_index_of(vsl_nt_bridge_ctx_t *ctx, uint32_t nt_handle);

/* Accept EITHER handle convention and yield the backing cap slot.
 *
 * Two minters exist and both feed these entry points:
 *   - vsl_nt_allocate_handle() -> legacy record handle 0x1000+idx, bound to a
 *     cap slot by vsl_nt_cap_bind(). This is what real syscalls carry.
 *   - vsl_nt_cap_acquire() / vsl_nt_cap_make_handle() -> a bare cap slot, used
 *     directly by the SRM/SD facade where no NT record is involved.
 *
 * The legacy table is consulted first: it is exact, and its values (>= 0x1000)
 * cannot collide with cap slots. Only if that misses do we treat the value as
 * a raw cap handle. */
static bool vsl_nt_cap_slot_of_any(vsl_nt_bridge_ctx_t *ctx, uint32_t handle,
                                   uint32_t *slot) {
    if (!ctx || !ctx->cap_ht) return false;
    if (vsl_nt_cap_slot_of(ctx, handle, slot)) return true;
    if (wubu_cap_handle_slot_live(ctx->cap_ht, handle)) {
        if (slot) *slot = handle;
        return true;
    }
    return false;
}

bool vsl_nt_cap_resolve_handle(vsl_nt_bridge_ctx_t *ctx, uint32_t nt_handle,
                               uint32_t desired_access) {
    uint32_t slot = 0;
    if (!vsl_nt_cap_slot_of_any(ctx, nt_handle, &slot)) return false;
    uint64_t req = vsl_nt_desired_access_to_cap(desired_access);
    if (req == 0) req = WUBU_RIGHT_INSPECT; /* null-mask probes still need resolve */
    if (!wubu_cap_handle_resolve_by_slot(ctx->cap_ht, ctx->current_pid,
                                         slot, req))
        return false;

    /* Second gate: the per-handle ceiling. NtDuplicateObject aliases the SAME
     * object but narrows what the new handle carries, so object rights alone
     * are not the answer -- a reduced duplicate must not report authority its
     * source had. This is NT's rule that GrantedAccess is a property of the
     * HANDLE, not of the object it points at. Raw cap-slot handles minted by
     * vsl_nt_cap_acquire() have no record, so their grant IS the object grant. */
    int idx = vsl_nt_cap_index_of(ctx, nt_handle);
    if (idx >= 0 && ctx->handle_table[idx].cap_backed) {
        uint32_t want = wubu_nt_expand_generic(ctx->handle_table[idx].type,
                                              desired_access);
        if (want != 0 &&
            (ctx->handle_table[idx].cap_rights & want) != want)
            return false;
    }
    return true;
}

int vsl_nt_cap_revoke_handle(vsl_nt_bridge_ctx_t *ctx, uint32_t nt_handle) {
    uint32_t slot = 0;
    if (!vsl_nt_cap_slot_of_any(ctx, nt_handle, &slot))
        return NT_STATUS_INVALID_HANDLE;
    /* Authority gate: only a holder with DELETE rights may revoke. */
    uint64_t del = vsl_nt_desired_access_to_cap(0x00010000u); /* DELETE */
    if (!del) del = WUBU_RIGHT_DELETE;
    if (!wubu_cap_handle_resolve_by_slot(ctx->cap_ht, ctx->current_pid,
                                         slot, del))
        return NT_STATUS_ACCESS_DENIED;
    int rc = wubu_cap_handle_revoke_by_slot(ctx->cap_ht, slot);
    switch (rc) {
    case WUBU_CAP_OK:       return 0;
    case WUBU_CAP_EREVOKED: return NT_STATUS_ACCESS_DENIED;
    default:                return NT_STATUS_INVALID_HANDLE;
    }
}

int vsl_nt_cap_release(vsl_nt_bridge_ctx_t *ctx, uint32_t nt_handle) {
    uint32_t slot = 0;
    if (!vsl_nt_cap_slot_of_any(ctx, nt_handle, &slot))
        return NT_STATUS_INVALID_HANDLE;
    int rc = wubu_cap_handle_close_slot(ctx->cap_ht, slot);
    return (rc == WUBU_CAP_OK) ? 0 : NT_STATUS_INVALID_HANDLE;
}

int vsl_nt_cap_make_handle(vsl_nt_bridge_ctx_t *ctx,
                           wubu_cap_token_t obj_token,
                           uint32_t *out_handle) {
    if (!ctx || !ctx->cap_ht || wubu_cap_token_is_null(obj_token) || !out_handle)
        return -1;
    /* obj_token.idx is the registry slot of the backing object; inserting a
     * new handle slot for that same idx aliases the object, so revoking the
     * object (via any alias) cascades to every alias. */
    wubu_cap_token_t htok =
        wubu_cap_handle_insert(ctx->cap_ht,
                               (uint32_t)wubu_cap_token_idx(obj_token), 0);
    if (wubu_cap_token_is_null(htok))
        return -1;
    *out_handle = (uint32_t)wubu_cap_token_idx(htok);
    return 0;
}

bool vsl_nt_cap_handle_is_valid(vsl_nt_bridge_ctx_t *ctx, uint32_t nt_handle,
                                uint32_t desired_access) {
    return vsl_nt_cap_resolve_handle(ctx, nt_handle, desired_access);
}

/* ---- Legacy handle table <-> cap substrate binding (Axis 1) ---- */

/* Locate the table index for an NT handle. Returns -1 if not present. */
static int vsl_nt_cap_index_of(vsl_nt_bridge_ctx_t *ctx, uint32_t nt_handle) {
    for (int i = 0; i < 4096; i++) {
        if (ctx->handle_table[i].valid &&
            ctx->handle_table[i].nt_handle == nt_handle)
            return i;
    }
    return -1;
}

/* An NT handle value (0x1000 + table index) is NOT a cap-table slot index.
 * Resolve it to the backing cap-table slot stored on its handle-table record.
 * Returns false (and leaves *slot untouched) if the handle is unknown or not
 * cap-backed. */
static bool vsl_nt_cap_slot_of(vsl_nt_bridge_ctx_t *ctx, uint32_t nt_handle,
                               uint32_t *slot) {
    int idx = vsl_nt_cap_index_of(ctx, nt_handle);
    if (idx < 0 || !ctx->handle_table[idx].cap_backed) return false;
    if (slot) *slot = ctx->handle_table[idx].cap_slot;
    return true;
}

int vsl_nt_cap_bind(vsl_nt_bridge_ctx_t *ctx, uint32_t nt_handle,
                    nt_object_type_t type, uint32_t desired_access) {
    if (!ctx || !ctx->cap_ht || nt_handle == 0) return -1;
    int idx = vsl_nt_cap_index_of(ctx, nt_handle);
    if (idx < 0) return -1;
    if (ctx->handle_table[idx].cap_backed) return 0; /* already bound */

    /* vsl_nt_cap_acquire derives the NT access mask into cap rights. If the
     * caller asked for no recognizable access, fall back to read so the
     * object is still bound (a handle with zero authority is not useful). */
    if (vsl_nt_desired_access_to_cap(desired_access) == 0)
        desired_access |= 0x80000000u; /* GENERIC_READ */

    uint32_t slot = 0;
    wubu_cap_token_t tok = WUBU_CAP_TOKEN_NULL;
    if (vsl_nt_cap_acquire(ctx, type, desired_access,
                           ctx->handle_table[idx].vsl_fd,
                           ctx->handle_table[idx].styx_fid,
                           ctx->handle_table[idx].data, &slot, &tok) != 0)
        return -1;

    ctx->handle_table[idx].cap_slot  = slot;
    ctx->handle_table[idx].cap_token = tok;
    ctx->handle_table[idx].cap_backed = true;
    /* Granted access, in concrete terms. wubu_nt_expand_generic is the same
     * mapping the SRM applies to a DesiredAccess request, so the recorded
     * ceiling and the requested mask are expressed in the same bits. */
    ctx->handle_table[idx].cap_rights =
        wubu_nt_expand_generic(type, desired_access);
    return 0;
}

uint32_t vsl_nt_cap_authorize(vsl_nt_bridge_ctx_t *ctx, uint32_t nt_handle,
                              uint32_t desired_access) {
    if (!ctx) return NT_STATUS_INVALID_HANDLE;
    int idx = vsl_nt_cap_index_of(ctx, nt_handle);
    if (idx < 0) return NT_STATUS_INVALID_HANDLE;
    if (!ctx->handle_table[idx].cap_backed) return NT_STATUS_SUCCESS;

    /* Two independent gates, both must pass:
     *
     *  (a) Object liveness + audience + rights: the cap object this handle
     *      points at must be live and cover the requested cap rights. A
     *      revoked object fails here.
     *  (b) Handle ceiling: the handle's own GrantedAccess (cap_rights) must
     *      cover the request. This is what makes a NtDuplicateObject rights
     *      reduction stick -- the duplicate shares the object, so (a) would
     *      alone let it keep the source's full authority. cap_rights is the
     *      ceiling NT keeps per handle. */
    uint64_t req = vsl_nt_desired_access_to_cap(desired_access);
    if (req == 0) req = WUBU_RIGHT_INSPECT;
    if (!wubu_cap_handle_resolve_by_slot(ctx->cap_ht, ctx->current_pid,
                                         ctx->handle_table[idx].cap_slot, req))
        return NT_STATUS_ACCESS_DENIED;

    uint32_t want_nt = wubu_nt_expand_generic(ctx->handle_table[idx].type,
                                              desired_access);
    if (want_nt == 0) return NT_STATUS_SUCCESS;
    if ((ctx->handle_table[idx].cap_rights & want_nt) != want_nt)
        return NT_STATUS_ACCESS_DENIED;
    return NT_STATUS_SUCCESS;
}

int vsl_nt_cap_unbind(vsl_nt_bridge_ctx_t *ctx, uint32_t nt_handle) {
    if (!ctx || !ctx->cap_ht) return -1;
    int idx = vsl_nt_cap_index_of(ctx, nt_handle);
    if (idx < 0) return -1;
    if (!ctx->handle_table[idx].cap_backed) return -1;

    int rc = wubu_cap_handle_close_slot(ctx->cap_ht,
                                        ctx->handle_table[idx].cap_slot);
    ctx->handle_table[idx].cap_backed = false;
    ctx->handle_table[idx].cap_slot  = 0;
    ctx->handle_table[idx].cap_token = WUBU_CAP_TOKEN_NULL;
    return (rc == WUBU_CAP_OK) ? 0 : -1;
}

int vsl_nt_cap_alias(vsl_nt_bridge_ctx_t *ctx, uint32_t src_handle,
                     uint32_t dst_handle) {
    if (!ctx || !ctx->cap_ht) return -1;
    int si = vsl_nt_cap_index_of(ctx, src_handle);
    int di = vsl_nt_cap_index_of(ctx, dst_handle);
    if (si < 0 || di < 0) return -1;
    if (!ctx->handle_table[si].cap_backed) return -1;
    if (wubu_cap_token_is_null(ctx->handle_table[si].cap_token)) return -1;

    /* A second handle slot pointing at the SAME object: revoking the object
     * through either handle now collapses both. */
    uint32_t new_slot = 0;
    if (vsl_nt_cap_make_handle(ctx, ctx->handle_table[si].cap_token,
                               &new_slot) != 0)
        return -1;
    ctx->handle_table[di].cap_slot   = new_slot;
    ctx->handle_table[di].cap_token  = ctx->handle_table[si].cap_token;
    ctx->handle_table[di].cap_backed = true;
    /* Inherit the source's granted access. The handler narrows this when the
     * caller passed a DesiredAccessMask, so the duplicate can never hold MORE
     * than the source did -- the reduction NtDuplicateObject exists to make. */
    ctx->handle_table[di].cap_rights = ctx->handle_table[si].cap_rights;
    return 0;
}

int vsl_nt_cap_alias_reduced(vsl_nt_bridge_ctx_t *ctx, uint32_t src_handle,
                             uint32_t dst_handle, uint32_t desired_access) {
    if (vsl_nt_cap_alias(ctx, src_handle, dst_handle) != 0) return -1;
    int di = vsl_nt_cap_index_of(ctx, dst_handle);
    if (di < 0) return -1;
    if (desired_access) {
        /* Intersect, never widen: a request for more than the source holds
         * is clamped down to the source, exactly as NT clamps. */
        uint32_t want = wubu_nt_expand_generic(
            ctx->handle_table[di].type, desired_access);
        ctx->handle_table[di].cap_rights &= want;
    }
    return 0;
}

int vsl_nt_cap_revoke_nt(vsl_nt_bridge_ctx_t *ctx, uint32_t nt_handle) {
    if (!ctx || !ctx->cap_ht) return -1;
    int idx = vsl_nt_cap_index_of(ctx, nt_handle);
    if (idx < 0) return -1;
    if (!ctx->handle_table[idx].cap_backed) return -1;
    if (vsl_nt_cap_revoke_handle(ctx, ctx->handle_table[idx].cap_slot) != 0)
        return -1;
    return 0;
}

/* Real NT *_ALL_ACCESS masks. STANDARD_RIGHTS_REQUIRED = 0x000F0000
 * (DELETE|READ_CONTROL|WRITE_DAC|WRITE_OWNER), SYNCHRONIZE = 0x00100000. */
#define WUBU_STD_RIGHTS_REQUIRED 0x000F0000u
#define WUBU_SYNCHRONIZE         0x00100000u

uint32_t vsl_nt_default_access_for_type(nt_object_type_t type) {
    switch (type) {
    /* Namespace / file-like: full DATA access both ways. */
    case NT_OBJECT_TYPE_FILE:
    case NT_OBJECT_TYPE_DIRECTORY:
    case NT_OBJECT_TYPE_SYMBOLIC_LINK:
        return WUBU_STD_RIGHTS_REQUIRED | WUBU_SYNCHRONIZE | 0x000001FFu;
    /* Registry. */
    case NT_OBJECT_TYPE_KEY:
        return WUBU_STD_RIGHTS_REQUIRED | WUBU_SYNCHRONIZE | 0x000003FFu;
    /* Synchronization primitives. */
    case NT_OBJECT_TYPE_EVENT:
    case NT_OBJECT_TYPE_SEMAPHORE:
        return WUBU_STD_RIGHTS_REQUIRED | WUBU_SYNCHRONIZE | 0x00000003u;
    case NT_OBJECT_TYPE_MUTANT:
    case NT_OBJECT_TYPE_KEYED_EVENT:
    case NT_OBJECT_TYPE_EVENT_PAIR:
        return WUBU_STD_RIGHTS_REQUIRED | WUBU_SYNCHRONIZE | 0x00000001u;
    case NT_OBJECT_TYPE_TIMER:
        return WUBU_STD_RIGHTS_REQUIRED | WUBU_SYNCHRONIZE | 0x0000001Fu;
    case NT_OBJECT_TYPE_WAITABLE_PORT:
        return WUBU_STD_RIGHTS_REQUIRED | WUBU_SYNCHRONIZE | 0x0000000Fu;
    case NT_OBJECT_TYPE_IO_COMPLETION:
        return WUBU_STD_RIGHTS_REQUIRED | 0x00000007u;
    /* Address space / views. */
    case NT_OBJECT_TYPE_SECTION:
    case NT_OBJECT_TYPE_DEBUG_OBJECT:
        return WUBU_STD_RIGHTS_REQUIRED | WUBU_SYNCHRONIZE | 0x0000000Fu;
    /* Identity and process control carry the heaviest authority. */
    case NT_OBJECT_TYPE_TOKEN:
        return WUBU_STD_RIGHTS_REQUIRED | 0x000003FFu;
    case NT_OBJECT_TYPE_PROCESS:
        return 0x001F0FFFu;
    case NT_OBJECT_TYPE_THREAD:
        return 0x001F03FFu;
    case NT_OBJECT_TYPE_PORT:
        return WUBU_STD_RIGHTS_REQUIRED | WUBU_SYNCHRONIZE | 0x0000000Fu;
    case NT_OBJECT_TYPE_PROFILE:
        return WUBU_STD_RIGHTS_REQUIRED | 0x00000002u;
    /* Transaction / TM family: query + control. */
    case NT_OBJECT_TYPE_JOB:
    case NT_OBJECT_TYPE_TRANSACTION:
    case NT_OBJECT_TYPE_TRANSACTION_MANAGER:
    case NT_OBJECT_TYPE_RESOURCE_MANAGER:
    case NT_OBJECT_TYPE_ENLISTMENT:
    case NT_OBJECT_TYPE_TM:
    case NT_OBJECT_TYPE_WORK_ITEM:
        return WUBU_STD_RIGHTS_REQUIRED | 0x00000007u;
    default:
        return WUBU_STD_RIGHTS_REQUIRED | WUBU_SYNCHRONIZE;
    }
}

/* ------------------------------------------------------------------ */
/* Security reference monitor facade                                   */
/* ------------------------------------------------------------------ */

/* Validate that `token_handle` names a live record -- the SRM treats ANY
 * cap-backed handle as a "subject" handle, because the caller's identity is
 * derived from the token (if it is one) or from the process holding the
 * handle (vsl_nt_cap_token_sidset falls back to the caller pid). Requiring a
 * TOKEN made the facade reject file/event/process handles, which is not what
 * SeAccessCheck semantics demand. */
static bool se_handle_is_valid(vsl_nt_bridge_ctx_t *ctx, uint32_t token_handle) {
    if (!ctx || token_handle == 0) return false;
    for (int i = 0; i < 4096; i++) {
        if (ctx->handle_table[i].valid &&
            ctx->handle_table[i].nt_handle == token_handle)
            return true;
    }
    return false;
}

bool vsl_nt_cap_token_sidset(vsl_nt_bridge_ctx_t *ctx, uint32_t token_handle,
                             wubu_nt_sidset_t *out) {
    if (!out || !ctx) return false;
    wubu_nt_sidset_reset(out);

    /* The caller's identity is whoever holds this authority. A real NT token
     * carries an authentication LUID we use as the owner SID; for any other
     * cap-backed handle the bridge's current pid plays the same role (the
     * sidset is the "subject" the SRM evaluates, and every handle in this
     * process is held by that subject). */
    nt_token_entry_t *t = vsl_nt_token_from_handle(token_handle);
    if (t) {
        out->owner_sid = (uint32_t)(t->luid_low & 0xFFFFFFFFu);
        out->mic = 0x2000u;                       /* MEDIUM_MANDATORY_LEVEL */
        if (t->imp_level <= 0) out->mic = 0x3000u;
        else if (t->imp_level >= 3) out->mic = 0x1000u;
        for (uint32_t i = 0; i < t->group_count; i++)
            if (t->group[i].attr & NT_PRIV_ATTR_ENABLED)
                wubu_nt_sidset_add(out, t->group[i].sid);
    } else {
        out->owner_sid = (uint32_t)ctx->current_pid;
        out->mic = 0x2000u;
    }

    /* The owner SID must be non-zero: a subject with no identity is never
     * granted by a real DACL, and a deny-everything default must fail closed. */
    if (out->owner_sid == 0) out->owner_sid = (uint32_t)g_vsl.current_pid;
    out->owner_sid |= 1u;   /* guarantee a distinct, non-zero principal */
    return true;
}

int64_t vsl_nt_cap_se_check(vsl_nt_bridge_ctx_t *ctx, uint32_t token_handle,
                            const void *security_descriptor,
                            nt_object_type_t type, uint32_t desired_access,
                            uint32_t *granted_out) {
    if (granted_out) *granted_out = 0;
    if (!ctx) return NT_STATUS_ACCESS_DENIED;

    /* 1. Validate the caller handle. A bad handle is INVALID_HANDLE, not
     *    ACCESS_DENIED. */
    if (!se_handle_is_valid(ctx, token_handle))
        return NT_STATUS_INVALID_HANDLE;

    /* 2. The DACL decides whether the access is permitted. */
    wubu_nt_sidset_t subj;
    if (!vsl_nt_cap_token_sidset(ctx, token_handle, &subj))
        return NT_STATUS_ACCESS_DENIED;

    wubu_nt_sd_view_t sd;
    memset(&sd, 0, sizeof(sd));
    sd.dacl_present = false;      /* absent DACL => full access (NT semantics) */
    sd.no_write_up  = true;       /* the MIC is enforced by default */
    sd.label_mic    = 0x2000u;    /* MEDIUM_MANDATORY_LEVEL */
    if (security_descriptor) {
        if (!wubu_nt_sd_read(security_descriptor, &sd))
            return NT_STATUS_INVALID_PARAMETER;
    }

    wubu_nt_access_result_t r = wubu_nt_access_check(&sd, &subj, type,
                                                     desired_access);
    if (r.status != WUBU_NT_STATUS_SUCCESS)
        return NT_STATUS_ACCESS_DENIED;

    /* 3. The cap can only ever REDUCE the grant. A handle's authority was
     *    fixed when it was minted (or reduced by NtDuplicateObject); the DACL
     *    permitting more does not enlarge it. */
    uint32_t granted = r.granted;
    if (vsl_nt_cap_handle_is_bound(ctx, token_handle)) {
        /* The handle is cap-backed: the DACL grant can only ever be REDUCED.
         * A permissive DACL never enlarges what the handle was granted, and a
         * revoked object (cap_rights == 0) refuses outright. */
        uint32_t cap_mask = vsl_nt_cap_granted_mask(ctx, token_handle);
        granted &= cap_mask;
        if ((granted & desired_access) != desired_access)
            return WUBU_NT_STATUS_ACCESS_DENIED;
    }

    if (granted_out) *granted_out = granted;
    return NT_STATUS_SUCCESS;
}

uint32_t vsl_nt_cap_granted_mask(vsl_nt_bridge_ctx_t *ctx, uint32_t nt_handle) {
    /* Uncapable: a caller with no cap binding gets the full NT mask, so the
     * DACL alone decides -- the pre-Axis-1 behaviour. */
    if (!vsl_nt_cap_handle_is_bound(ctx, nt_handle))
        return 0xFFFFFFFFu;
    const vsl_nt_bridge_ctx_t *c = ctx;
    for (int i = 0; i < 4096; i++) {
        if (!c->handle_table[i].valid ||
            c->handle_table[i].nt_handle != nt_handle) continue;
        /* Liveness: resolve with required_rights == 0. A revoked object fails
         * to resolve, so its handles report no authority at all.
         *
         * Probe through the cap SLOT, not handle_table[i].cap_token. The two
         * are minted together but only the slot is revalidated on every
         * rebind (vsl_nt_cap_bind / alias / duplicate), and cap_token is left
         * pointing at whatever object idx held when the record was created.
         * A later cap_create() reuses that idx for a different object, so the
         * token's (idx, gen) pair goes stale and liveness would report a live
         * handle as dead -- silently zeroing its granted access. */
        if (!wubu_cap_handle_resolve_by_slot(ctx->cap_ht, ctx->current_pid,
                                             c->handle_table[i].cap_slot, 0))
            return 0u;
        /* The granted rights live on the NT handle (cap_rights), not on the
         * opaque cap object: that is where generic expansion and any
         * NtDuplicateObject reduction were applied. */
        return c->handle_table[i].cap_rights;
    }
    return 0u;
}

bool vsl_nt_cap_handle_is_bound(vsl_nt_bridge_ctx_t *ctx, uint32_t nt_handle) {
    if (!ctx) return false;
    for (int i = 0; i < 4096; i++) {
        if (ctx->handle_table[i].valid && ctx->handle_table[i].nt_handle == nt_handle)
            return ctx->handle_table[i].cap_backed;
    }
    return false;
}
