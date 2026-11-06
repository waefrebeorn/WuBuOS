/*
 * wubu_win32k.c -- WuBuOS win32k-lite object model on the wubu_cap authority
 * core. See wubu_win32k.h. Every object is a cap token; every mutation is a
 * wubu_cap_resolve(pid, tok, rights) gate, so a revoked handle denies access
 * while the object record survives. Self-contained C11, no god headers.
 */
#include "wubu_win32k.h"

#include <stdio.h>
#include <string.h>
#include <pthread.h>

/* Per-object rights (subtype-specific). */
#define WK_R_STATION (WUBU_RIGHT_READ | WUBU_RIGHT_WRITE)
#define WK_R_DESKTOP (WUBU_RIGHT_READ | WUBU_RIGHT_WRITE | WUBU_RIGHT_DELETE)
#define WK_R_CLASS   (WUBU_RIGHT_READ | WUBU_RIGHT_WRITE | WUBU_RIGHT_DELETE)
#define WK_R_WINDOW  (WUBU_RIGHT_READ | WUBU_RIGHT_WRITE | WUBU_RIGHT_DELETE)
#define WK_R_GDI     (WUBU_RIGHT_READ | WUBU_RIGHT_WRITE | WUBU_RIGHT_DELETE)

typedef struct wubu_win32k_entry wubu_win32k_entry_t;

struct wubu_win32k_entry {
    bool            used;
    uint16_t        subtype;
    uint32_t        idx;          /* cap object idx (also the registry key) */
    uint32_t        gen;          /* cap generation at create time */
    uint32_t        parent;       /* parent cap idx (WUBU_CAP_IDX_NONE if none) */
    int32_t         owner_pid;
    /* GDI objects: mutable refcount + exclusive-lock ownership + delayed-delete. */
    int32_t         gdi_shared;
    int32_t         gdi_excl_pid;
    bool            gdi_dead;
    char            name[WUBU_WIN32K_NAME_MAX];
    char            class_name[WUBU_WIN32K_NAME_MAX];
    char            title[WUBU_WIN32K_NAME_MAX];
    uintptr_t       wndproc;
    /* message queue (windows only) */
    size_t          q_head, q_tail, q_count;
    wubu_win32k_msg_t q_msgs[WUBU_WIN32K_QUEUE_MAX];
};

static wubu_win32k_entry_t g_ent[WUBU_WIN32K_MAX_OBJ];

/* Serializes all message-queue mutations so any thread can post and the
 * owning thread can wait/get — the WM pump runs in one thread while Windows
 * message producers (input, timers, other agents) post from anywhere. */
static pthread_mutex_t g_q_lock = PTHREAD_MUTEX_INITIALIZER;

static wubu_win32k_entry_t *ent_find(uint16_t subtype, const char *name,
                                     uint32_t parent) {
    for (uint32_t i = 0; i < WUBU_WIN32K_MAX_OBJ; i++)
        if (g_ent[i].used && g_ent[i].subtype == subtype
            && g_ent[i].parent == parent && strcmp(g_ent[i].name, name) == 0)
            return &g_ent[i];
    return NULL;
}

/* Resolve the token -> live entry, auditing rights. NULL on any denial.
 * The cap object is OPAQUE (wubu_cap_resolve only tells us pass/fail); the
 * entry is tied to the cap slot index (g_ent[cap_idx]). */
static wubu_win32k_entry_t *wk_resolve(wubu_cap_token_t tok, int32_t pid,
                                       uint64_t rights) {
    if (wubu_cap_token_is_null(tok)) return NULL;
    uint32_t idx = wubu_cap_token_idx(tok);
    if (idx >= WUBU_WIN32K_MAX_OBJ) return NULL;
    if (!wubu_cap_resolve(pid, tok, rights)) return NULL; /* audience+rights+gen */
    wubu_win32k_entry_t *e = &g_ent[idx];
    if (!e->used) return NULL;
    if (e->gen != wubu_cap_token_gen(tok)) return NULL;   /* stale/revoked */
    return e;
}

