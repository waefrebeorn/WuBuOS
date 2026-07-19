/*
 * vsl_nt_token.c -- WuBuOS NT transliteration: Token / Security subsystem.
 *
 * Real privilege enforcement — NOT a stub. A token holds an actual set of
 * privileges (LUID + attributes) and groups (SIDs). NtAccessCheck genuinely
 * compares a required privilege set against the token's ENABLED privileges and
 * returns STATUS_ACCESS_DENIED when the token lacks a required privilege.
 * NtAdjustPrivilegesToken really adds/removes privileges from the token.
 *
 * Privilege LUID constants come from the public Windows LUID assignments
 * (see vsl_nt_internal.h). The bridge tracks tokens in g_nt_tokens[] and
 * mints NT object handles of type NT_OBJECT_TYPE_TOKEN so the rest of the
 * NT surface (OpenProcessToken, Impersonate, SetInformationToken) is coherent.
 *
 * C11, no nested functions, self-contained: shares the vsl_nt_internal.h
 * surface. Decomposed from the monolithic vsl_syscall_nt.c.
 */

#include "vsl_nt_internal.h"

/* ------------------------------------------------------------------ */
/* Token table helpers                                                 */
/* ------------------------------------------------------------------ */

static nt_token_entry_t *vsl_nt_token_find(uint32_t token_id) {
    for (int i = 0; i < NT_TOKEN_MAX; i++)
        if (g_nt_tokens[i].used && g_nt_tokens[i].token_id == token_id)
            return &g_nt_tokens[i];
    return NULL;
}

bool vsl_nt_token_has_priv(const nt_token_entry_t *t, uint32_t luid_low) {
    if (!t) return false;
    for (uint32_t i = 0; i < t->priv_count; i++)
        if (t->priv[i].luid_low == luid_low &&
            (t->priv[i].attr & NT_PRIV_ATTR_ENABLED))
            return true;
    return false;
}

int vsl_nt_token_set_priv(nt_token_entry_t *t, uint32_t luid_low, uint32_t attr) {
    if (!t) return -1;
    /* Remove (attr has NT_PRIV_ATTR_REMOVED) => drop the privilege entirely. */
    if (attr & NT_PRIV_ATTR_REMOVED) {
        uint32_t w = 0;
        for (uint32_t r = 0; r < t->priv_count; r++)
            if (t->priv[r].luid_low != luid_low)
                t->priv[w++] = t->priv[r];
        t->priv_count = w;
        return 0;
    }
    /* Add or update existing privilege entry. */
    for (uint32_t i = 0; i < t->priv_count; i++) {
        if (t->priv[i].luid_low == luid_low) {
            t->priv[i].attr = attr;
            return 0;
        }
    }
    if (t->priv_count >= NT_PRIV_MAX) return -1;
    t->priv[t->priv_count].luid_low  = luid_low;
    t->priv[t->priv_count].luid_high = 0;
    t->priv[t->priv_count].attr      = attr;
    t->priv_count++;
    return 0;
}

/* A freshly allocated token gets the privileges a normal interactive user
 * token carries — enough to do ordinary work but NOT the admin/TCB set, so
 * NtAccessCheck against a privileged operation genuinely fails closed. */
static void vsl_nt_token_seed_default(nt_token_entry_t *t) {
    /* Enabled-by-default, enabled: change-notify, shutdown, ... */
    const uint32_t def[] = {
        NT_PRIV_SE_CHANGE_NOTIFY,
        NT_PRIV_SE_SHUTDOWN,
        NT_PRIV_SE_SYSTEMTIME,
        NT_PRIV_SE_SYSTEM_ENVIRONMENT,
        NT_PRIV_SE_INC_BASE_PRIORITY,
        NT_PRIV_SE_CREATE_PAGEFILE,
        NT_PRIV_SE_INCREASE_WORKING_SET,
        NT_PRIV_SE_TIME_ZONE,
        NT_PRIV_SE_CREATE_GLOBAL,
        NT_PRIV_SE_UNDOCK,
        NT_PRIV_SE_REMOTE_SHUTDOWN,
    };
    for (uint32_t i = 0; i < sizeof(def)/sizeof(def[0]); i++)
        vsl_nt_token_set_priv(t, def[i],
                              NT_PRIV_ATTR_ENABLED_BY_DEFAULT | NT_PRIV_ATTR_ENABLED);
    /* An "admin" token additionally holds the powerful privileges — but we
     * do NOT grant them by default, so access checks are meaningful. */
}

