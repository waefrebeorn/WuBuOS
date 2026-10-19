/*
 * vsl_nt_ob.c -- NT Object Manager namespace over the /n 9P tree.
 *
 * The four things an NT object path needs that POSIX paths do not:
 *   1. canonicalisation  (\??\C:\Brain\Kv\slot0 == /n/brain/kv/slot0 == /N/BRAIN/KV/SLOT0)
 *   2. an access-mask translation into a 9P open mode, with unmapped bits denied
 *   3. open-by-name that returns a cap-backed handle whose rights are the node's
 *   4. directory enumeration that pages without repeating, and reparse links
 *
 * The node store here is an in-memory tree. That is deliberate: the namespace
 * logic is separable from the wire protocol, and this way it can be tested
 * without a mounted server. The bridge fills the same records from Tstat.
 */
#include "vsl_nt_ob.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

/* 9P open modes (9P2000 §5.2). */
#define P9_OREAD   0
#define P9_OWRITE  1
#define P9_ORDWR   2
#define P9_OEXEC   3
#define P9_ORCLOSE 4

/* NT access bits used by this module. */
#ifndef NT_GENERIC_READ
#define NT_GENERIC_READ      0x80000000u
#define NT_GENERIC_WRITE     0x40000000u
#define NT_GENERIC_EXECUTE   0x20000000u
#define NT_GENERIC_ALL       0x10000000u
#define NT_DELETE            0x00010000u
#define NT_READ_CONTROL      0x00020000u
#define NT_SYNCHRONIZE       0x00100000u
#endif

static void lowercase(char *s)
{
    for (; *s; s++) *s = (char)tolower((unsigned char)*s);
}

int vsl_nt_ob_canon(const char *in, char *out, size_t outsz)
{
    if (!in || !out || outsz < 8) return NT_STATUS_INVALID_PARAMETER;

    /* Strip the Win32 prefixes before doing anything else. */
    const char *p = in;
    if (p[0] == '\\' && p[1] == '\\' && p[2] == '?' && p[3] == '?' && p[4] == '\\') p += 5;
    else if (p[0] == '\\' && p[1] == '?' && p[2] == '?' && p[3] == '\\') p += 4;

    char work[256];
    size_t w = 0;

    /* Drive-letter paths live under the /n root, matching the 9P tree. */
    if (isalpha((unsigned char)p[0]) && p[1] == ':') {
        const char *rest = p + 2;
        while (*rest == '\\' || *rest == '/') rest++;
        w = (size_t)snprintf(work, sizeof work, "/n/");
        for (; *rest && w + 1 < sizeof work; rest++)
            work[w++] = (*rest == '\\') ? '/' : *rest;
        work[w] = '\0';
    } else {
        for (; *p && w + 1 < sizeof work; p++) {
            char c = *p;
            if (c == '\\') c = '/';
            work[w++] = c;
        }
        work[w] = '\0';
    }

    /* Collapse separators, resolve "." and reject ".." that escapes the root. */
    char norm[256];
    size_t n = 0;
    norm[n++] = '/';

    const char *seg = work;
    while (*seg) {
        while (*seg == '/') seg++;
        if (!*seg) break;
        const char *end = seg;
        while (*end && *end != '/') end++;
        size_t len = (size_t)(end - seg);

        if (len == 1 && seg[0] == '.') {
            /* skip */
        } else if (len == 2 && seg[0] == '.' && seg[1] == '.') {
            /* A canonical object path never contains "..": accepting it would
             * let a traversal be silently rebased under /n. */
            return NT_STATUS_OBJECT_PATH_INVALID;
        } else {
            if (n + len + 2 >= sizeof norm) return NT_STATUS_INVALID_PARAMETER;
            if (n > 1) norm[n++] = '/';
            memcpy(norm + n, seg, len);
            n += len;
        }
        seg = end;
    }
    norm[n] = '\0';
    lowercase(norm);

    /* NT object paths are rooted in the object namespace: a bare \Brain\Kv is
     * the same node as /n/brain/kv. Anything already under /n keeps its spelling. */
    if (strncmp(norm, "/n/", 3) != 0 && strcmp(norm, "/n") != 0) {
        char prefixed[256];
        if (strlen(norm) + 3 >= sizeof prefixed) return NT_STATUS_INVALID_PARAMETER;
        snprintf(prefixed, sizeof prefixed, "/n%s", norm);
        memcpy(norm, prefixed, sizeof norm);
        n = strlen(norm);
    }

    if (strncmp(norm, "/n", 2) != 0)
        return NT_STATUS_OBJECT_PATH_INVALID;

    if (n + 1 > outsz) return NT_STATUS_BUFFER_TOO_SMALL;
    memcpy(out, norm, n + 1);
    lowercase(out);
    return NT_STATUS_SUCCESS;
}