static int make_obj(uint16_t kind, uint16_t subtype, const char *name,
                    uint32_t parent, int32_t owner_pid, wubu_cap_token_t *out) {
    int32_t aud[1] = { owner_pid };
    uint64_t rights;
    if (subtype >= 0x10) {
        rights = WK_R_GDI;
    } else {
        rights = (subtype == WUBU_WIN32K_SUB_STATION) ? WK_R_STATION
               : (subtype == WUBU_WIN32K_SUB_DESKTOP) ? WK_R_DESKTOP
               : (subtype == WUBU_WIN32K_SUB_CLASS)   ? WK_R_CLASS
                                                      : WK_R_WINDOW;
    }
    wubu_cap_token_t tok = WUBU_CAP_TOKEN_NULL;
    int r = wubu_cap_create(kind, rights, aud, 0, 0,
                            owner_pid, parent, &tok);
    if (r <= 0) return r;                     /* slot (>0) on success, err else */
    if ((uint32_t)r >= WUBU_WIN32K_MAX_OBJ) { /* win32k registry out of slots */
        wubu_cap_destroy((uint32_t)r);
        return WUBU_CAP_ENOMEM;
    }
    wubu_win32k_entry_t *e = &g_ent[r];
    memset(e, 0, sizeof(*e));
    e->used   = true;
    e->subtype = subtype;
    e->idx    = (uint32_t)r;
    e->gen    = wubu_cap_token_gen(tok);
    e->parent = parent;
    e->owner_pid = owner_pid;
    snprintf(e->name, sizeof(e->name), "%s", name);
    *out = tok;
    return r;
}

/* ---- public API ---- */

void wubu_win32k_init(void) {
    wubu_cap_init();
    memset(g_ent, 0, sizeof(g_ent));
}

int wubu_win32k_station_create(const char *name, int32_t owner_pid,
                               wubu_cap_token_t *out) {
    if (!name || !out) return WUBU_CAP_EINVAL;
    if (ent_find(WUBU_WIN32K_SUB_STATION, name, WUBU_CAP_IDX_NONE)) return WUBU_CAP_EPERM;
    return make_obj(WUBU_CAP_KIND_WINDOW, WUBU_WIN32K_SUB_STATION, name,
                    WUBU_CAP_IDX_NONE, owner_pid, out);
}

int wubu_win32k_desktop_create(const char *name, wubu_cap_token_t station,
                               int32_t caller_pid, wubu_cap_token_t *out) {
    if (!name || !out) return WUBU_CAP_EINVAL;
    wubu_win32k_entry_t *s = wk_resolve(station, caller_pid, WK_R_STATION);
    if (!s) return WUBU_CAP_EPERM;
    if (ent_find(WUBU_WIN32K_SUB_DESKTOP, name, s->idx)) return WUBU_CAP_EPERM;
    return make_obj(WUBU_CAP_KIND_WINDOW, WUBU_WIN32K_SUB_DESKTOP, name,
                    s->idx, caller_pid, out);
}

int wubu_win32k_class_register(wubu_cap_token_t desktop, const char *name,
                               uintptr_t wndproc, int32_t caller_pid) {
    if (!name) return WUBU_CAP_EINVAL;
    wubu_win32k_entry_t *d = wk_resolve(desktop, caller_pid, WK_R_DESKTOP);
    if (!d) return WUBU_CAP_EPERM;
    if (ent_find(WUBU_WIN32K_SUB_CLASS, name, d->idx)) return WUBU_CAP_EPERM;
    wubu_cap_token_t tok;
    int r = make_obj(WUBU_CAP_KIND_WINDOW, WUBU_WIN32K_SUB_CLASS, name,
                     d->idx, caller_pid, &tok);
    if (r <= 0) return r;
    wubu_win32k_entry_t *c = ent_find(WUBU_WIN32K_SUB_CLASS, name, d->idx);
    if (c) c->wndproc = wndproc;
    return WUBU_CAP_OK;
}

int wubu_win32k_class_unregister(wubu_cap_token_t desktop, const char *name,
                                 int32_t caller_pid) {
    if (!name) return WUBU_CAP_EINVAL;
    wubu_win32k_entry_t *d = wk_resolve(desktop, caller_pid, WK_R_DESKTOP);
    if (!d) return WUBU_CAP_EPERM;
    wubu_win32k_entry_t *c = ent_find(WUBU_WIN32K_SUB_CLASS, name, d->idx);
    if (!c) return WUBU_CAP_EBADF;
    wubu_cap_revoke(wubu_cap_token_pack(c->gen, c->idx, 0));
    c->used = false;
    return WUBU_CAP_OK;
}

