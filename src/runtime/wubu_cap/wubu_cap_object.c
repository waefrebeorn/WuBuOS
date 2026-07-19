/*
 * wubu_cap_object.c -- WuBuOS capability core: registry + lifecycle + resolver.
 *
 * Ported from GrahaOS kernel/cap/object.c. Differences from the kernel source:
 *   - kmem_cache/kmalloc/kfree       -> malloc/free
 *   - spinlock_t                      -> wubu_cap_lock_acquire/release (pthread shim)
 *   - kpanic/klog/audit_write_*       -> dropped (host has no kernel log ring)
 *   - kind_data payload hooks (chan/vmo/stream deactivate) -> no-ops (WuBuOS
 *     wires its own kind-specific teardown via wubu_cap_set_destroy_hook later)
 *
 * The object layout, token packing, generation-counted revoke, audience set,
 * rights-bitmap checks, and derive/revoke semantics are preserved exactly.
 */
#include "wubu_cap_internal.h"

#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <pthread.h>

/* ---- Globals ---- */
wubu_cap_object_t *g_wubu_cap_ptrs[WUBU_CAP_OBJECT_CAPACITY];
uint32_t           g_wubu_cap_count = 0;
void              *g_wubu_cap_lock;

static void wubu_cap_lock_init_impl(void) {
    /* Lazily create a pthread mutex on first use. */
    if (!g_wubu_cap_lock) {
        pthread_mutex_t *m = (pthread_mutex_t *)malloc(sizeof(pthread_mutex_t));
        if (m) { pthread_mutex_init(m, NULL); g_wubu_cap_lock = m; }
    }
}
void wubu_cap_lock_acquire(void) {
    if (!g_wubu_cap_lock) wubu_cap_lock_init_impl();
    if (g_wubu_cap_lock) pthread_mutex_lock((pthread_mutex_t *)g_wubu_cap_lock);
}
void wubu_cap_lock_release(void) {
    if (g_wubu_cap_lock) pthread_mutex_unlock((pthread_mutex_t *)g_wubu_cap_lock);
}


/* ---- File-local helpers ---- */
static void cap_zero(wubu_cap_object_t *o) { memset(o, 0, sizeof(*o)); }

static uint8_t cap_copy_audience(int32_t *dst, const int32_t *src,
                                 int32_t fallback_pid) {
    if (!src) {
        dst[0] = fallback_pid;
        for (int i = 1; i < WUBU_CAP_AUDIENCE_MAX; i++) dst[i] = WUBU_PID_NONE;
        return 1;
    }
    uint8_t n = 0;
    for (int i = 0; i < WUBU_CAP_AUDIENCE_MAX; i++) {
        if (src[i] == WUBU_PID_NONE) break;
        dst[n++] = src[i];
    }
    for (uint8_t i = n; i < WUBU_CAP_AUDIENCE_MAX; i++) dst[i] = WUBU_PID_NONE;
    return n;
}

static uint32_t cap_find_free_slot_locked(void) {
    for (uint32_t i = 1; i < WUBU_CAP_OBJECT_CAPACITY; i++)
        if (!g_wubu_cap_ptrs[i]) return i;
    return WUBU_CAP_IDX_NONE;
}

static bool audience_is_subset(const wubu_cap_object_t *parent,
                               const int32_t *subset) {
    if (parent->flags & WUBU_CAP_FLAG_PUBLIC) return true;
    if (!subset) return true;
    for (int i = 0; i < WUBU_CAP_AUDIENCE_MAX; i++) {
        int32_t p = subset[i];
        if (p == WUBU_PID_NONE) break;
        if (p == WUBU_PID_PUBLIC) return false;
        bool found = false;
        for (uint8_t j = 0; j < parent->audience_count && j < WUBU_CAP_AUDIENCE_MAX; j++)
            if (parent->audience_set[j] == p) { found = true; break; }
        if (!found) return false;
    }
    return true;
}

/* ---- Init ---- */
void wubu_cap_init(void) {
    wubu_cap_lock_init_impl();
    if (g_wubu_cap_ptrs[0]) return;  /* idempotent */

    wubu_cap_object_t *sentinel = (wubu_cap_object_t *)malloc(sizeof(*sentinel));
    if (!sentinel) return;  /* host OOM; capability core stays uninitialized */
    cap_zero(sentinel);
    sentinel->kind           = WUBU_CAP_KIND_NONE;
    sentinel->generation     = 1;
    sentinel->rights         = WUBU_RIGHTS_ALL;
    sentinel->audience_set[0] = WUBU_PID_PUBLIC;
    for (int i = 1; i < WUBU_CAP_AUDIENCE_MAX; i++) sentinel->audience_set[i] = WUBU_PID_NONE;
    sentinel->audience_count = 1;
    sentinel->flags          = WUBU_CAP_FLAG_IMMORTAL | WUBU_CAP_FLAG_PUBLIC;
    sentinel->parent_idx     = WUBU_CAP_IDX_NONE;
    sentinel->owner_pid      = WUBU_PID_KERNEL;
    sentinel->first_child_idx = WUBU_CAP_IDX_NONE;
    sentinel->next_sibling_idx = WUBU_CAP_IDX_NONE;

    g_wubu_cap_ptrs[0] = sentinel;
    g_wubu_cap_count   = 1;
}

