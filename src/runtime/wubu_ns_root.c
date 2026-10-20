/*
 * wubu_ns_root.c -- per-process /n root view derived from the capability set.
 *
 * See wubu_ns_root.h for the contract and the relationship to the existing
 * owners (wubu_cap.h = the capability model; wubu_ns_fs.c = the on-disk /n
 * tree; wubu_ns_mount.c = the shared bind/union mount table). This module
 * owns THE PROCESS VIEW: which /n subtrees the process may touch, with
 * which rights, inherited how.
 *
 * NAMESPACE #583 (per-process root from the capability set), #599 (explicit
 * cross-container share), #600 (host control-plane hidden in containers),
 * #601 (fork/exec inheritance), #602 (rebase / unshare-equivalent).
 *
 * C11, opaque struct, fail-closed on every boundary.
 */
#include "wubu_ns_root.h"

#include <stdlib.h>
#include <string.h>

/* Opaque view: a name, a class, and the grant table. */
struct ns_grant {
    char     src[WUBU_NS_ROOT_MAX_PATH];
    char     at[WUBU_NS_ROOT_MAX_PATH];
    uint64_t rights;
    unsigned flags;
    int      used;
};

struct wubu_ns_root {
    char name[WUBU_NS_ROOT_MAX_NAME];
    int  root_class;                     /* WUBU_NS_ROOT_HOST/CONTAINER */
    struct ns_grant grants[WUBU_NS_ROOT_MAX_GRANTS];
    int n_grants;
};

/* Handle magic: adoption must fail on forged/zeroed handles. */
#define WUBU_NS_SHARE_MAGIC 0x5348415245313231ULL  /* "SHARE121" */

static size_t slen(const char *s) { return s ? strlen(s) : 0; }

/* Copy a path, fail-closed on overflow or NULL. */
static int path_copy(char dst[WUBU_NS_ROOT_MAX_PATH], const char *src)
{
    if (!dst || !src) return -1;
    size_t n = strlen(src);
    if (n >= WUBU_NS_ROOT_MAX_PATH) return -1;
    memcpy(dst, src, n + 1);
    return 0;
}

wubu_ns_root_t *wubu_ns_root_create(const char *name, int root_class)
{
    if (!name || root_class < 0) return NULL;
    wubu_ns_root_t *r = calloc(1, sizeof(*r));
    if (!r) return NULL;
    size_t n = strlen(name);
    if (n >= sizeof(r->name)) n = sizeof(r->name) - 1;
    memcpy(r->name, name, n);
    r->name[n] = 0;
    r->root_class = root_class;
    return r;
}

void wubu_ns_root_free(wubu_ns_root_t *r) { free(r); }

const char *wubu_ns_root_name(const wubu_ns_root_t *r)
{
    return r ? r->name : NULL;
}

int wubu_ns_root_class(const wubu_ns_root_t *r)
{
    return r ? r->root_class : -1;
}

int wubu_ns_root_grant_count(const wubu_ns_root_t *r)
{
    return r ? r->n_grants : -1;
}

int wubu_ns_root_grant(wubu_ns_root_t *r, const char *src,
                       const char *at, uint64_t rights, unsigned flags)
{
    if (!r || !src || !at || !src[0] || !at[0]) return -1;
    /* Duplicate mount point is rejected: a view binds a subtree at an
     * exact /n path once; stacking is the mount table's union job. */
    for (int i = 0; i < r->n_grants; i++) {
        if (r->grants[i].used && strcmp(r->grants[i].at, at) == 0)
            return -2;
    }
    if (r->n_grants >= WUBU_NS_ROOT_MAX_GRANTS) return -3;
    struct ns_grant *g = &r->grants[r->n_grants];
    if (path_copy(g->src, src) != 0) return -1;
    if (path_copy(g->at, at) != 0) return -1;
    g->rights = rights;
    g->flags  = flags;
    g->used   = 1;
    r->n_grants++;
    return 0;
}

int wubu_ns_root_revoke(wubu_ns_root_t *r, const char *at)
{
    if (!r || !at) return -1;
    for (int i = 0; i < r->n_grants; i++) {
        if (r->grants[i].used && strcmp(r->grants[i].at, at) == 0) {
            /* shift the tail down so the table stays dense */
            for (int j = i; j + 1 < r->n_grants; j++)
                r->grants[j] = r->grants[j + 1];
            memset(&r->grants[r->n_grants - 1], 0,
                   sizeof(r->grants[0]));
            r->n_grants--;
            return 0;
        }
    }
    return -1;   /* no such grant: fail closed */
}

/* Component-aligned prefix match: "/n/a" matches "/n/a" and "/n/a/b"
 * but NOT "/n/ab". Returns the match length or 0. */
static size_t prefix_match(const char *at, const char *path)
{
    size_t la = strlen(at);
    if (strncmp(path, at, la) != 0) return 0;
    if (path[la] == 0) return la;              /* exact */
    if (path[la] == '/') return la;            /* subtree */
    return 0;
}