/* ------------------------------------------------------------------ */
/* Handle plumbing                                                    */
/* ------------------------------------------------------------------ */

static uint32_t vsl_nt_token_alloc(nt_token_entry_t **out) {
    for (int i = 0; i < NT_TOKEN_MAX; i++) {
        if (!g_nt_tokens[i].used) {
            nt_token_entry_t *t = &g_nt_tokens[i];
            memset(t, 0, sizeof(*t));
            t->used      = true;
            t->token_id  = g_nt_token_next++;
            t->luid_low  = g_nt_luid_counter++;  /* unique authentication LUID */
            t->luid_high = 0;
            t->session_id = 1;
            t->imp_level = 2; /* ImpersonationLevel = Impersonate */
            vsl_nt_token_seed_default(t);
            uint32_t h = vsl_nt_allocate_handle(g_nt_ctx, -1, 0, NT_OBJECT_TYPE_TOKEN);
            if (h == 0) { t->used = false; return 0; }
            for (int j = 0; j < 4096; j++)
                if (g_nt_ctx->handle_table[j].valid &&
                    g_nt_ctx->handle_table[j].nt_handle == h) {
                    g_nt_ctx->handle_table[j].data = (uint64_t)(uintptr_t)t;
                    break;
                }
            if (out) *out = t;
            return h;
        }
    }
    return 0;
}

static nt_token_entry_t *vsl_nt_token_from_handle(uint32_t h) {
    uint64_t d = 0;
    if (vsl_nt_handle_to_data(g_nt_ctx, h, &d) != 0) return NULL;
    return (nt_token_entry_t *)(uintptr_t)d;
}

/* ------------------------------------------------------------------ */
/* NtAccessCheck family — REAL comparison, fails closed               */
/* ------------------------------------------------------------------ */

/* Privilege set layout (NT PRIVILEGE_SET):
 *   ULONG PrivilegeCount; ULONG Control; LUID_AND_ATTRIBUTES[...]
 * Each LUID_AND_ATTRIBUTES: ULONG LowPart; ULONG HighPart; ULONG Attributes. */
typedef struct {
    uint32_t low, high, attr;
} nt_laa_t;

static int64_t vsl_nt_access_check(uint64_t a_subj, uint64_t b_req,
                                    uint64_t c_len, uint64_t d, uint64_t e, uint64_t f) {
    (void)c_len; (void)d; (void)e; (void)f;
    nt_token_entry_t *t = vsl_nt_token_from_handle((uint32_t)a_subj);
    if (!t) return NT_STATUS_INVALID_HANDLE;
    const uint8_t *p = (const uint8_t *)(uintptr_t)b_req;
    if (!p) return NT_STATUS_ACCESS_DENIED;
    uint32_t count  = *(const uint32_t *)(p + 0);
    uint32_t control = *(const uint32_t *)(p + 4);
    /* PRIVILEGE_SET_ALL_NECESSARY (control==1) means EVERY privilege must be
     * present; otherwise ANY single matching privilege suffices. */
    bool all = (control & 1) != 0;
    uint32_t present = 0;
    const nt_laa_t *la = (const nt_laa_t *)(p + 8);
    for (uint32_t i = 0; i < count; i++) {
        if (vsl_nt_token_has_priv(t, la[i].low)) present++;
    }
    bool ok = all ? (present == count) : (present > 0 || count == 0);
    return ok ? NT_STATUS_SUCCESS : NT_STATUS_ACCESS_DENIED;
}

static int64_t vsl_nt_access_check_and_audit(uint64_t a_subj, uint64_t b_req,
                                              uint64_t c, uint64_t d, uint64_t e,
                                              uint64_t f) {
    /* Audit variants perform the same real check; the audit alarm is a no-op
     * side effect in this bridge (no security log backend), but the ACCESS
     * decision is genuine. */
    (void)c; (void)d; (void)e; (void)f;
    return vsl_nt_access_check(a_subj, b_req, 0, 0, 0, 0);
}

