/* explorer_fsops.c -- Explorer file-operation workers.
 *
 * Two backends, one decision per path:
 *
 *   guest store  - the Styx table Notepad and the Run dialog write into (persisted
 *                  to fs.img).  The user's own files live here, so operations on
 *                  them MUST go through it; a host path instead either fails
 *                  silently or touches an unrelated file.
 *   9P / host    - fallback for real mounts, archives and drive browsing, where a
 *                  host path is the correct interpretation.
 *
 * guest_path() is consulted by every worker, so Copy, Move and Delete can never
 * disagree about which filesystem they are acting on.
 */

#include "../egojar.h"
#include "explorer_internal.h"
#include "../apps/guest_file.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <errno.h>

/* True when this path exists in the guest store (file or folder). */
static bool guest_path(const char *p)
{
    return p && (guest_file_exists(p) || guest_file_is_dir(p));
}

/* -- Guest-store workers -------------------------------------------- */

typedef struct {
    const char *dst;
    uint64_t   *copied;
} GuestCopyCtx;

/* Growable list of child names, for delete-before-enumerate. */
typedef struct {
    char (*names)[256];
    int  count;
    int  cap;
} GuestNameList;

/* Read a whole guest file into memory.  Returns a malloc'd buffer the caller
 * frees, or NULL.  Sizes are capped by the guest's per-file ceiling, so a single
 * allocation is safe without trusting any length from the caller. */
static uint8_t *guest_read_all(const char *name, uint32_t *out_len)
{
    /* Length comes from guest_file_stat: guest_file_read() refuses a NULL buffer,
     * so there is no size-query form -- asking stat first also means a file that
     * grew between the two calls is copied whole rather than truncated. */
    uint32_t len = 0;
    bool is_dir = false;
    if (!guest_file_stat(name, &len, &is_dir) || is_dir) return NULL;

    uint8_t *buf = (uint8_t *)malloc((size_t)len + 1);
    if (!buf) return NULL;
    if (len > 0) {
        uint32_t got = 0;
        if (guest_file_read(name, buf, len, &got) != 0 || got != len) {
            free(buf);                       /* short read: refuse, do not write */
            return NULL;
        }
    }
    buf[len] = '\0';
    if (out_len) *out_len = len;
    return buf;
}

static bool guest_copy_child(const char *name, bool is_dir, uint32_t size,
                             void *user)
{
    (void)size;
    GuestCopyCtx *ctx = (GuestCopyCtx *)user;
    char child_dst[EX_MAX_PATH];
    snprintf(child_dst, sizeof(child_dst), "%s/%s", ctx->dst, name);

    if (is_dir) {
        guest_file_make_dir(child_dst);
    } else {
        uint32_t len = 0;
        uint8_t *body = guest_read_all(name, &len);
        if (!body) return true;              /* unreadable: skip, keep going */
        if (guest_file_write(child_dst, body, len) == 0 && ctx->copied)
            *ctx->copied += len;
        free(body);
    }
    return true;
}

static void ex_guest_copy(const char *src, const char *dst, uint64_t *copied)
{
    if (guest_file_is_dir(src)) {
        guest_file_make_dir(dst);
        GuestCopyCtx ctx = { dst, copied };
        guest_file_children(src, guest_copy_child, &ctx);
        return;
    }
    uint32_t len = 0;
    uint8_t *body = guest_read_all(src, &len);
    if (!body) return;
    if (guest_file_write(dst, body, len) == 0 && copied) *copied += len;
    free(body);
}

/* Append one name to the list, growing it when full. */
static bool guest_collect_child(const char *name, bool is_dir, uint32_t size,
                                void *user)
{
    (void)is_dir; (void)size;
    GuestNameList *l = (GuestNameList *)user;
    if (l->count >= l->cap) {
        int ncap = l->cap ? l->cap * 2 : 16;
        char (*grown)[256] = (char (*)[256])realloc(l->names, (size_t)ncap * 256);
        if (!grown) return false;
        l->names = grown;
        l->cap = ncap;
    }
    strncpy(l->names[l->count], name, 255);
    l->names[l->count][255] = '\0';
    l->count++;
    return true;
}

static void ex_guest_delete(const char *path)
{
    if (!guest_file_is_dir(path)) { guest_file_delete(path); return; }

    /* Depth-first.  Names are collected BEFORE anything is removed: deleting
     * while enumerating would shift entries underneath the walk and silently
     * leave half the folder behind -- the classic half-done recursive delete. */
    GuestNameList list = { NULL, 0, 0 };
    guest_file_children(path, guest_collect_child, &list);
    for (int i = 0; i < list.count; i++) {
        char child[EX_MAX_PATH];
        snprintf(child, sizeof(child), "%s/%s", path, list.names[i]);
        ex_guest_delete(child);
    }
    free(list.names);
    guest_file_rmdir(path);       /* empty now; refuses if anything remains */
}


