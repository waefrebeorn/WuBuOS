/*
 * wubu_ns_mount.c -- the /n bind/union mount table.
 *
 * See wubu_ns_mount.h for the contract and the owner boundaries
 * (styxfs_mount_t = container-repo mounts inside the 9P server;
 * wubu_ns_root_t = the per-process view; THIS = the shared bind table).
 *
 * NAMESPACE #582, #586, #597.
 */
#include "wubu_ns_mount.h"

#include <stdlib.h>
#include <string.h>
#include <dirent.h>

struct ns_bind {
    char     src[WUBU_NS_MOUNT_MAX_PATH];
    char     at[WUBU_NS_MOUNT_MAX_PATH];
    unsigned flags;
    int      layer;    /* bind order inside a union stack */
    int      used;
};

struct wubu_ns_mount_table {
    struct ns_bind binds[WUBU_NS_MOUNT_MAX_BINDS];
    int n_binds;
};

static size_t slen(const char *s) { return s ? strlen(s) : 0; }

/* Component-aligned prefix match, as in wubu_ns_root.c. */
static size_t prefix_match(const char *at, const char *path)
{
    size_t la = strlen(at);
    if (strncmp(path, at, la) != 0) return 0;
    if (path[la] == 0 || path[la] == '/') return la;
    return 0;
}

wubu_ns_mount_table_t *wubu_ns_mount_table_new(void)
{
    return calloc(1, sizeof(wubu_ns_mount_table_t));
}

void wubu_ns_mount_table_free(wubu_ns_mount_table_t *t) { free(t); }

int wubu_ns_mount_bind_count(const wubu_ns_mount_table_t *t)
{
    return t ? t->n_binds : -1;
}

int wubu_ns_bind(wubu_ns_mount_table_t *t, const char *src,
                 const char *at, unsigned flags)
{
    if (!t || !src || !at || !src[0] || !at[0]) return -1;
    if (strlen(src) >= WUBU_NS_MOUNT_MAX_PATH ||
        strlen(at)  >= WUBU_NS_MOUNT_MAX_PATH) return -1;
    if (t->n_binds >= WUBU_NS_MOUNT_MAX_BINDS) return -3;
    /* Duplicate `at` is legal only as a union stack. */
    int stack = 0;
    for (int i = 0; i < t->n_binds; i++) {
        if (t->binds[i].used && strcmp(t->binds[i].at, at) == 0) {
            if (!(t->binds[i].flags & WUBU_NS_MOUNT_UNION) ||
                !(flags & WUBU_NS_MOUNT_UNION))
                return -2;
            stack++;
        }
    }
    struct ns_bind *b = &t->binds[t->n_binds];
    memcpy(b->src, src, slen(src) + 1);
    memcpy(b->at,  at,  slen(at) + 1);
    b->flags = flags;
    b->layer = stack;
    b->used  = 1;
    t->n_binds++;
    return 0;
}

int wubu_ns_unbind(wubu_ns_mount_table_t *t, const char *at)
{
    if (!t || !at) return -1;
    int removed = 0;
    for (int i = 0; i < t->n_binds; ) {
        if (t->binds[i].used && strcmp(t->binds[i].at, at) == 0) {
            for (int j = i; j + 1 < t->n_binds; j++)
                t->binds[j] = t->binds[j + 1];
            memset(&t->binds[t->n_binds - 1], 0, sizeof(t->binds[0]));
            t->n_binds--;
            removed++;
        } else {
            i++;
        }
    }
    return removed ? removed : -1;
}

/* Longest matching prefix; among a union stack at the same prefix, the
 * FIRST-bound layer wins for path resolution (writes land on the top). */
static int find_layer(wubu_ns_mount_table_t *t, const char *npath,
                      size_t *match_len)
{
    int best = -1;
    size_t best_len = 0;
    int    best_layer = 1 << 30;
    for (int i = 0; i < t->n_binds; i++) {
        if (!t->binds[i].used) continue;
        size_t m = prefix_match(t->binds[i].at, npath);
        if (m == 0) continue;
        if (m > best_len ||
            (m == best_len && t->binds[i].layer < best_layer)) {
            best_len = m;
            best = i;
            best_layer = t->binds[i].layer;
        }
    }
    if (match_len) *match_len = best_len;
    return best;
}

int wubu_ns_resolve(wubu_ns_mount_table_t *t, const char *npath,
                    char src_out[WUBU_NS_MOUNT_MAX_PATH],
                    unsigned *flags_out)
{
    if (!t || !npath) return -1;
    size_t mlen = 0;
    int i = find_layer(t, npath, &mlen);
    if (i < 0) return -1;
    const char *rest = npath + strlen(t->binds[i].at);
    if (src_out) {
        size_t n = strlen(t->binds[i].src);
        if (n + slen(rest) >= WUBU_NS_MOUNT_MAX_PATH) return -1;
        memcpy(src_out, t->binds[i].src, n);
        memcpy(src_out + n, rest, slen(rest) + 1);
    }
    if (flags_out) *flags_out = t->binds[i].flags;
    return 0;
}

int wubu_ns_mount_allows(wubu_ns_mount_table_t *t, const char *npath,
                         uint64_t op_rights)
{
    if (!t || !npath) return 0;
    size_t mlen = 0;
    int i = find_layer(t, npath, &mlen);
    if (i < 0) return 0;                    /* unbound: deny */
    /* EVERY bind covering the path must permit the operation: a single
     * read-only layer anywhere in the stack denies writes (#586). */
    for (int j = 0; j < t->n_binds; j++) {
        if (!t->binds[j].used) continue;
        if (prefix_match(t->binds[j].at, npath) != mlen) continue;
        if (t->binds[j].at[mlen] != 0 && t->binds[j].at[mlen] != '/')
            continue;
        if ((t->binds[j].flags & WUBU_NS_MOUNT_RO) &&
            (op_rights & 0x2ULL /* WUBU_RIGHT_WRITE */))
            return 0;
    }
    return 1;
}

int wubu_ns_mount_readdir(wubu_ns_mount_table_t *t, const char *at,
                          char names[][WUBU_NS_MOUNT_MAX_NAME], int max)
{
    if (!t || !at || !names || max <= 0) return -1;
    int n = 0;
    /* Walk the layers in bind order (layer 0 first: first occurrence of
     * a name wins), merging + deduping across the stack (#597). */
    for (int layer = 0; layer < WUBU_NS_MOUNT_MAX_BINDS; layer++) {
        int any = 0;
        for (int i = 0; i < t->n_binds; i++) {
            if (!t->binds[i].used) continue;
            if (strcmp(t->binds[i].at, at) != 0) continue;
            if (t->binds[i].layer != layer) continue;
            any = 1;
            DIR *d = opendir(t->binds[i].src);
            if (!d) continue;                /* a hole, not an error */
            struct dirent *e;
            while ((e = readdir(d)) != NULL) {
                if (e->d_name[0] == '.' && e->d_name[1] == 0) continue;
                if (e->d_name[0] == '.' && e->d_name[1] == '.' &&
                    e->d_name[2] == 0) continue;
                size_t ln = strlen(e->d_name);
                if (ln >= WUBU_NS_MOUNT_MAX_NAME) continue;
                int dup = 0;
                for (int k = 0; k < n; k++) {
                    if (strcmp(names[k], e->d_name) == 0) { dup = 1; break; }
                }
                if (dup) continue;
                if (n >= max) { closedir(d); return n; }
                memcpy(names[n], e->d_name, ln + 1);
                n++;
            }
            closedir(d);
        }
        if (!any) break;                     /* no layer with this index */
    }
    return n;
}