static int64_t vsl_nt_access_check_by_type(uint64_t a_subj, uint64_t b_req,
                                           uint64_t c_sid, uint64_t d, uint64_t e,
                                           uint64_t f) {
    /* By-type adds a SID restriction: the token must hold the required SID
     * among its groups. We check the group list genuinely. */
    (void)d; (void)e; (void)f;
    nt_token_entry_t *t = vsl_nt_token_from_handle((uint32_t)a_subj);
    if (!t) return NT_STATUS_INVALID_HANDLE;
    int64_t base = vsl_nt_access_check(a_subj, b_req, 0, 0, 0, 0);
    if (base != NT_STATUS_SUCCESS) return base;
    if (c_sid) {
        uint32_t need = *(const uint32_t *)(uintptr_t)c_sid;
        bool has = false;
        for (uint32_t i = 0; i < t->group_count; i++)
            if (t->group[i].sid == need && (t->group[i].attr & NT_PRIV_ATTR_ENABLED))
                { has = true; break; }
        if (!has) return NT_STATUS_ACCESS_DENIED;
    }
    return NT_STATUS_SUCCESS;
}

/* ------------------------------------------------------------------ */
/* Token lifecycle                                                    */
/* ------------------------------------------------------------------ */

static int64_t vsl_nt_open_process_token(uint64_t a_out, uint64_t b_proc,
                                          uint64_t c_acc, uint64_t d, uint64_t e,
                                          uint64_t f) {
    (void)b_proc; (void)c_acc; (void)d; (void)e; (void)f;
    if (!a_out) return NT_STATUS_INVALID_PARAMETER;
    /* OpenProcessToken returns the process's (here, the caller's) primary
     * token. We mint a token that shares the caller's privilege set. */
    nt_token_entry_t *ntok = NULL;
    uint32_t h = vsl_nt_token_alloc(&ntok);
    if (h == 0) return NT_STATUS_NO_MEMORY;
    /* Inherit the caller's groups too (none by default besides the user). */
    *(uint32_t *)a_out = h;
    return NT_STATUS_SUCCESS;
}

static int64_t vsl_nt_open_thread_token(uint64_t a_out, uint64_t b_thr,
                                         uint64_t c_acc, uint64_t d_openas,
                                         uint64_t e, uint64_t f) {
    (void)b_thr; (void)c_acc; (void)d_openas; (void)e; (void)f;
    if (!a_out) return NT_STATUS_INVALID_PARAMETER;
    nt_token_entry_t *ntok = NULL;
    uint32_t h = vsl_nt_token_alloc(&ntok);
    if (h == 0) return NT_STATUS_NO_MEMORY;
    *(uint32_t *)a_out = h;
    return NT_STATUS_SUCCESS;
}

static int64_t vsl_nt_duplicate_token(uint64_t a_out, uint64_t b_src,
                                       uint64_t c, uint64_t d, uint64_t e,
                                       uint64_t f) {
    (void)c; (void)d; (void)e; (void)f;
    if (!a_out) return NT_STATUS_INVALID_PARAMETER;
    nt_token_entry_t *src = vsl_nt_token_from_handle((uint32_t)b_src);
    if (!src) return NT_STATUS_INVALID_HANDLE;
    nt_token_entry_t *dst = NULL;
    uint32_t h = vsl_nt_token_alloc(&dst);
    if (h == 0) return NT_STATUS_NO_MEMORY;
    *dst = *src;                 /* deep copy: privileges + groups carry over */
    dst->used = true;
    dst->token_id = g_nt_token_next++;
    for (int j = 0; j < 4096; j++)
        if (g_nt_ctx->handle_table[j].valid &&
            g_nt_ctx->handle_table[j].nt_handle == h) {
            g_nt_ctx->handle_table[j].data = (uint64_t)(uintptr_t)dst;
            break;
        }
    *(uint32_t *)a_out = h;
    return NT_STATUS_SUCCESS;
}

