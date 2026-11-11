/*
 * wubu_gvfs_mount.c -- GVfs: the mount table and path-to-mount resolution.
 *
 * See wubu_gvfs_mount.h.
 */
#include "wubu_gvfs_mount.h"

#include <string.h>

#include <stdio.h>

#include "wubu_gvfs_path.h"

void wubu_gvfs_mount_table_init(wubu_gvfs_mount_table_t *t)
{
    if (!t) return;
    memset(t, 0, sizeof(*t));
}

/* Is `path` at or below `root`?  Both must be normalized. */
static bool path_within(const char *path, const char *root)
{
    size_t rl = strlen(root);
    if (strncmp(path, root, rl) != 0) return false;
    /* "/mnt" must not match "/mntx" -- the next character must be '/' or the
     * end of the path. */
    if (path[rl] == '\0') return true;
    if (strcmp(root, "/") == 0) return true;      /* the root contains everything */
    return path[rl] == '/';
}

int wubu_gvfs_mount(wubu_gvfs_mount_table_t *t, const char *root,
                    const char *scheme, bool read_only)
{
    if (!t || !root || !scheme) return -1;
    if (t->count >= WUBU_GVFS_MAX_MOUNTS) return -1;

    /* The root must ALREADY be in canonical form, or prefix comparison is
     * ambiguous: "relative/path" silently becoming "/relative/path" means the
     * caller mounted a different path than the one they wrote.  Requiring
     * normalize(root) == root rejects a relative root, any "..", a trailing
     * slash and any duplicate separator in one rule. */
    char norm[WUBU_GVFS_ROOT_MAX];
    if (wubu_gvfs_path_normalize(root, norm, sizeof(norm)) != NULL)
        return -1;
    if (strcmp(norm, root) != 0)
        return -1;
    if (strlen(norm) >= WUBU_GVFS_ROOT_MAX) return -1;
    if (strlen(scheme) >= WUBU_GVFS_SCHEME_MAX) return -1;

    /* a duplicate root is an ERROR, not a replace: replacing a live mount
     * would silently redirect its traffic. */
    for (uint32_t i = 0; i < t->count; i++) {
        if (t->mounts[i].active && strcmp(t->mounts[i].root, norm) == 0)
            return -1;
    }

    wubu_gvfs_mount_t *m = &t->mounts[t->count];
    memset(m, 0, sizeof(*m));
    memcpy(m->root, norm, strlen(norm) + 1);
    memcpy(m->scheme, scheme, strlen(scheme) + 1);
    m->read_only = read_only;
    m->active = true;
    t->count++;
    return 0;
}

int wubu_gvfs_unmount(wubu_gvfs_mount_table_t *t, const char *root)
{
    if (!t || !root) return -1;
    for (uint32_t i = 0; i < t->count; i++) {
        if (t->mounts[i].active && strcmp(t->mounts[i].root, root) == 0) {
            t->mounts[i].active = false;
            return 0;
        }
    }
    return -1;
}

bool wubu_gvfs_mount_contains(const wubu_gvfs_mount_t *m, const char *path)
{
    if (!m || !path || !m->active) return false;
    return path_within(path, m->root);
}

const wubu_gvfs_mount_t *
wubu_gvfs_resolve(const wubu_gvfs_mount_table_t *t, const char *path,
                  char *inner, size_t n)
{
    if (!t || !path) return NULL;

    /* Normalize the path first: a resolver that matched raw strings would let
     * "/mnt/../etc" reach a mount it should not. */
    char norm[WUBU_GVFS_ROOT_MAX];
    if (wubu_gvfs_path_normalize(path, norm, sizeof(norm)) != NULL)
        return NULL;

    /* LONGEST PREFIX WINS.  "/mnt" and "/mnt/smb" both contain "/mnt/smb/x";
     * the deeper mount owns it.  First-match or shortest-prefix would hand the
     * deeper mount's files to the shallower filesystem. */
    const wubu_gvfs_mount_t *best = NULL;
    size_t best_len = 0;
    for (uint32_t i = 0; i < t->count; i++) {
        const wubu_gvfs_mount_t *m = &t->mounts[i];
        if (!m->active) continue;
        if (!path_within(norm, m->root)) continue;
        size_t rl = strlen(m->root);
        if (best == NULL || rl > best_len) {
            best = m;
            best_len = rl;
        }
    }
    if (!best) return NULL;                      /* never default to the root */

    /* the inner path is always rooted, so a filesystem never sees a relative
     * path and can never be talked into escaping its own root */
    if (inner && n) {
        if (strcmp(best->root, "/") == 0) {
            snprintf(inner, n, "%s", norm);
        } else if (strcmp(norm, best->root) == 0) {
            snprintf(inner, n, "/");
        } else {
            snprintf(inner, n, "%s", norm + strlen(best->root));
        }
    }
    return best;
}
