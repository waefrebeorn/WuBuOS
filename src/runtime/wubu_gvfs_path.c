/*
 * wubu_gvfs_path.c -- GVfs: path normalization and resolution.
 *
 * See wubu_gvfs_path.h.  Paths are split on '/' and reassembled from a
 * component stack, so no input string is ever taken on trust.
 */
#include "wubu_gvfs_path.h"

#include <stdio.h>

#include <string.h>

bool wubu_gvfs_path_is_absolute(const char *path)
{
    return path && path[0] == '/';
}

/* Split into components, rejecting any ".." that would rise above the root.
 * Returns false on a traversal. */
static bool split_components(const char *path, char comps[][256],
                             int *n, int max)
{
    *n = 0;
    if (!path) return false;

    const char *p = path;
    while (*p) {
        while (*p == '/') p++;
        if (!*p) break;

        const char *start = p;
        while (*p && *p != '/') p++;
        size_t len = (size_t)(p - start);
        if (len >= 256) return false;             /* a component too long */

        if (len == 1 && start[0] == '.') continue;             /* "." */

        if (len == 2 && start[0] == '.' && start[1] == '.') {
            /* ".." pops one component.  At the root it is a TRAVERSAL and is
             * rejected rather than clamped: clamping would silently hand back
             * a different file than the one named. */
            if (*n == 0) return false;
            (*n)--;
            continue;
        }

        if (*n >= max) return false;              /* too many components */
        memcpy(comps[*n], start, len);
        comps[*n][len] = '\0';
        (*n)++;
    }
    return true;
}

const char *wubu_gvfs_path_normalize(const char *in, char *out, size_t n)
{
    if (!in) return "no path";
    if (!out || n == 0) return "no output buffer";

    char comps[512][256];
    int count = 0;
    if (!split_components(in, comps, &count, 512))
        return "the path escapes the root (a traversal)";

    /* reassemble */
    size_t o = 0;
    if (count == 0) {
        if (n < 2) return "no output buffer";
        out[0] = '/';
        out[1] = '\0';
        return NULL;
    }
    if (n < 2) return "no output buffer";
    out[o++] = '/';
    for (int i = 0; i < count; i++) {
        size_t l = strlen(comps[i]);
        if (o + l + 2 > n) return "no output buffer";
        memcpy(out + o, comps[i], l);
        o += l;
        if (i + 1 < count) out[o++] = '/';
    }
    out[o] = '\0';
    return NULL;
}

const char *wubu_gvfs_path_resolve(const char *base, const char *ref,
                                   char *out, size_t n)
{
    if (!base || !ref) return "no base or reference";

    /* An ABSOLUTE reference disagrees with the base about what the root is, so
     * the caller must choose one rather than having one picked silently. */
    if (wubu_gvfs_path_is_absolute(ref))
        return "an absolute reference cannot be resolved against a base directory";

    char bnorm[1024];
    const char *e = wubu_gvfs_path_normalize(base, bnorm, sizeof(bnorm));
    if (e) return e;

    /* The reference is NOT normalized on its own.  Its ".." is relative to the
     * BASE, not to the root, so "../x" against "/mnt/dir" must be joined first
     * and normalized afterwards -- normalizing it alone rejects every ".."
     * before the base can absorb it. */
    /* The base is a DIRECTORY -- this is a filesystem abstraction, not a URI
     * resolver, so unlike RFC 3986 the base's last component is NOT a document
     * name to be replaced.  "/mnt/dir" + "file" is "/mnt/dir/file", and
     * "/mnt/dir" + "../x" is "/mnt/x". */
    char joined[2048];
    if (strcmp(bnorm, "/") == 0)
        snprintf(joined, sizeof(joined), "/%s", ref);
    else
        snprintf(joined, sizeof(joined), "%s/%s", bnorm, ref);
    return wubu_gvfs_path_normalize(joined, out, n);
}

bool wubu_gvfs_path_is_safe(const char *path)
{
    if (!path) return false;
    char norm[1024];
    return wubu_gvfs_path_normalize(path, norm, sizeof(norm)) == NULL;
}

const char *wubu_gvfs_path_split(const char *path,
                                 char *dir, size_t dn,
                                 char *name, size_t nn)
{
    if (!path) return "no path";
    const char *slash = strrchr(path, '/');
    if (!slash) {
        if (dn) { if (dn < 1) return "no output buffer"; dir[0] = '\0'; }
        if (nn) {
            size_t l = strlen(path);
            if (nn < l + 1) return "no output buffer";
            memcpy(name, path, l + 1);
        }
        return NULL;
    }
    size_t dl = (size_t)(slash - path);
    if (dir && dn) {
        if (dl == 0) {
            if (dn < 2) return "no output buffer";
            dir[0] = '/'; dir[1] = '\0';
        } else {
            if (dn < dl + 1) return "no output buffer";
            memcpy(dir, path, dl);
            dir[dl] = '\0';
        }
    }
    if (name && nn) {
        size_t l = strlen(slash + 1);
        if (nn < l + 1) return "no output buffer";
        memcpy(name, slash + 1, l + 1);
    }
    return NULL;
}
