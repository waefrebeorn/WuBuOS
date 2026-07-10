/* dosgui_explorer_fsops.c -- Explorer file-operation workers.
 *
 * Self-contained module extracted from dosgui_explorer.c: ex_worker_copy /
 * move / delete + ex_handle_file_op dispatcher. Uses the shared g_explorer
 * state + 9P backend via dosgui_explorer_internal.h. Minimal includes.
 */

#include "dosgui_explorer_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>

static void ex_worker_copy(const char *src, const char *dst, uint64_t *copied, uint64_t total) {
    (void)total;
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
    /* Try rename first (same filesystem) */
    if (ex_9p_rename(src, dst) == 0) return;
    
    /* Cross-filesystem: copy then delete */
    uint64_t copied = 0;
    ex_worker_copy(src, dst, &copied, 0);
    ex_9p_unlink(src);
}

static void ex_worker_delete(const char *path, bool permanent) {
    (void)permanent;
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
    /* Use worker functions with 9P/Styx */
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
    dosgui_explorer_refresh();
}