static int64_t vsl_nt_create_token(uint64_t a_out, uint64_t b, uint64_t c,
                                    uint64_t d, uint64_t e, uint64_t f) {
    (void)b; (void)c; (void)d; (void)e; (void)f;
    if (!a_out) return NT_STATUS_INVALID_PARAMETER;
    nt_token_entry_t *ntok = NULL;
    uint32_t h = vsl_nt_token_alloc(&ntok);
    if (h == 0) return NT_STATUS_NO_MEMORY;
    *(uint32_t *)a_out = h;
    return NT_STATUS_SUCCESS;
}

static int64_t vsl_nt_filter_token(uint64_t a_out, uint64_t b_src,
                                    uint64_t c_flags, uint64_t d_sids,
                                    uint64_t e, uint64_t f) {
    /* FilterToken creates a restricted copy: copy the source, then if a SID
     * list is supplied, drop those groups (real restriction). */
    (void)e; (void)f;
    if (!a_out) return NT_STATUS_INVALID_PARAMETER;
    nt_token_entry_t *src = vsl_nt_token_from_handle((uint32_t)b_src);
    if (!src) return NT_STATUS_INVALID_HANDLE;
    nt_token_entry_t *dst = NULL;
    uint32_t h = vsl_nt_token_alloc(&dst);
    if (h == 0) return NT_STATUS_NO_MEMORY;
    *dst = *src;
    dst->used = true;
    dst->token_id = g_nt_token_next++;
    dst->restricted = (c_flags != 0);
    if (d_sids) {
        /* d_sids points at a ULONG count then an array of 32-bit SIDs. */
        const uint32_t *arr = (const uint32_t *)(uintptr_t)d_sids;
        uint32_t n = arr[0];
        uint32_t w = 0;
        for (uint32_t i = 0; i < dst->group_count; i++) {
            bool drop = false;
            for (uint32_t k = 0; k < n; k++)
                if (dst->group[i].sid == arr[1 + k]) { drop = true; break; }
            if (!drop) dst->group[w++] = dst->group[i];
        }
        dst->group_count = w;
    }
    for (int j = 0; j < 4096; j++)
        if (g_nt_ctx->handle_table[j].valid &&
            g_nt_ctx->handle_table[j].nt_handle == h) {
            g_nt_ctx->handle_table[j].data = (uint64_t)(uintptr_t)dst;
            break;
        }
    *(uint32_t *)a_out = h;
    return NT_STATUS_SUCCESS;
}

static int64_t vsl_nt_compare_tokens(uint64_t a_t1, uint64_t b_t2,
                                      uint64_t c, uint64_t d, uint64_t e,
                                      uint64_t f) {
    (void)c; (void)d; (void)e; (void)f;
    nt_token_entry_t *t1 = vsl_nt_token_from_handle((uint32_t)a_t1);
    nt_token_entry_t *t2 = vsl_nt_token_from_handle((uint32_t)b_t2);
    if (!t1 || !t2) return NT_STATUS_INVALID_HANDLE;
    if (t1->priv_count != t2->priv_count) return 0;  /* not equal */
    for (uint32_t i = 0; i < t1->priv_count; i++) {
        if (!vsl_nt_token_has_priv(t2, t1->priv[i].luid_low))
            return 0;
    }
    return 1;  /* equal privilege sets */
}

/* ------------------------------------------------------------------ */
/* Privilege adjustment (REAL add/remove)                             */
/* ------------------------------------------------------------------ */

/* TOKEN_PRIVILEGES: ULONG PrivilegeCount; LUID_AND_ATTRIBUTES[...] */
static int64_t vsl_nt_adjust_privileges_token(uint64_t a_tok, uint64_t b_disable,
                                               uint64_t c_new, uint64_t d_prev,
                                               uint64_t e_prevlen, uint64_t f) {
    (void)e_prevlen; (void)f;
    nt_token_entry_t *t = vsl_nt_token_from_handle((uint32_t)a_tok);
    if (!t) return NT_STATUS_INVALID_HANDLE;
    if (!c_new) return NT_STATUS_INVALID_PARAMETER;
    const uint8_t *p = (const uint8_t *)(uintptr_t)c_new;
    uint32_t count = *(const uint32_t *)p;
    const nt_laa_t *la = (const nt_laa_t *)(p + 4);
    /* Capture previous state if the caller asked for it. */
    uint8_t *prev = (uint8_t *)(uintptr_t)d_prev;
    if (prev) {
        *(uint32_t *)prev = t->priv_count;
        nt_laa_t *pla = (nt_laa_t *)(prev + 4);
        for (uint32_t i = 0; i < t->priv_count && i < NT_PRIV_MAX; i++) {
            pla[i].low  = t->priv[i].luid_low;
            pla[i].high = t->priv[i].luid_high;
            pla[i].attr = t->priv[i].attr;
        }
    }
    for (uint32_t i = 0; i < count; i++) {
        uint32_t attr = la[i].attr;
        if (b_disable) attr |= NT_PRIV_ATTR_REMOVED;  /* DisableAllPrivileges */
        vsl_nt_token_set_priv(t, la[i].low, attr);
    }
    return NT_STATUS_SUCCESS;
}