int vsl_nt_access_to_9p(uint32_t mask, int *mode, uint32_t *unmapped)
{
    if (!mode || !unmapped) return NT_STATUS_INVALID_PARAMETER;

    const uint32_t known = NT_GENERIC_READ | NT_GENERIC_WRITE | NT_GENERIC_EXECUTE |
                           NT_GENERIC_ALL | NT_DELETE | NT_READ_CONTROL | NT_SYNCHRONIZE;

    uint32_t bad = mask & ~known;
    if (bad) {
        *unmapped = bad;
        return NT_STATUS_ACCESS_DENIED;
    }
    *unmapped = 0;

    if (mask & NT_DELETE) { *mode = P9_ORCLOSE; return NT_STATUS_SUCCESS; }

    int read = (mask & (NT_GENERIC_READ | NT_GENERIC_ALL | NT_READ_CONTROL)) != 0;
    int write = (mask & (NT_GENERIC_WRITE | NT_GENERIC_ALL)) != 0;

    if (read && write) *mode = P9_ORDWR;
    else if (write)    *mode = P9_OWRITE;
    else if (read)     *mode = P9_OREAD;
    else if (mask & NT_SYNCHRONIZE) *mode = P9_OREAD;
    else if (mask & NT_GENERIC_EXECUTE) *mode = P9_OEXEC;
    else { *unmapped = mask ? mask : 1u; return NT_STATUS_ACCESS_DENIED; }

    return NT_STATUS_SUCCESS;
}

/* ---- node store ------------------------------------------------------- */

static ob_node_t *find_node(ob_tree_t *t, const char *canon)
{
    for (int i = 0; i < t->count; i++)
        if (strcmp(t->nodes[i].path, canon) == 0) return &t->nodes[i];
    return NULL;
}

void ob_tree_init(ob_tree_t *t)
{
    if (!t) return;
    memset(t, 0, sizeof *t);
}

int ob_tree_add(ob_tree_t *t, const char *path, uint32_t is_dir, uint64_t rights)
{
    if (!t || !path) return -1;
    char canon[256];
    if (vsl_nt_ob_canon(path, canon, sizeof canon) != NT_STATUS_SUCCESS) return -1;
    if (find_node(t, canon)) return -1;
    if (t->count >= OB_MAX_NODES) return -1;

    ob_node_t *n = &t->nodes[t->count++];
    memset(n, 0, sizeof *n);
    snprintf(n->path, sizeof n->path, "%s", canon);
    n->is_dir = is_dir;
    n->rights = rights;
    n->type = is_dir ? OB_TYPE_DIRECTORY : OB_TYPE_FILE;
    return 0;
}

int ob_tree_symlink(ob_tree_t *t, const char *path, const char *target)
{
    if (!t || !path || !target) return -1;
    char canon[256], tcanon[256];
    if (vsl_nt_ob_canon(path, canon, sizeof canon) != NT_STATUS_SUCCESS) return -1;
    if (vsl_nt_ob_canon(target, tcanon, sizeof tcanon) != NT_STATUS_SUCCESS) return -1;

    ob_node_t *n = find_node(t, canon);
    if (!n) {
        if (t->count >= OB_MAX_NODES) return -1;
        n = &t->nodes[t->count++];
        memset(n, 0, sizeof *n);
        snprintf(n->path, sizeof n->path, "%s", canon);
    }
    n->type = OB_TYPE_SYMLINK;
    n->reparse_tag = OB_REPARSE_SYMLINK;
    n->rights = OB_RIGHT_READ;
    snprintf(n->target, sizeof n->target, "%s", tcanon);
    return 0;
}