/* ---- Hot-path resolve ---- */
wubu_cap_object_t *wubu_cap_resolve(int32_t calling_pid, wubu_cap_token_t tok,
                                   uint64_t required_rights) {
    if (wubu_cap_token_is_null(tok)) return NULL;
    uint32_t idx = wubu_cap_token_idx(tok);
    if (idx == 0 || idx >= WUBU_CAP_OBJECT_CAPACITY) return NULL;

    wubu_cap_object_t *obj = g_wubu_cap_ptrs[idx];
    if (!obj) return NULL;
    if (obj->deleted) return NULL;
    if (obj->generation != wubu_cap_token_gen(tok)) return NULL;

    if (!(obj->flags & WUBU_CAP_FLAG_PUBLIC)) {
        bool in_audience = false;
        for (uint8_t i = 0; i < obj->audience_count && i < WUBU_CAP_AUDIENCE_MAX; i++)
            if (obj->audience_set[i] == calling_pid) { in_audience = true; break; }
        if (!in_audience) return NULL;
    }
    if ((obj->rights & required_rights) != required_rights) return NULL;
    return obj;
}

/* ---- Lifecycle ---- */
int wubu_cap_create(uint16_t kind, uint64_t rights, const int32_t *audience,
                    uint8_t flags, uintptr_t kind_data, int32_t owner_pid,
                    uint32_t parent_idx, wubu_cap_token_t *out) {
    if (!g_wubu_cap_ptrs[0]) wubu_cap_init();
    if (out) *out = WUBU_CAP_TOKEN_NULL;

    wubu_cap_object_t *obj = (wubu_cap_object_t *)malloc(sizeof(*obj));
    if (!obj) return WUBU_CAP_ENOMEM;
    cap_zero(obj);

    obj->kind          = kind;
    obj->generation    = 1;
    obj->rights        = rights;
    obj->audience_count = cap_copy_audience(obj->audience_set, audience, owner_pid);
    for (uint8_t i = 0; i < obj->audience_count; i++)
        if ((uint32_t)obj->audience_set[i] == (uint32_t)WUBU_PID_PUBLIC) { flags |= WUBU_CAP_FLAG_PUBLIC; break; }
    obj->flags           = flags;
    obj->parent_idx      = parent_idx;
    obj->owner_pid       = owner_pid;
    obj->kind_data       = kind_data;
    obj->first_child_idx = WUBU_CAP_IDX_NONE;
    obj->next_sibling_idx = WUBU_CAP_IDX_NONE;

    wubu_cap_lock_acquire();
    uint32_t slot = cap_find_free_slot_locked();
    if (slot == WUBU_CAP_IDX_NONE) {
        wubu_cap_lock_release();
        free(obj);
        return WUBU_CAP_ENOMEM;
    }
    g_wubu_cap_ptrs[slot] = obj;
    if (slot >= g_wubu_cap_count) g_wubu_cap_count = slot + 1;
    wubu_cap_lock_release();

    if (parent_idx != WUBU_CAP_IDX_NONE && parent_idx < WUBU_CAP_OBJECT_CAPACITY)
        wubu_cap_link_child(parent_idx, slot);

    if (out) *out = wubu_cap_token_pack(1, slot, flags);
    return (int)slot;
}

static int cap_derive_inner(wubu_cap_token_t parent_tok, int32_t caller_pid,
                           uint64_t rights_subset, const int32_t *audience_subset,
                           uint8_t flags_subset, bool allow_audience_expand,
                           wubu_cap_token_t *out) {
    if (out) *out = WUBU_CAP_TOKEN_NULL;
    uint32_t parent_idx = wubu_cap_token_idx(parent_tok);
    if (parent_idx == 0 || parent_idx >= WUBU_CAP_OBJECT_CAPACITY) return WUBU_CAP_EINVAL;

    wubu_cap_object_t *parent = g_wubu_cap_ptrs[parent_idx];
    if (!parent) return WUBU_CAP_EINVAL;
    if (parent->deleted) return WUBU_CAP_EREVOKED;
    if (!wubu_cap_validate_audience(parent, caller_pid)) return WUBU_CAP_EPERM;
    if ((parent->rights & WUBU_RIGHT_DERIVE) == 0) return WUBU_CAP_EPERM;
    if ((parent->rights & rights_subset) != rights_subset) return WUBU_CAP_EPERM;

    const uint8_t PRIV = WUBU_CAP_FLAG_PUBLIC | WUBU_CAP_FLAG_IMMORTAL;
    uint8_t priv_sub = flags_subset & PRIV;
    if ((parent->flags & priv_sub) != priv_sub) return WUBU_CAP_EPERM;
    if (!allow_audience_expand && !audience_is_subset(parent, audience_subset))
        return WUBU_CAP_EPERM;

    const int32_t *aud = audience_subset;
    int32_t def[WUBU_CAP_AUDIENCE_MAX];
    if (!aud) {
        def[0] = caller_pid;
        for (int i = 1; i < WUBU_CAP_AUDIENCE_MAX; i++) def[i] = WUBU_PID_NONE;
        aud = def;
    }
    return wubu_cap_create(parent->kind, rights_subset, aud, flags_subset,
                          parent->kind_data, caller_pid, parent_idx, out);
}

