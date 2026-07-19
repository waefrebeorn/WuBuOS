/*
 * wubu_cap_handle.c -- WuBuOS per-process capability handle table.
 *
 * Userspace (or a WuBuOS agent process) never sees object idx directly; it
 * holds a wubu_cap_token_t whose idx is a SLOT in its own handle table. Closing
 * a handle bumps the slot's local generation so a stale token fails resolve.
 * Ported from GrahaOS kernel/cap/handle_table.c (kmalloc/kfree -> malloc/free,
 * spinlock -> pthread shim, fixed 1024 cap for the hosted build).
 */
#include "wubu_cap_internal.h"

#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <pthread.h>

#define WUBU_CAP_HANDLE_INITIAL 16
#define WUBU_CAP_HANDLE_MAX     1024
#define WUBU_CAP_HANDLE_NONE    0xFFFFFFFFu

typedef struct wubu_cap_hentry {
    uint32_t object_idx;
    uint32_t local_gen;
    uint8_t  token_flags;
    uint8_t  reserved[3];
    uint32_t next_free;
} wubu_cap_hentry_t;

struct wubu_cap_handle_table {
    wubu_cap_hentry_t *entries;
    uint32_t           capacity;
    uint32_t           count;
    uint32_t           next_free;
    pthread_mutex_t    lock;
};

static void rebuild_free(wubu_cap_hentry_t *e, uint32_t start, uint32_t end,
                         uint32_t *head) {
    for (uint32_t i = end; i-- > start; ) {
        e[i].object_idx = WUBU_CAP_IDX_NONE;
        e[i].local_gen  = 1;
        e[i].token_flags = 0;
        e[i].reserved[0] = e[i].reserved[1] = e[i].reserved[2] = 0;
        e[i].next_free   = *head;
        *head = i;
    }
}

wubu_cap_handle_table_t *wubu_cap_handle_table_create(void) {
    wubu_cap_handle_table_t *t = (wubu_cap_handle_table_t *)malloc(sizeof(*t));
    if (!t) return NULL;
    t->capacity  = WUBU_CAP_HANDLE_INITIAL;
    t->count     = 0;
    t->next_free = WUBU_CAP_HANDLE_NONE;
    pthread_mutex_init(&t->lock, NULL);
    t->entries = (wubu_cap_hentry_t *)malloc((size_t)t->capacity * sizeof(*t->entries));
    if (!t->entries) { free(t); return NULL; }
    /* Slot 0 is reserved as the null-token sentinel (like the object
     * registry): never hand it out, so a token with idx==0 is always null. */
    t->entries[0].object_idx = WUBU_CAP_IDX_NONE;
    t->entries[0].next_free   = WUBU_CAP_HANDLE_NONE;
    rebuild_free(t->entries, 1, t->capacity, &t->next_free);
    return t;
}

void wubu_cap_handle_table_free(wubu_cap_handle_table_t *t) {
    if (!t) return;
    pthread_mutex_lock(&t->lock);
    if (t->entries) { free(t->entries); t->entries = NULL; }
    t->capacity = t->count = 0; t->next_free = WUBU_CAP_HANDLE_NONE;
    pthread_mutex_unlock(&t->lock);
    pthread_mutex_destroy(&t->lock);
    free(t);
}

static int grow(wubu_cap_handle_table_t *t) {
    if (t->capacity >= WUBU_CAP_HANDLE_MAX) return WUBU_CAP_ENOMEM;
    uint32_t new_cap = t->capacity * 2;
    if (new_cap > WUBU_CAP_HANDLE_MAX) new_cap = WUBU_CAP_HANDLE_MAX;
    wubu_cap_hentry_t *ne = (wubu_cap_hentry_t *)malloc((size_t)new_cap * sizeof(*ne));
    if (!ne) return WUBU_CAP_ENOMEM;
    memcpy(ne, t->entries, (size_t)t->capacity * sizeof(*ne));
    uint32_t head = t->next_free;
    rebuild_free(ne, t->capacity, new_cap, &head);
    free(t->entries);
    t->entries = ne; t->capacity = new_cap; t->next_free = head;
    return WUBU_CAP_OK;
}