/* Rights a request needs, derived from the same mask the bridge sees. */
static uint64_t rights_for_mask(uint32_t mask)
{
    uint64_t r = OB_RIGHT_READ;
    if (mask & (NT_GENERIC_WRITE | NT_GENERIC_ALL | NT_DELETE)) r |= OB_RIGHT_WRITE;
    return r;
}

int vsl_nt_ob_bind_cap(ob_tree_t *t, const char *path, uint64_t cap_rights,
                       int32_t owner_pid, wubu_cap_token_t *out)
{
    if (!t || !path) return NT_STATUS_INVALID_PARAMETER;
    char canon[256];
    if (vsl_nt_ob_canon(path, canon, sizeof canon) != NT_STATUS_SUCCESS)
        return NT_STATUS_OBJECT_PATH_INVALID;
    ob_node_t *n = find_node(t, canon);
    if (!n) return NT_STATUS_OBJECT_NAME_NOT_FOUND;

    int32_t aud[WUBU_CAP_AUDIENCE_MAX];
    for (int i = 0; i < WUBU_CAP_AUDIENCE_MAX; i++) aud[i] = -1;
    aud[0] = owner_pid;

    wubu_cap_token_t tok;
    int r = wubu_cap_create(WUBU_CAP_KIND_STYX_NODE, cap_rights, aud, 0,
                            (uintptr_t)n, owner_pid, WUBU_CAP_IDX_NONE, &tok);
    if (r < 0) return NT_STATUS_NO_MEMORY;
    n->cap = tok;
    n->has_cap = 1;
    if (out) *out = tok;
    return NT_STATUS_SUCCESS;
}

int vsl_nt_ob_open(ob_tree_t *t, const char *name, uint32_t access, int32_t pid,
                   uint32_t *handle, uint64_t *rights)
{
    if (!t || !name || !handle) return NT_STATUS_INVALID_PARAMETER;

    char canon[256];
    int st = vsl_nt_ob_canon(name, canon, sizeof canon);
    if (st != NT_STATUS_SUCCESS) return st;

    /* Canonicalisation already rejected escapes; walk to the target anyway so a
     * symlink resolves to the node it names. */
    ob_node_t *n = find_node(t, canon);
    for (int hops = 0; n && n->type == OB_TYPE_SYMLINK; hops++) {
        if (hops > 8) return NT_STATUS_OBJECT_PATH_INVALID;
        n = find_node(t, n->target);
    }
    if (!n) return NT_STATUS_OBJECT_NAME_NOT_FOUND;

    uint64_t need = rights_for_mask(access);
    if ((n->rights & need) != need) return NT_STATUS_ACCESS_DENIED;

    /* When the node is cap-backed, the capability is the authority, not the
     * node's copied rights field: resolve enforces audience + rights + revoke. */
    if (n->has_cap) {
        uint64_t cap_need = 0;
        if (need & OB_RIGHT_READ)  cap_need |= WUBU_RIGHT_READ;
        if (need & OB_RIGHT_WRITE) cap_need |= WUBU_RIGHT_WRITE;
        if (!wubu_cap_resolve(pid, n->cap, cap_need)) return NT_STATUS_ACCESS_DENIED;

        wubu_cap_inspect_t insp;
        if (wubu_cap_inspect(n->cap, pid, &insp) == 0 && rights) *rights = insp.rights;
    }

    int mode;
    uint32_t unmapped;
    if (vsl_nt_access_to_9p(access, &mode, &unmapped) != NT_STATUS_SUCCESS)
        return NT_STATUS_ACCESS_DENIED;

    n->handle_refs++;
    *handle = (uint32_t)(n - t->nodes) + 1u;
    if (rights) *rights = n->rights;
    return NT_STATUS_SUCCESS;
}

