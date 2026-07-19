/*
 * wubu_cap_system.c -- WuBuOS bootcap cascade root (extends wubu_cap).
 *
 * Adopted from GrahaOS kernel/cap/system.c (CAP_KIND_SYSTEM): the bootcap is
 * the cascade root for diminishing-derive. init gets a narrowed sub-token;
 * revoke at the bootcap cascades to all descendants via the eager cascade in
 * wubu_cap_revoke_cascade (which walks first_child/next_sibling).
 *
 * Userspace cannot synthesise CAP_KIND_SYSTEM tokens -- only derive from an
 * existing one via wubu_cap_system_install_to_pid(). This is the system-wide
 * privilege gate (snapshot restore, global transaction scope, etc).
 *
 * Self-contained: includes only wubu_cap_internal.h. No god header.
 */
#include "wubu_cap_internal.h"

#include <string.h>

static uint32_t g_bootcap_idx = WUBU_CAP_IDX_NONE;

void wubu_cap_system_init(void) {
    if (g_bootcap_idx != WUBU_CAP_IDX_NONE) return; /* idempotent */
    int32_t kern[] = { WUBU_PID_KERNEL };
    wubu_cap_token_t boot;
    int r = wubu_cap_create(WUBU_CAP_KIND_SYSTEM, WUBU_RIGHTS_ALL,
                            kern,
                            WUBU_CAP_FLAG_PUBLIC | WUBU_CAP_FLAG_EAGER_REVOKE,
                            0, WUBU_PID_KERNEL,
                            WUBU_CAP_IDX_NONE, &boot);
    if (r <= 0) return;
    g_bootcap_idx = (uint32_t)r; /* wubu_cap_create returns the slot idx */
}

uint32_t wubu_cap_system_bootcap_idx(void) {
    return g_bootcap_idx;
}

int wubu_cap_system_install_to_pid(wubu_cap_handle_table_t *t, int32_t pid,
                                   uint64_t rights_subset) {
    if (!t) return WUBU_CAP_EINVAL;
    if (g_bootcap_idx == WUBU_CAP_IDX_NONE) return WUBU_CAP_EINVAL;
    wubu_cap_object_t *root = g_wubu_cap_ptrs[g_bootcap_idx];
    if (!root || root->deleted) return WUBU_CAP_EREVOKED;
    /* rights must be a subset of the bootcap's rights */
    if ((rights_subset & ~root->rights) != 0) return WUBU_CAP_EPERM;
    int32_t aud[] = { pid };
    wubu_cap_token_t sub;
    int r = wubu_cap_derive(
        wubu_cap_token_pack(root->generation, g_bootcap_idx, 0),
        WUBU_PID_KERNEL, rights_subset, aud, 0, &sub);
    if (r <= 0) return WUBU_CAP_EPERM;
    /* Insert the derived SYSTEM sub-token into the caller's handle table so
     * wubu_cap_system_resolve (table scan) can find it. */
    wubu_cap_token_t htok = wubu_cap_handle_insert(t, (uint32_t)r, 0);
    return wubu_cap_token_is_null(htok) ? WUBU_CAP_ENOMEM : WUBU_CAP_OK;
}

/* Hot-path gate: does `pid`'s handle table hold a live SYSTEM cap covering
 * `required_rights`? Delegates to the handle-table kind scan (which sees the
 * table's private slots). */
int wubu_cap_system_resolve(wubu_cap_handle_table_t *t, int32_t pid,
                            uint64_t required_rights) {
    return wubu_cap_handle_resolve_kind(t, pid,
                                       WUBU_CAP_KIND_SYSTEM, required_rights);
}