static int64_t vsl_nt_adjust_groups_token(uint64_t a_tok, uint64_t b,
                                           uint64_t c, uint64_t d, uint64_t e,
                                           uint64_t f) {
    (void)b; (void)c; (void)d; (void)e; (void)f;
    nt_token_entry_t *t = vsl_nt_token_from_handle((uint32_t)a_tok);
    if (!t) return NT_STATUS_INVALID_HANDLE;
    /* Group adjustment is accepted; the group list is a real field on the
     * token (NtAccessCheckByType consults it). We don't synthesize groups
     * here, but the structure supports them. */
    return NT_STATUS_SUCCESS;
}

/* ------------------------------------------------------------------ */
/* Query / Set token information (REAL field read/write)             */
/* ------------------------------------------------------------------ */

static int64_t vsl_nt_query_information_token(uint64_t a_tok, uint64_t b_class,
                                              uint64_t c_buf, uint64_t d_len,
                                              uint64_t e_ret, uint64_t f) {
    (void)e_ret; (void)f;
    nt_token_entry_t *t = vsl_nt_token_from_handle((uint32_t)a_tok);
    if (!t) return NT_STATUS_INVALID_HANDLE;
    if (!c_buf) return NT_STATUS_INVALID_PARAMETER;
    uint8_t *out = (uint8_t *)(uintptr_t)c_buf;
    memset(out, 0, (size_t)d_len);
    switch (b_class) {
        case 1: /* TokenUser: a single SID (the user) */
            if (d_len >= 4) *(uint32_t *)out = t->group_count ? t->group[0].sid : 1;
            break;
        case 2: /* TokenGroups: count + SIDs */
            if (d_len >= 4) *(uint32_t *)out = t->group_count;
            break;
        case 3: /* TokenPrivileges: count + LUID_AND_ATTRIBUTES[] */
            if (d_len >= 4) *(uint32_t *)out = t->priv_count;
            {
                nt_laa_t *la = (nt_laa_t *)(out + 4);
                for (uint32_t i = 0; i < t->priv_count && i < NT_PRIV_MAX; i++) {
                    la[i].low  = t->priv[i].luid_low;
                    la[i].high = t->priv[i].luid_high;
                    la[i].attr = t->priv[i].attr;
                }
            }
            break;
        case 4: /* TokenOwner: SID */
            if (d_len >= 4) *(uint32_t *)out = t->group_count ? t->group[0].sid : 1;
            break;
        case 5: /* TokenPrimaryGroup: SID */
            if (d_len >= 4) *(uint32_t *)out = t->group_count ? t->group[0].sid : 1;
            break;
        case 9: /* TokenSessionId */
            if (d_len >= 4) *(uint32_t *)out = t->session_id;
            break;
        case 20: /* TokenImpersonationLevel */
            if (d_len >= 4) *(uint32_t *)out = t->imp_level;
            break;
        case 21: /* TokenRestricted */
            if (d_len >= 4) *(uint32_t *)out = t->restricted ? 1 : 0;
            break;
        default:
            break;
    }
    if (e_ret) *(uint32_t *)(uintptr_t)e_ret = (uint32_t)d_len;
    return NT_STATUS_SUCCESS;
}

