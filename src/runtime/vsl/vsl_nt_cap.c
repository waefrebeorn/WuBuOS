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

bool vsl_nt_cap_resolve_handle(vsl_nt_bridge_ctx_t *ctx, uint32_t nt_handle,
                               uint32_t desired_access) {
    if (!ctx || !ctx->cap_ht || nt_handle == 0) return false;
    uint64_t req = vsl_nt_desired_access_to_cap(desired_access);
    if (req == 0) req = WUBU_RIGHT_INSPECT; /* null-mask probes still need resolve */
    return wubu_cap_handle_resolve_by_slot(ctx->cap_ht, ctx->current_pid,
                                           nt_handle, req) != NULL;
}

int vsl_nt_cap_revoke_handle(vsl_nt_bridge_ctx_t *ctx, uint32_t nt_handle) {
    if (!ctx || !ctx->cap_ht || nt_handle == 0) return NT_STATUS_INVALID_HANDLE;
    /* Authority gate: only a holder with DELETE rights may revoke. */
    uint64_t del = vsl_nt_desired_access_to_cap(0x00010000u); /* DELETE */
    if (!del) del = WUBU_RIGHT_DELETE;
    if (!wubu_cap_handle_resolve_by_slot(ctx->cap_ht, ctx->current_pid,
                                         nt_handle, del))
        return NT_STATUS_ACCESS_DENIED;
    int rc = wubu_cap_handle_revoke_by_slot(ctx->cap_ht, nt_handle);
    switch (rc) {
    case WUBU_CAP_OK:       return 0;
    case WUBU_CAP_EREVOKED: return NT_STATUS_ACCESS_DENIED;
    default:                return NT_STATUS_INVALID_HANDLE;
    }
}

int vsl_nt_cap_release(vsl_nt_bridge_ctx_t *ctx, uint32_t nt_handle) {
    if (!ctx || !ctx->cap_ht || nt_handle == 0) return NT_STATUS_INVALID_HANDLE;
    int rc = wubu_cap_handle_close_slot(ctx->cap_ht, nt_handle);
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
    return 0;
}

uint32_t vsl_nt_cap_authorize(vsl_nt_bridge_ctx_t *ctx, uint32_t nt_handle,
                              uint32_t desired_access) {
    if (!ctx) return NT_STATUS_INVALID_HANDLE;
    int idx = vsl_nt_cap_index_of(ctx, nt_handle);
    if (idx < 0) return NT_STATUS_INVALID_HANDLE;
    if (!ctx->handle_table[idx].cap_backed) return NT_STATUS_SUCCESS;

    uint64_t req = vsl_nt_desired_access_to_cap(desired_access);
    if (req == 0) req = WUBU_RIGHT_INSPECT; /* inspect-only probe */
    if (!wubu_cap_handle_resolve_by_slot(ctx->cap_ht, ctx->current_pid,
                                         ctx->handle_table[idx].cap_slot, req))
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
