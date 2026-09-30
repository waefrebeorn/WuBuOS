/*
 * vsl_nt_cap.h -- WuBuOS NT Object Manager backed by wubu_cap.
 *
 * This is the AGI authority surface your NT-kernel-level work needs: instead of
 * the flat 4096-slot handle table in vsl_nt_bridge_ctx_t, every NT object type
 * (File, Key, Directory, Section, Process, Token, ...) is a wubu_cap object and
 * every NT handle IS a capability handle. One authority substrate answers the
 * question agents keep asking the Object Manager: "what exactly is this AI agent
 * allowed to do?" — revocably, with central audit.
 *
 * NT handles are small integers = capability handle-table slot (0 == null),
 * so the existing uint32-NT-handle ABI is preserved while the underlying
 * authority (generation-counted, audience-gated, rights-checked, revocable)
 * is the wubu_cap model. See wubu_cap.h for the primitive contract.
 *
 * Slice coverage: NT_OBJECT_TYPE_FILE, DIRECTORY, KEY, SECTION, PROCESS,
 * THREAD, TOKEN, EVENT, MUTANT, SEMAPHORE, TIMER, PORT, TRANSACTION plus
 * NT access-mask -> wubu_cap rights translation. Full NT SD/ACL + namespace
 * (ObOpenByKeyName) lands in Axis 2; this module is the backing authority.
 */
#ifndef WUBU_VSL_NT_CAP_H
#define WUBU_VSL_NT_CAP_H

#include <stdint.h>
#include <stdbool.h>
#include "vsl_nt_bridge.h"   /* vsl_nt_bridge_ctx_t, nt_object_type_t */
#include "wubu_nt_sd.h"      /* wubu_nt_sidset_t (SRM facade) */
#include "wubu_cap/wubu_cap.h"
#include "vsl_nt_bridge.h"

/* Per-NT-handle backing record (NT handle value = cap slot index). */
typedef struct nt_objrec {
    int            vsl_fd;     /* VSL/Styx file descriptor, if any */
    uint64_t       styx_fid;   /* Styx9 fid, if any */
    uint64_t       data;       /* opaque payload (pid_t, mmap base, ...) */
    nt_object_type_t type;     /* NT object type this handle represents */
} nt_objrec_t;

/* NT access-mask -> wubu_cap rights translation. */
uint64_t vsl_nt_desired_access_to_cap(uint32_t nt_desired_access);

/* NT object type -> wubu_cap kind. */
uint16_t vsl_nt_kind_to_cap(nt_object_type_t nt_type);

/* Cap-backed NT handle lifecycle (the AGI authority gate).
 *  - acquire opens a cap object of `nt_type` with rights derived from
 *    `desired_access`, audience={ctx->current_pid}, and returns an NT handle
 *    (cap slot) + the raw cap token (for privileged revoke/derive tests).
 *  - resolve_handle authorizes `desired_access` against the handle's cap;
 *    returns true iff the caller is in the audience AND the granted rights
 *    cover the requested access. This is the per-op gate every NT syscall
 *    against a handle should run before touching the object.
 *  - revoke_handle authorizes DELETE rights then revokes the backing object,
 *    cascading to every handle (any process) that aliases it -- the single
 *    "agent lost authority" operation.
 *  - release closes the handle slot. */
int  vsl_nt_cap_acquire(vsl_nt_bridge_ctx_t *ctx, nt_object_type_t nt_type,
                        uint32_t desired_access, int vsl_fd, uint64_t styx_fid,
                        uint64_t data, uint32_t *out_handle,
                        wubu_cap_token_t *out_obj_token);
bool vsl_nt_cap_resolve_handle(vsl_nt_bridge_ctx_t *ctx, uint32_t nt_handle,
                               uint32_t desired_access);
int  vsl_nt_cap_revoke_handle(vsl_nt_bridge_ctx_t *ctx, uint32_t nt_handle);
int vsl_nt_cap_release(vsl_nt_bridge_ctx_t *ctx, uint32_t nt_handle);

/* Issue a SECOND NT handle (cap slot) that aliases the SAME object as
 * `obj_token` -- the NT analogue of NtDuplicateObject. Used to prove that
 * revocation of one handle cascades to its aliases (the per-agent authority
 * primitive the NT architecture article says AI agents need). */
int vsl_nt_cap_make_handle(vsl_nt_bridge_ctx_t *ctx,
                           wubu_cap_token_t obj_token,
                           uint32_t *out_handle);

/* Authoritative answer for "is NT handle valid + carrying >= rights".
 * Returns true only if the backing cap resolves for the requested rights. */
bool vsl_nt_cap_handle_is_valid(vsl_nt_bridge_ctx_t *ctx, uint32_t nt_handle,
                                uint32_t desired_access);

/* ---- NT key authority masks shared by the registry gates and tests ----
 * WUBU_NT_KEY_DEFAULT_ACCESS: KEY_QUERY_VALUE | KEY_SET_VALUE |
 *   KEY_CREATE_SUB_KEY | KEY_ENUMERATE_SUB_KEY | KEY_NOTIFY | DELETE |
 *   READ_CONTROL | SYNCHRONIZE = 0x000F0033. This is the authority a key
 *   handle is bound with at open/create time. */