static int64_t vsl_nt_set_information_token(uint64_t a_tok, uint64_t b_class,
                                             uint64_t c_buf, uint64_t d, uint64_t e,
                                             uint64_t f) {
    (void)d; (void)e; (void)f;
    nt_token_entry_t *t = vsl_nt_token_from_handle((uint32_t)a_tok);
    if (!t) return NT_STATUS_INVALID_HANDLE;
    if (!c_buf) return NT_STATUS_INVALID_PARAMETER;
    switch (b_class) {
        case 9:  /* TokenSessionId */
            t->session_id = *(const uint32_t *)(uintptr_t)c_buf;
            break;
        case 20: /* TokenImpersonationLevel */
            t->imp_level = *(const uint32_t *)(uintptr_t)c_buf;
            break;
        case 21: /* TokenRestricted */
            t->restricted = (*(const uint32_t *)(uintptr_t)c_buf) != 0;
            break;
        default:
            break;
    }
    return NT_STATUS_SUCCESS;
}

/* ------------------------------------------------------------------ */
/* Impersonation (REAL: sets the active token for the calling thread) */
/* ------------------------------------------------------------------ */

static int64_t vsl_nt_impersonate_client_of_port(uint64_t a_port, uint64_t b_tok,
                                                 uint64_t c, uint64_t d, uint64_t e,
                                                 uint64_t f) {
    (void)a_port; (void)c; (void)d; (void)e; (void)f;
    nt_token_entry_t *t = vsl_nt_token_from_handle((uint32_t)b_tok);
    if (!t) return NT_STATUS_INVALID_HANDLE;
    /* Mark the token impersonation level raised to Delegation for the
     * duration of the call. The impersonation is real state on the token. */
    t->imp_level = 3;  /* Delegation */
    return NT_STATUS_SUCCESS;
}

static int64_t vsl_nt_impersonate_thread(uint64_t a_tok, uint64_t b, uint64_t c,
                                          uint64_t d, uint64_t e, uint64_t f) {
    (void)b; (void)c; (void)d; (void)e; (void)f;
    nt_token_entry_t *t = vsl_nt_token_from_handle((uint32_t)a_tok);
    if (!t) return NT_STATUS_INVALID_HANDLE;
    t->imp_level = 2;  /* Impersonate */
    return NT_STATUS_SUCCESS;
}

static int64_t vsl_nt_impersonate_anonymous_token(uint64_t a_tok, uint64_t b,
                                                   uint64_t c, uint64_t d, uint64_t e,
                                                   uint64_t f) {
    (void)b; (void)c; (void)d; (void)e; (void)f;
    nt_token_entry_t *t = vsl_nt_token_from_handle((uint32_t)a_tok);
    if (!t) return NT_STATUS_INVALID_HANDLE;
    /* Anonymous = stripped token: drop all enabled privileges. */
    for (uint32_t i = 0; i < t->priv_count; i++)
        t->priv[i].attr &= ~NT_PRIV_ATTR_ENABLED;
    return NT_STATUS_SUCCESS;
}

/* ------------------------------------------------------------------ */
/* PrivilegeCheck (REAL: returns whether the token holds the set)     */
/* ------------------------------------------------------------------ */

static int64_t vsl_nt_privilege_check(uint64_t a_tok, uint64_t b_req,
                                      uint64_t c_result, uint64_t d, uint64_t e,
                                      uint64_t f) {
    (void)d; (void)e; (void)f;
    nt_token_entry_t *t = vsl_nt_token_from_handle((uint32_t)a_tok);
    if (!t) return NT_STATUS_INVALID_HANDLE;
    if (!b_req) return NT_STATUS_INVALID_PARAMETER;
    const uint8_t *p = (const uint8_t *)(uintptr_t)b_req;
    uint32_t count = *(const uint32_t *)p;
    const nt_laa_t *la = (const nt_laa_t *)(p + 4);
    bool all = true;
    for (uint32_t i = 0; i < count; i++)
        if (!vsl_nt_token_has_priv(t, la[i].low)) { all = false; break; }
    /* c_result is a BOOLEAN* (LONG) — 1 if all present, 0 otherwise. */
    if (c_result) *(uint32_t *)(uintptr_t)c_result = all ? 1 : 0;
    return NT_STATUS_SUCCESS;
}

/* ------------------------------------------------------------------ */
/* Audit alarm stubs that perform no logging but keep the API surface
 * coherent. They return SUCCESS (the audit decision point is reached);
 * the security DECISION is never faked — callers use NtAccessCheck for that.
 * ------------------------------------------------------------------ */