int vsl_nt_ob_resolve(ob_tree_t *t, const char *name, char *out, size_t outsz)
{
    if (!t || !name || !out) return NT_STATUS_INVALID_PARAMETER;

    char canon[256];
    int st = vsl_nt_ob_canon(name, canon, sizeof canon);
    if (st != NT_STATUS_SUCCESS) return st;

    ob_node_t *n = find_node(t, canon);
    for (int hops = 0; n && n->type == OB_TYPE_SYMLINK; hops++) {
        if (hops > 8) return NT_STATUS_OBJECT_PATH_INVALID;
        n = find_node(t, n->target);
    }
    if (!n) return NT_STATUS_OBJECT_NAME_NOT_FOUND;
    if (strlen(n->path) + 1 > outsz) return NT_STATUS_BUFFER_TOO_SMALL;
    snprintf(out, outsz, "%s", n->path);
    return NT_STATUS_SUCCESS;
}

int vsl_nt_ob_mkdir(ob_tree_t *t, const char *name)
{
    if (!t || !name) return NT_STATUS_INVALID_PARAMETER;
    char canon[256];
    if (vsl_nt_ob_canon(name, canon, sizeof canon) != NT_STATUS_SUCCESS)
        return NT_STATUS_OBJECT_PATH_INVALID;
    if (find_node(t, canon)) return NT_STATUS_OBJECT_NAME_COLLISION;
    if (ob_tree_add(t, canon, 1, OB_RIGHT_READ | OB_RIGHT_WRITE) != 0)
        return NT_STATUS_NO_MEMORY;
    return NT_STATUS_SUCCESS;
}

int vsl_nt_ob_query_dir(ob_tree_t *t, const char *dir, int *cursor,
                        char *name, size_t namesz, int *more)
{
    if (!t || !dir || !cursor || !name || !more) return NT_STATUS_INVALID_PARAMETER;

    char canon[256];
    if (vsl_nt_ob_canon(dir, canon, sizeof canon) != NT_STATUS_SUCCESS)
        return NT_STATUS_OBJECT_PATH_INVALID;

    size_t dlen = strlen(canon);
    int idx = *cursor;

    while (idx < t->count) {
        const char *p = t->nodes[idx++].path;
        if (strncmp(p, canon, dlen) == 0 && p[dlen] == '/' &&
            strchr(p + dlen + 1, '/') == NULL) {
            const char *leaf = p + dlen + 1;
            if (strlen(leaf) + 1 > namesz) return NT_STATUS_BUFFER_TOO_SMALL;
            snprintf(name, namesz, "%s", leaf);
            *cursor = idx;
            *more = 0;
            for (int j = idx; j < t->count; j++) {
                const char *q = t->nodes[j].path;
                if (strncmp(q, canon, dlen) == 0 && q[dlen] == '/' &&
                    strchr(q + dlen + 1, '/') == NULL) { *more = 1; break; }
            }
            return NT_STATUS_SUCCESS;
        }
    }
    *more = 0;
    return NT_STATUS_NO_MORE_ENTRIES;
}

int ObOpenByKeyName(ob_tree_t *t, const char *name, uint32_t access, int32_t pid,
                    uint32_t *handle, uint64_t *rights)
{
    return vsl_nt_ob_open(t, name, access, pid, handle, rights);
}