int wubu_win32k_window_create(wubu_cap_token_t desktop, const char *class_name,
                              const char *title, int32_t owner_pid,
                              wubu_cap_token_t *out) {
    if (!class_name || !out) return WUBU_CAP_EINVAL;
    wubu_win32k_entry_t *d = wk_resolve(desktop, owner_pid, WK_R_DESKTOP);
    if (!d) return WUBU_CAP_EPERM;
    /* A window must name a registered class on this desktop. */
    wubu_win32k_entry_t *c = ent_find(WUBU_WIN32K_SUB_CLASS, class_name, d->idx);
    if (!c) return WUBU_CAP_EBADF;
    wubu_cap_token_t tok;
    int r = make_obj(WUBU_CAP_KIND_WINDOW, WUBU_WIN32K_SUB_WINDOW,
                     title ? title : class_name, d->idx, owner_pid, &tok);
    if (r <= 0) return r;
    wubu_win32k_entry_t *w = ent_find(WUBU_WIN32K_SUB_WINDOW,
                                      title ? title : class_name, d->idx);
    if (w) {
        snprintf(w->class_name, sizeof(w->class_name), "%s", class_name);
        w->wndproc = c->wndproc;
        if (title) snprintf(w->title, sizeof(w->title), "%s", title);
        else snprintf(w->title, sizeof(w->title), "%s", class_name);
    }
    *out = tok;
    return (int)wubu_cap_token_idx(tok);
}

int wubu_win32k_window_destroy(wubu_cap_token_t w, int32_t caller_pid) {
    wubu_win32k_entry_t *e = wk_resolve(w, caller_pid, WK_R_WINDOW);
    if (!e) return WUBU_CAP_EPERM;
    wubu_cap_revoke(wubu_cap_token_pack(e->gen, e->idx, 0));
    e->used = false;
    return WUBU_CAP_OK;
}

int wubu_win32k_post_message(wubu_cap_token_t w, int32_t caller_pid,
                             uint32_t msg, uint64_t wp, uint64_t lp) {
    wubu_win32k_entry_t *e = wk_resolve(w, caller_pid, WUBU_RIGHT_WRITE);
    if (!e || e->subtype != WUBU_WIN32K_SUB_WINDOW) return WUBU_CAP_EPERM;
    pthread_mutex_lock(&g_q_lock);
    if (e->q_count >= WUBU_WIN32K_QUEUE_MAX) { pthread_mutex_unlock(&g_q_lock); return WUBU_CAP_EAGAIN; }
    e->q_msgs[e->q_tail].msg = msg;
    e->q_msgs[e->q_tail].wparam = wp;
    e->q_msgs[e->q_tail].lparam = lp;
    e->q_tail = (e->q_tail + 1) % WUBU_WIN32K_QUEUE_MAX;
    e->q_count++;
    int depth = (int)e->q_count;
    pthread_mutex_unlock(&g_q_lock);
    return depth;
}

int wubu_win32k_peek_message(wubu_cap_token_t w, int32_t caller_pid,
                             wubu_win32k_msg_t *out) {
    if (!out) return WUBU_CAP_EINVAL;
    wubu_win32k_entry_t *e = wk_resolve(w, caller_pid, WUBU_RIGHT_READ);
    if (!e || e->subtype != WUBU_WIN32K_SUB_WINDOW) return WUBU_CAP_EPERM;
    int present;
    pthread_mutex_lock(&g_q_lock);
    if (e->q_count == 0) { present = 0; }
    else { *out = e->q_msgs[e->q_head]; present = 1; }
    pthread_mutex_unlock(&g_q_lock);
    return present;
}

int wubu_win32k_get_message(wubu_cap_token_t w, int32_t caller_pid,
                            wubu_win32k_msg_t *out) {
    if (!out) return WUBU_CAP_EINVAL;
    wubu_win32k_entry_t *e = wk_resolve(w, caller_pid, WUBU_RIGHT_READ);
    if (!e || e->subtype != WUBU_WIN32K_SUB_WINDOW) return WUBU_CAP_EPERM;
    int got;
    pthread_mutex_lock(&g_q_lock);
    if (e->q_count == 0) { got = 0; }
    else {
        *out = e->q_msgs[e->q_head];
        e->q_head = (e->q_head + 1) % WUBU_WIN32K_QUEUE_MAX;
        e->q_count--;
        got = 1;
    }
    pthread_mutex_unlock(&g_q_lock);
    return got;
}