int wubu_ns_root_lookup(const wubu_ns_root_t *r, const char *path,
                        uint64_t *rights_out,
                        char src_out[WUBU_NS_ROOT_MAX_PATH])
{
    if (!r || !path) return -1;
    int    best = -1;
    size_t best_len = 0;
    for (int i = 0; i < r->n_grants; i++) {
        if (!r->grants[i].used) continue;
        /* #600: a CONTAINER view never sees host control-plane grants. */
        if (r->root_class == WUBU_NS_ROOT_CONTAINER &&
            (r->grants[i].flags & WUBU_NS_GRANT_HOST_CP))
            continue;
        size_t m = prefix_match(r->grants[i].at, path);
        if (m > best_len) { best_len = m; best = i; }
    }
    if (best < 0) return -1;                   /* ungranted: fail closed */
    const char *at   = r->grants[best].at;
    const char *rest = path + strlen(at);
    if (src_out) {
        /* src_out = grant.src + remainder ("/x" or "") */
        size_t n = strlen(r->grants[best].src);
        if (n + slen(rest) >= WUBU_NS_ROOT_MAX_PATH) return -1;
        memcpy(src_out, r->grants[best].src, n);
        memcpy(src_out + n, rest, slen(rest) + 1);
    }
    if (rights_out) *rights_out = r->grants[best].rights;
    return 0;
}

int wubu_ns_root_allows(const wubu_ns_root_t *r, const char *path,
                        uint64_t rights)
{
    uint64_t have = 0;
    if (wubu_ns_root_lookup(r, path, &have, NULL) != 0) return 0;
    return (have & rights) == rights;
}

wubu_ns_root_t *wubu_ns_root_fork(const wubu_ns_root_t *parent)
{
    /* #601: the child inherits a copy of the grant table. The name gets
     * a fork suffix so both views can coexist in diagnostics. */
    if (!parent) return NULL;
    size_t n = strlen(parent->name);
    char child[WUBU_NS_ROOT_MAX_NAME];
    if (n + 2 >= sizeof(child)) n = sizeof(child) - 3;
    memcpy(child, parent->name, n);
    child[n] = '~';
    child[n + 1] = 'f';
    child[n + 2] = 0;
    wubu_ns_root_t *c = wubu_ns_root_create(child, parent->root_class);
    if (!c) return NULL;
    for (int i = 0; i < parent->n_grants; i++) {
        if (parent->grants[i].used &&
            wubu_ns_root_grant(c, parent->grants[i].src,
                               parent->grants[i].at,
                               parent->grants[i].rights,
                               parent->grants[i].flags) != 0) {
            wubu_ns_root_free(c);
            return NULL;
        }
    }
    return c;
}

int wubu_ns_root_exec(wubu_ns_root_t *r)
{
    if (!r) return -1;
    int dropped = 0;
    for (int i = 0; i < r->n_grants; ) {
        if (r->grants[i].used &&
            (r->grants[i].flags & WUBU_NS_GRANT_EPHEMERAL)) {
            for (int j = i; j + 1 < r->n_grants; j++)
                r->grants[j] = r->grants[j + 1];
            memset(&r->grants[r->n_grants - 1], 0,
                   sizeof(r->grants[0]));
            r->n_grants--;
            dropped++;
        } else {
            i++;
        }
    }
    return dropped;
}

int wubu_ns_root_rebase(wubu_ns_root_t *r, const char *new_base)
{
    if (!r || !new_base || !new_base[0]) return -1;
    size_t lb = strlen(new_base);
    for (int i = 0; i < r->n_grants; i++) {
        if (!r->grants[i].used) continue;
        char was[WUBU_NS_ROOT_MAX_PATH];
        memcpy(was, r->grants[i].at, sizeof(was));
        if (lb + strlen(was) >= WUBU_NS_ROOT_MAX_PATH) return -1;
        memcpy(r->grants[i].at, new_base, lb);
        memcpy(r->grants[i].at + lb, was, strlen(was) + 1);
    }
    return 0;
}

wubu_ns_share_t *wubu_ns_root_share(const wubu_ns_root_t *r, const char *at)
{
    if (!r || !at) return NULL;
    for (int i = 0; i < r->n_grants; i++) {
        if (r->grants[i].used && strcmp(r->grants[i].at, at) == 0) {
            wubu_ns_share_t *sh = calloc(1, sizeof(*sh));
            if (!sh) return NULL;
            if (path_copy(sh->src, r->grants[i].src) != 0) {
                free(sh);
                return NULL;
            }
            sh->rights   = r->grants[i].rights;
            sh->issuer_id = (uint64_t)(uintptr_t)r;
            sh->magic    = WUBU_NS_SHARE_MAGIC;
            return sh;
        }
    }
    return NULL;   /* no grant at that path: no handle minted */
}

int wubu_ns_root_adopt(wubu_ns_root_t *r, const wubu_ns_share_t *sh,
                      const char *at)
{
    if (!r || !sh || !at) return -1;
    /* #599: adoption is the ONLY cross-view transfer, and it fails closed
     * on any forged or zeroed handle. The handle's rights cap what the
     * adopting view receives -- the issuer cannot grant more than it had. */
    if (sh->magic != WUBU_NS_SHARE_MAGIC || !sh->src[0]) return -2;
    return wubu_ns_root_grant(r, sh->src, at, sh->rights, 0);
}

void wubu_ns_share_free(wubu_ns_share_t *sh) { free(sh); }