int NtCreateDirectoryObject(ob_tree_t *t, const char *name, int32_t pid,
                            uint32_t *handle, wubu_cap_token_t *cap)
{
    int rc = vsl_nt_ob_mkdir(t, name);
    if (rc != NT_STATUS_SUCCESS) return rc;

    char canon[256];
    if (vsl_nt_ob_canon(name, canon, sizeof canon) != NT_STATUS_SUCCESS)
        return NT_STATUS_OBJECT_PATH_INVALID;

    wubu_cap_token_t tok;
    rc = vsl_nt_ob_bind_cap(t, canon, WUBU_RIGHT_READ | WUBU_RIGHT_WRITE, pid, &tok);
    if (rc != NT_STATUS_SUCCESS) return rc;

    if (handle) {
        /* The handle is the capability's slot, which is what an NT handle is. */
        *handle = wubu_cap_token_idx(tok) + 1u;
    }
    if (cap) *cap = tok;
    return NT_STATUS_SUCCESS;
}

int NtQueryDirectoryObject(ob_tree_t *t, const char *dir, int *cursor, int restart,
                           char *name, size_t namesz, int *more)
{
    if (!cursor) return NT_STATUS_INVALID_PARAMETER;
    if (restart) *cursor = 0;
    return vsl_nt_ob_query_dir(t, dir, cursor, name, namesz, more);
}

int NtQueryDirectoryFile(ob_tree_t *t, const char *dir, int *cursor, int restart,
                         char *name, size_t namesz, int *more)
{
    return NtQueryDirectoryObject(t, dir, cursor, restart, name, namesz, more);
}

int vsl_nt_ob_readlink(ob_tree_t *t, const char *name, char *buf, size_t bufsz)
{
    if (!t || !name || !buf) return NT_STATUS_INVALID_PARAMETER;

    char canon[256];
    if (vsl_nt_ob_canon(name, canon, sizeof canon) != NT_STATUS_SUCCESS)
        return NT_STATUS_OBJECT_PATH_INVALID;

    ob_node_t *n = find_node(t, canon);
    if (!n) return NT_STATUS_OBJECT_NAME_NOT_FOUND;
    if (n->type != OB_TYPE_SYMLINK) return NT_STATUS_INVALID_PARAMETER;
    if (strlen(n->target) + 1 > bufsz) return NT_STATUS_BUFFER_TOO_SMALL;

    snprintf(buf, bufsz, "%s", n->target);
    return NT_STATUS_SUCCESS;
}

int vsl_nt_ob_stat(ob_tree_t *t, const char *name, styx_dir_t *dir)
{
    if (!t || !name || !dir) return NT_STATUS_INVALID_PARAMETER;

    char canon[256];
    if (vsl_nt_ob_canon(name, canon, sizeof canon) != NT_STATUS_SUCCESS)
        return NT_STATUS_OBJECT_PATH_INVALID;

    ob_node_t *orig = find_node(t, canon);
    if (!orig) return NT_STATUS_OBJECT_NAME_NOT_FOUND;

    /* The link node itself is what stat reports (DMSYMLINK); following it is
     * what open does, and the two must not be conflated. */
    ob_node_t *n = orig;
    for (int hops = 0; n && n->type == OB_TYPE_SYMLINK; hops++) {
        if (hops > 8) return NT_STATUS_OBJECT_PATH_INVALID;
        n = find_node(t, n->target);
    }
    if (!n) return NT_STATUS_OBJECT_NAME_NOT_FOUND;

    memset(dir, 0, sizeof *dir);
    dir->qid.path = (uint64_t)(n - t->nodes) + 1u;
    dir->qid.version = 0;
    dir->qid.type = (n->is_dir ? STX_QTDIR : 0);

    if (orig->type == OB_TYPE_SYMLINK) dir->mode = STX_DMSYMLINK | 0777u;
    else if (n->is_dir)                dir->mode = STX_DMDIR | 0755u;
    else                              dir->mode = 0644u;

    const char *leaf = strrchr(canon, '/');
    snprintf(dir->name, sizeof dir->name, "%s", leaf ? leaf + 1 : canon);
    snprintf(dir->uid, sizeof dir->uid, "sys");
    snprintf(dir->gid, sizeof dir->gid, "sys");
    snprintf(dir->muid, sizeof dir->muid, "sys");
    dir->size = 0;
    return NT_STATUS_SUCCESS;
}