#define WUBU_NT_KEY_DEFAULT_ACCESS 0x000F0033u
/* Mutation gate: KEY_SET_VALUE (0x2) | DELETE (0x10000). */
#define WUBU_NT_KEY_WRITE_GUARD 0x00010002u
/* Read gate: KEY_QUERY_VALUE (0x1) | READ_CONTROL (0x20000). */
#define WUBU_NT_KEY_READ_GUARD  0x00020001u

/* The NT *_ALL_ACCESS mask for an object type: what a freshly created
 * object grants its creator. Real NT values (STANDARD_RIGHTS_REQUIRED
 * 0x000F0000 | SYNCHRONIZE 0x00100000 | type-specific bits), so the cap
 * rights derived from them are meaningful rather than blanket. */
uint32_t vsl_nt_default_access_for_type(nt_object_type_t type);

/* ---- The security reference monitor facade (Axis 2) ------------------
 *
 * The single place an NT open is authorised. It fuses three independent
 * sources of truth into one NTSTATUS, the way NtAccessCheck does:
 *
 *   1. The token   -- which SIDs/groups and which integrity level the caller
 *                     holds.
 *   2. The object  -- its security descriptor, whose DACL and mandatory label
 *                     decide whether the access is permitted at all.
 *   3. The cap     -- what authority the caller's live handle actually
 *                     carries. The DACL can permit an access the handle was
 *                     never granted, and the cap can hold an authority whose
 *                     object has since been revoked; both must pass.
 *
 * Granting is an intersection, never a union: the result is the most
 * permissive mask permitted by ALL THREE. That is the property that makes the
 * answer meaningful rather than advisory. */
int64_t vsl_nt_cap_se_check(vsl_nt_bridge_ctx_t *ctx, uint32_t token_handle,
                            const void *security_descriptor,
                            nt_object_type_t type, uint32_t desired_access,
                            uint32_t *granted_out);

/* Build the SID set the SD evaluator needs from a live NT token. */
bool vsl_nt_cap_token_sidset(vsl_nt_bridge_ctx_t *ctx,
                             uint32_t token_handle, wubu_nt_sidset_t *out);

/* ---- Binding the legacy NT handle table onto the cap substrate (Axis 1) ----
 * The bridge's 4096-slot table remains the record store (NT handle values are
 * 0x1000+index), so each record carries the cap slot/token that holds the
 * object's authority. These helpers attach, gate, and detach that authority. */

/* Mint a cap object for an EXISTING NT handle and record the binding.
 * desired_access is the NT access mask the caller was granted at open time.
 * Returns 0 on success (handle is now cap-backed), -1 if the cap substrate is
 * unavailable (caller should keep using the legacy path). */
int  vsl_nt_cap_bind(vsl_nt_bridge_ctx_t *ctx, uint32_t nt_handle,
                      nt_object_type_t type, uint32_t desired_access);

/* Authority gate: is `nt_handle` still authorized for `desired_access`?
 * Returns NT_STATUS_SUCCESS when authorized, NT_STATUS_ACCESS_DENIED when the
 * cap was revoked or lacks the rights, NT_STATUS_INVALID_HANDLE for unknown
 * handles. Unbound (legacy) handles return NT_STATUS_SUCCESS so existing
 * behavior is preserved until every path is migrated. */
/* The cap object's own rights ceiling -- what this handle was minted with,
 * possibly reduced by NtDuplicateObject. The SRM intersects the DACL grant
 * with this, so a permissive DACL can never enlarge a handle's authority. */
uint32_t vsl_nt_cap_granted_mask(vsl_nt_bridge_ctx_t *ctx, uint32_t nt_handle);

/* True if the handle carries a live (unrevoked) cap binding. */
bool vsl_nt_cap_handle_is_bound(vsl_nt_bridge_ctx_t *ctx, uint32_t nt_handle);

uint32_t vsl_nt_cap_authorize(vsl_nt_bridge_ctx_t *ctx, uint32_t nt_handle,
                              uint32_t desired_access);

/* Detach + close the cap handle backing an NT handle (called on NtClose and
 * on shutdown). Safe to call on unbound handles. Returns 0 if a cap was
 * released, -1 otherwise. */
int  vsl_nt_cap_unbind(vsl_nt_bridge_ctx_t *ctx, uint32_t nt_handle);

/* Alias the cap object of `src_handle` onto an already-allocated
 * `dst_handle` (the NtDuplicateObject path) so both share one revocable
 * object. Returns 0 on success, -1 if src is unbound. */
int  vsl_nt_cap_alias(vsl_nt_bridge_ctx_t *ctx, uint32_t src_handle,
                      uint32_t dst_handle);

/* Revoke the object behind an NT handle (the "lose authority" operation).
 * The record stays valid but every cap-backed gate then denies access. */
int  vsl_nt_cap_revoke_nt(vsl_nt_bridge_ctx_t *ctx, uint32_t nt_handle);

#endif /* WUBU_VSL_NT_CAP_H */