int wubu_cap_derive(wubu_cap_token_t parent, int32_t caller_pid,
                    uint64_t rights_subset, const int32_t *audience_subset,
                    uint8_t flags_subset, wubu_cap_token_t *out) {
    return cap_derive_inner(parent, caller_pid, rights_subset, audience_subset,
                            flags_subset, /*allow_expand=*/false, out);
}

int wubu_cap_revoke(wubu_cap_token_t tok) {
    uint32_t idx = wubu_cap_token_idx(tok);
    if (idx == 0 || idx >= WUBU_CAP_OBJECT_CAPACITY) return WUBU_CAP_EINVAL;

    wubu_cap_lock_acquire();
    wubu_cap_object_t *obj = g_wubu_cap_ptrs[idx];
    if (!obj) { wubu_cap_lock_release(); return WUBU_CAP_EINVAL; }
    if (obj->flags & WUBU_CAP_FLAG_IMMORTAL) { wubu_cap_lock_release(); return WUBU_CAP_EPERM; }
    if (obj->deleted) { wubu_cap_lock_release(); return WUBU_CAP_EREVOKED; }
    obj->deleted = 1;
    obj->generation++;
    wubu_cap_lock_release();

    int count = 1;
    if (obj->flags & WUBU_CAP_FLAG_EAGER_REVOKE) {
        int c = wubu_cap_revoke_cascade(idx);
        if (c > 0) count += c;
    }
    return count;
}

void wubu_cap_destroy(uint32_t idx) {
    if (idx == 0 || idx >= WUBU_CAP_OBJECT_CAPACITY) return;
    wubu_cap_lock_acquire();
    wubu_cap_object_t *obj = g_wubu_cap_ptrs[idx];
    if (obj) g_wubu_cap_ptrs[idx] = NULL;
    wubu_cap_lock_release();
    if (obj) free(obj);
}

int wubu_cap_inspect(wubu_cap_token_t tok, int32_t caller_pid,
                     wubu_cap_inspect_t *out) {
    if (!out) return WUBU_CAP_EFAULT;
    uint32_t idx = wubu_cap_token_idx(tok);
    if (idx == 0 || idx >= WUBU_CAP_OBJECT_CAPACITY) return WUBU_CAP_EINVAL;
    wubu_cap_object_t *obj = g_wubu_cap_ptrs[idx];
    if (!obj) return WUBU_CAP_EINVAL;
    if (obj->deleted) return WUBU_CAP_EREVOKED;
    if (!wubu_cap_validate_audience(obj, caller_pid)) return WUBU_CAP_EPERM;

    out->kind       = obj->kind;
    out->flags      = obj->flags;
    out->reserved1  = 0;
    out->generation = obj->generation;
    out->rights     = obj->rights;
    out->owner_pid  = obj->owner_pid;

    int filled = 0;
    for (uint8_t i = 0; i < obj->audience_count && i < WUBU_CAP_AUDIENCE_MAX; i++) {
        int32_t p = obj->audience_set[i];
        if (p == caller_pid || (uint32_t)p == (uint32_t)WUBU_PID_PUBLIC)
            out->audience[filled++] = p;
    }
    for (int i = filled; i < WUBU_CAP_AUDIENCE_MAX; i++) out->audience[i] = WUBU_PID_NONE;
    for (int i = 0; i < 4; i++) out->reserved2[i] = 0;
    return WUBU_CAP_OK;
}

void wubu_cap_link_child(uint32_t parent_idx, uint32_t child_idx) {
    if (parent_idx == WUBU_CAP_IDX_NONE || parent_idx >= WUBU_CAP_OBJECT_CAPACITY) return;
    if (child_idx == 0 || child_idx >= WUBU_CAP_OBJECT_CAPACITY) return;
    wubu_cap_lock_acquire();
    wubu_cap_object_t *parent = g_wubu_cap_ptrs[parent_idx];
    wubu_cap_object_t *child  = g_wubu_cap_ptrs[child_idx];
    if (parent && child) {
        child->next_sibling_idx = parent->first_child_idx;
        parent->first_child_idx = child_idx;
    }
    wubu_cap_lock_release();
}