static void ex_guest_move(const char *src, const char *dst)
{
    if (guest_file_rename(src, dst)) return;
    uint64_t copied = 0;
    ex_guest_copy(src, dst, &copied);
    ex_guest_delete(src);
}

/* -- Host / 9P workers (unchanged backend, now a fallback) ------------ */

static void ex_worker_copy(const char *src, const char *dst, uint64_t *copied, uint64_t total) {
    (void)total;
    if (guest_path(src)) { ex_guest_copy(src, dst, copied); return; }

    struct stat st;
    if (ex_9p_stat(src, &st) != 0) return;
    
    if (S_ISDIR(st.st_mode)) {
        /* Directory copy - recurse */
        struct dirent **entries;
        int count = ex_9p_readdir(src, &entries);
        if (count >= 0) {
            ex_9p_mkdir(dst, st.st_mode & 0777);
            for (int i = 0; i < count; i++) {
                if (strcmp(entries[i]->d_name, ".") == 0 || strcmp(entries[i]->d_name, "..") == 0) {
                    free(entries[i]);
                    continue;
                }
                char child_src[EX_MAX_PATH];
                char child_dst[EX_MAX_PATH];
                snprintf(child_src, sizeof(child_src), "%s/%s", src, entries[i]->d_name);
                snprintf(child_dst, sizeof(child_dst), "%s/%s", dst, entries[i]->d_name);
                ex_worker_copy(child_src, child_dst, copied, 0);
                free(entries[i]);
            }
            free(entries);
        }
    } else {
        /* File copy */
        int fdi = ex_9p_open(src, O_RDONLY);
        int fdo = ex_9p_open(dst, O_WRONLY | O_CREAT | O_TRUNC);
        if (fdi >= 0 && fdo >= 0) {
            char buf[8192];
            ssize_t n;
            while ((n = ex_9p_read(fdi, buf, sizeof(buf))) > 0) {
                ex_9p_write(fdo, buf, n);
                if (copied) *copied += n;
            }
            ex_9p_close(fdi);
            ex_9p_close(fdo);
        } else {
            if (fdi >= 0) ex_9p_close(fdi);
            if (fdo >= 0) ex_9p_close(fdo);
        }
    }
}

static void ex_worker_move(const char *src, const char *dst) {
    if (guest_path(src)) { ex_guest_move(src, dst); return; }

    /* Try rename first (same filesystem) */
    if (ex_9p_rename(src, dst) == 0) return;
    
    /* Cross-filesystem: copy then delete */
    uint64_t copied = 0;
    ex_worker_copy(src, dst, &copied, 0);
    ex_9p_unlink(src);
}

static void ex_worker_delete(const char *path, bool permanent) {
    (void)permanent;
    if (guest_path(path)) { ex_guest_delete(path); return; }

    struct stat st;
    if (ex_9p_stat(path, &st) != 0) return;
    
    if (S_ISDIR(st.st_mode)) {
        /* Recursive directory delete */
        struct dirent **entries;
        int count = ex_9p_readdir(path, &entries);
        if (count >= 0) {
            for (int i = 0; i < count; i++) {
                if (strcmp(entries[i]->d_name, ".") == 0 || strcmp(entries[i]->d_name, "..") == 0) {
                    free(entries[i]);
                    continue;
                }
                char child[EX_MAX_PATH];
                snprintf(child, sizeof(child), "%s/%s", path, entries[i]->d_name);
                ex_worker_delete(child, permanent);
                free(entries[i]);
            }
            free(entries);
        }
        ex_9p_unlink(path);
    } else {
        ex_9p_unlink(path);
    }
}

void ex_handle_file_op(ExExplorerState *ex) {
    for (int i = 0; i < ex->file_op.count; i++) {
        if (ex->file_op.error) break;

        const char *src = ex->file_op.paths[i];
        char dst[EX_MAX_PATH];
        const char *base = strrchr(src, '/');
        base = base ? base + 1 : src;

        snprintf(dst, sizeof(dst), "%s/%s", ex->file_op.dest_path, base);

        if (ex->file_op.type == EX_OP_COPY) {
            uint64_t copied = 0;
            ex_worker_copy(src, dst, &copied, ex->file_op.total_bytes);
            ex->file_op.copied_bytes += copied;
            snprintf(ex->status_text, sizeof(ex->status_text),
                     "Copied: %s", base);
        } else if (ex->file_op.type == EX_OP_MOVE) {
            ex_worker_move(src, dst);
            snprintf(ex->status_text, sizeof(ex->status_text),
                     "Moved: %s", base);
        } else if (ex->file_op.type == EX_OP_DELETE) {
            ex_worker_delete(src, false);
            snprintf(ex->status_text, sizeof(ex->status_text),
                     "Deleted: %s", base);
        }
    }

    ex->file_op.in_progress = false;
    ex->file_op.current_idx = ex->file_op.count;

    /* Refresh view */
    explorer_refresh();
}