int wubu_win32k_revoke(wubu_cap_token_t tok) {
    return wubu_cap_revoke(tok);
}

static uint32_t gdi_order(uint16_t subtype) {
    switch (subtype) {
    case WUBU_WIN32K_SUB_DC:      return 1;
    case WUBU_WIN32K_SUB_BRUSH:   return 2;
    case WUBU_WIN32K_SUB_PEN:     return 3;
    case WUBU_WIN32K_SUB_BITMAP:  return 4;
    default:                      return 0;
    }
}

static const char *gdi_kind(uint16_t subtype) {
    switch (subtype) {
    case WUBU_WIN32K_SUB_DC:      return "dc";
    case WUBU_WIN32K_SUB_BRUSH:   return "brush";
    case WUBU_WIN32K_SUB_PEN:     return "pen";
    case WUBU_WIN32K_SUB_BITMAP:  return "bitmap";
    default:                      return "gdi";
    }
}

static int count_locks(int32_t pid) {
    int c = 0;
    for (uint32_t i = 0; i < WUBU_WIN32K_MAX_OBJ; i++)
        if (g_ent[i].used && g_ent[i].subtype >= 0x10
            && g_ent[i].gdi_excl_pid == pid)
            c++;
    return c;
}

static wubu_win32k_entry_t *gdi_resolve(wubu_cap_token_t tok, int32_t pid,
                                        uint64_t rights) {
    wubu_win32k_entry_t *e = wk_resolve(tok, pid, rights);
    if (!e || e->subtype < 0x10) return NULL;
    return e;
}

int wubu_win32k_gdi_create(uint32_t subtype, const char *label,
                           int32_t owner_pid, wubu_cap_token_t *out) {
    if (!label || !out) return WUBU_CAP_EINVAL;
    if (gdi_order((uint16_t)subtype) == 0) return WUBU_CAP_EINVAL;
    int r = make_obj(WUBU_CAP_KIND_GDI, (uint16_t)subtype, label,
                     WUBU_CAP_IDX_NONE, owner_pid, out);
    if (r > 0) {
        wubu_win32k_entry_t *e = &g_ent[r];
        e->gdi_shared = 1;
        e->gdi_excl_pid = 0;
        e->gdi_dead = false;
    }
    return r;
}

int wubu_win32k_gdi_addref(wubu_cap_token_t g, int32_t caller_pid) {
    wubu_win32k_entry_t *e = gdi_resolve(g, caller_pid, WUBU_RIGHT_READ);
    if (!e) return WUBU_CAP_EPERM;
    if (e->gdi_dead) return WUBU_CAP_EREVOKED;
    e->gdi_shared++;
    return e->gdi_shared;
}

int wubu_win32k_gdi_deref(wubu_cap_token_t g, int32_t caller_pid) {
    wubu_win32k_entry_t *e = gdi_resolve(g, caller_pid, WUBU_RIGHT_READ);
    if (!e) return WUBU_CAP_EPERM;
    if (e->gdi_shared <= 0) return WUBU_CAP_EINVAL;
    e->gdi_shared--;
    if (e->gdi_shared == 0 && e->gdi_excl_pid == 0) { /* delayed deletion */
        wubu_cap_revoke(wubu_cap_token_pack(e->gen, e->idx, 0));
        e->used = false;
    }
    return e->gdi_shared;
}

int wubu_win32k_gdi_acquire(wubu_cap_token_t g, int32_t caller_pid) {
    wubu_win32k_entry_t *e = gdi_resolve(g, caller_pid, WUBU_RIGHT_WRITE);
    if (!e) return WUBU_CAP_EPERM;
    if (e->gdi_dead) return WUBU_CAP_EREVOKED;
    if (e->gdi_excl_pid == caller_pid) return count_locks(caller_pid);
    uint32_t order = gdi_order(e->subtype);
    /* Ordered acquisition: no LOWER-order GDI object may be locked by another pid. */
    for (uint32_t i = 0; i < WUBU_WIN32K_MAX_OBJ; i++) {
        wubu_win32k_entry_t *x = &g_ent[i];
        if (!x->used || x->subtype < 0x10) continue;
        if (x->subtype != e->subtype && gdi_order(x->subtype) < order
            && x->gdi_excl_pid != 0 && x->gdi_excl_pid != caller_pid)
            return WUBU_CAP_EPERM;
    }
    e->gdi_excl_pid = caller_pid;
    return count_locks(caller_pid);
}