wubu_cap_token_t wubu_cap_handle_insert(wubu_cap_handle_table_t *t,
                                       uint32_t object_idx, uint8_t token_flags) {
    wubu_cap_token_t null = WUBU_CAP_TOKEN_NULL;
    if (!t || object_idx == WUBU_CAP_IDX_NONE) return null;
    pthread_mutex_lock(&t->lock);
    if (t->next_free == WUBU_CAP_HANDLE_NONE) {
        if (grow(t) != WUBU_CAP_OK) { pthread_mutex_unlock(&t->lock); return null; }
    }
    uint32_t slot = t->next_free;
    t->next_free = t->entries[slot].next_free;
    t->entries[slot].object_idx  = object_idx;
    t->entries[slot].token_flags = token_flags;
    t->entries[slot].next_free   = WUBU_CAP_HANDLE_NONE;
    t->count++;
    uint32_t local_gen = t->entries[slot].local_gen;
    pthread_mutex_unlock(&t->lock);
    return wubu_cap_token_pack(local_gen, slot, token_flags);
}

wubu_cap_object_t *wubu_cap_handle_resolve(wubu_cap_handle_table_t *t,
                                          int32_t caller_pid,
                                          wubu_cap_token_t tok,
                                          uint64_t required_rights) {
    if (!t) return NULL;
    uint32_t slot = wubu_cap_token_idx(tok);
    pthread_mutex_lock(&t->lock);
    if (slot >= t->capacity) { pthread_mutex_unlock(&t->lock); return NULL; }
    wubu_cap_hentry_t *e = &t->entries[slot];
    if (e->object_idx == WUBU_CAP_IDX_NONE) { pthread_mutex_unlock(&t->lock); return NULL; }
    if (e->local_gen != wubu_cap_token_gen(tok)) { pthread_mutex_unlock(&t->lock); return NULL; }
    uint32_t object_idx = e->object_idx;
    uint8_t flags = e->token_flags;
    pthread_mutex_unlock(&t->lock);
    /* Re-pack with the OBJECT idx + object gen so wubu_cap_resolve does the
     * audience/rights check against the registry. flags travel unchanged. */
    wubu_cap_token_t objtok = wubu_cap_token_pack(
        g_wubu_cap_ptrs[object_idx] ? g_wubu_cap_ptrs[object_idx]->generation : 0,
        object_idx, flags);
    return wubu_cap_resolve(caller_pid, objtok, required_rights);
}

int wubu_cap_handle_close(wubu_cap_handle_table_t *t, wubu_cap_token_t tok) {
    if (!t) return WUBU_CAP_EFAULT;
    uint32_t slot = wubu_cap_token_idx(tok);
    pthread_mutex_lock(&t->lock);
    if (slot >= t->capacity) { pthread_mutex_unlock(&t->lock); return WUBU_CAP_EINVAL; }
    wubu_cap_hentry_t *e = &t->entries[slot];
    if (e->object_idx == WUBU_CAP_IDX_NONE) { pthread_mutex_unlock(&t->lock); return WUBU_CAP_EINVAL; }
    e->object_idx = WUBU_CAP_IDX_NONE;
    e->local_gen++;
    e->token_flags = 0;
    e->next_free = t->next_free;
    t->next_free = slot;
    t->count--;
    pthread_mutex_unlock(&t->lock);
    return WUBU_CAP_OK;
}

/* Scan the table for any live handle whose underlying object is of `kind`
 * and whose rights cover `required_rights` (and is not revoked). Returns
 * WUBU_CAP_OK on a match, WUBU_CAP_EPERM otherwise. Used for capability-kind
 * gates (e.g. CAP_KIND_SYSTEM) without needing the caller to hold a specific
 * token -- the table itself is the authority. */
int wubu_cap_handle_resolve_kind(wubu_cap_handle_table_t *t, int32_t pid,
                                 uint16_t kind, uint64_t required_rights) {
    if (!t) return WUBU_CAP_EPERM;
    pthread_mutex_lock(&t->lock);
    for (uint32_t slot = 1; slot < t->capacity; slot++) {
        wubu_cap_hentry_t *e = &t->entries[slot];
        if (e->object_idx == WUBU_CAP_IDX_NONE) continue;
        wubu_cap_object_t *obj = g_wubu_cap_ptrs[e->object_idx];
        if (!obj || obj->deleted) continue;
        if (obj->kind != kind) continue;
        /* Re-pack with object gen so wubu_cap_resolve does the audience +
         * rights + revoke-generation check against the registry. */
        wubu_cap_token_t otok = wubu_cap_token_pack(obj->generation,
                                                    e->object_idx, e->token_flags);
        pthread_mutex_unlock(&t->lock);
        wubu_cap_object_t *r = wubu_cap_resolve(pid, otok, required_rights);
        return (r != NULL) ? WUBU_CAP_OK : WUBU_CAP_EPERM;
    }
    pthread_mutex_unlock(&t->lock);
    return WUBU_CAP_EPERM;
}