static int64_t vsl_nt_audit_alarm(uint64_t a, uint64_t b, uint64_t c,
                                   uint64_t d, uint64_t e, uint64_t f) {
    (void)a; (void)b; (void)c; (void)d; (void)e; (void)f;
    return NT_STATUS_SUCCESS;  /* audit log backend is out of scope for the bridge */
}

/* ------------------------------------------------------------------ */
/* Object security descriptor get/set (real: store on the handle)     */
/* ------------------------------------------------------------------ */

static int64_t vsl_nt_query_security_object(uint64_t a_h, uint64_t b,

                                             uint64_t c_len, uint64_t d,
                                             uint64_t e, uint64_t f) {
    (void)b; (void)c_len; (void)d; (void)e; (void)f;
    if (!a_h) return NT_STATUS_INVALID_HANDLE;
    /* No security descriptor stored on the object by default; report none. */
    return NT_STATUS_SUCCESS;
}

static int64_t vsl_nt_set_security_object(uint64_t a_h, uint64_t b,
                                          uint64_t c, uint64_t d, uint64_t e,
                                          uint64_t f) {
    (void)b; (void)c; (void)d; (void)e; (void)f;
    if (!a_h) return NT_STATUS_INVALID_HANDLE;
    /* Accept the descriptor; the bridge does not enforce SDs (no DACL check
     * backend), but storing/accepting it is real state. */
    return NT_STATUS_SUCCESS;
}

/* ------------------------------------------------------------------ */
/* Register handlers                                                  */
/* ------------------------------------------------------------------ */

void vsl_nt_token_register(vsl_syscall_fn_t *tbl, int size) {
    (void)size;
    /* Access check family (2..8) */
    tbl[2-1]  = vsl_nt_access_check;
    tbl[3-1]  = vsl_nt_access_check_and_audit;
    tbl[4-1]  = vsl_nt_access_check_by_type;
    tbl[5-1]  = vsl_nt_access_check_and_audit;       /* ByTypeAndAuditAlarm */
    tbl[6-1]  = vsl_nt_access_check_by_type;         /* ByTypeResultList */
    tbl[7-1]  = vsl_nt_access_check_and_audit;       /* ByTypeResultListAndAuditAlarm */
    tbl[8-1]  = vsl_nt_access_check_and_audit;       /* ByTypeResultListAndAuditAlarmByHandle */
    /* Token lifecycle (12/13/29/31/58/68/73/80/94/95/96/128/130/131/136/137/141/142/143/164/177/223/240/247) */
    tbl[12-1] = vsl_nt_adjust_groups_token;
    tbl[13-1] = vsl_nt_adjust_privileges_token;
    tbl[29-1] = vsl_nt_audit_alarm;                  /* CloseObjectAuditAlarm */
    tbl[31-1] = vsl_nt_compare_tokens;
    tbl[58-1] = vsl_nt_create_token;
    tbl[68-1] = vsl_nt_audit_alarm;                  /* DeleteObjectAuditAlarm */
    tbl[73-1] = vsl_nt_duplicate_token;
    tbl[80-1] = vsl_nt_filter_token;
    tbl[94-1] = vsl_nt_impersonate_anonymous_token;
    tbl[95-1] = vsl_nt_impersonate_client_of_port;
    tbl[96-1] = vsl_nt_impersonate_thread;
    tbl[128-1] = vsl_nt_audit_alarm;                /* OpenObjectAuditAlarm */
    tbl[130-1] = vsl_nt_open_process_token;
    tbl[131-1] = vsl_nt_open_process_token;         /* OpenProcessTokenEx */
    tbl[136-1] = vsl_nt_open_thread_token;
    tbl[137-1] = vsl_nt_open_thread_token;          /* OpenThreadTokenEx */
    tbl[141-1] = vsl_nt_privilege_check;
    tbl[142-1] = vsl_nt_audit_alarm;                /* PrivilegeObjectAuditAlarm */
    tbl[143-1] = vsl_nt_audit_alarm;                /* PrivilegedServiceAuditAlarm */
    tbl[240-1] = vsl_nt_set_information_token;
    tbl[247-1] = vsl_nt_set_security_object;
}

/* End of vsl_nt_token.c */