int wubu_win32k_gdi_release(wubu_cap_token_t g, int32_t caller_pid) {
    wubu_win32k_entry_t *e = gdi_resolve(g, caller_pid, WUBU_RIGHT_WRITE);
    if (!e) return WUBU_CAP_EPERM;
    if (e->gdi_excl_pid != caller_pid) return WUBU_CAP_EPERM;
    e->gdi_excl_pid = 0;
    if (e->gdi_dead && e->gdi_shared == 0) { /* delayed deletion complete */
        wubu_cap_revoke(wubu_cap_token_pack(e->gen, e->idx, 0));
        e->used = false;
    }
    return count_locks(caller_pid);
}

int wubu_win32k_gdi_mark_destroy(wubu_cap_token_t g, int32_t caller_pid) {
    wubu_win32k_entry_t *e = gdi_resolve(g, caller_pid, WUBU_RIGHT_DELETE);
    if (!e) return WUBU_CAP_EPERM;
    if (e->gdi_dead) return WUBU_CAP_EREVOKED;
    e->gdi_dead = true;                       /* refuse new refs/locks */
    if (e->gdi_shared == 0 && e->gdi_excl_pid == 0) { /* nothing pins it: free now */
        e->used = false;
        wubu_cap_revoke(wubu_cap_token_pack(e->gen, e->idx, 0));
    }
    /* Token stays resolvable so existing holders can deref the refs down;
     * addref/acquire refuse via gdi_dead. Delayed deletion completes when a
     * deref/release brings shared to 0 while unlocked. */
    return WUBU_CAP_OK;
}

size_t wubu_win32k_snapshot(char *buf, size_t cap) {
    size_t n = 0;
    for (uint32_t i = 0; i < WUBU_WIN32K_MAX_OBJ; i++) {
        if (!g_ent[i].used) continue;
        char line[160];
        int len = 0;
        switch (g_ent[i].subtype) {
        case WUBU_WIN32K_SUB_STATION:
            len = snprintf(line, sizeof(line), "station %s owner=%d\n",
                           g_ent[i].name, g_ent[i].owner_pid);
            break;
        case WUBU_WIN32K_SUB_DESKTOP:
            len = snprintf(line, sizeof(line), "desktop %s parent=%u owner=%d\n",
                           g_ent[i].name, g_ent[i].parent, g_ent[i].owner_pid);
            break;
        case WUBU_WIN32K_SUB_CLASS:
            len = snprintf(line, sizeof(line), "class %s parent=%u\n",
                           g_ent[i].name, g_ent[i].parent);
            break;
        case WUBU_WIN32K_SUB_WINDOW:
            len = snprintf(line, sizeof(line),
                           "window %s class=%s queue=%zu/%u parent=%u\n",
                           g_ent[i].title, g_ent[i].class_name,
                           g_ent[i].q_count, (unsigned)WUBU_WIN32K_QUEUE_MAX,
                           g_ent[i].parent);
            break;
        default:
            if (gdi_order((uint16_t)g_ent[i].subtype) != 0) {
                len = snprintf(line, sizeof(line),
                               "gdi %s %s refs=%d lock=%s%s\n",
                               gdi_kind((uint16_t)g_ent[i].subtype),
                               g_ent[i].name, g_ent[i].gdi_shared,
                               g_ent[i].gdi_excl_pid ? "pid" : "free",
                               g_ent[i].gdi_dead ? ",dead" : "");
            }
            break;
        }
        if (len > 0) {
            int need = (int)(size_t)len + 1;
            if (n + (size_t)len >= cap) break;
            memcpy(buf + n, line, (size_t)len);
            n += (size_t)len;
            buf[n] = '\0'; (void)need;
        }
    }
    if (cap > 0 && n == 0) buf[0] = '\0';
    return n;
}
