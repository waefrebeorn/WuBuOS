/* wubu_snapshot_copy.c -- WuBuOS snapshot manager: recursive fs copy + dir size.
 * Extracted from wubu_snapshot.c (separable leaf). Self-contained: filesystem +
 * ftw + sendfile only. C11, minimal includes.
 */
#define _POSIX_C_SOURCE 200809L
#include "wubu_snapshot.h"
#include "wubu_snapshot_internal.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <ftw.h>
#include <sys/stat.h>
#include <sys/sendfile.h>
#include <dirent.h>
#include <linux/limits.h>

/* Copy context (was local to wubu_snapshot.c) */
typedef struct {
    const char *src_root;
    const char *dst_root;
    uint64_t bytes_copied;
    int files_copied;
    int errors;
} CopyContext;

static CopyContext *g_copy_ctx = NULL;
struct FTW;

static int copy_ftw_callback(const char *fpath, const struct stat *sb,
                             int typeflag, struct FTW *ftwbuf) {
    CopyContext *ctx = g_copy_ctx;
    if (!ctx) return -1;

    /* Compute destination path */
    const char *rel = fpath + strlen(ctx->src_root);
    while (*rel == '/') rel++; /* skip leading slashes */

    char dst_path[WUBU_MAX_PATH];
    snprintf(dst_path, sizeof(dst_path), "%s/%s", ctx->dst_root, rel);

    switch (typeflag) {
    case FTW_D: /* directory */
    case FTW_DP: /* directory (post-order) */
    case FTW_DNR: /* directory not readable */
        /* Create directory with same permissions */
        if (mkdir(dst_path, sb->st_mode & 0777) < 0 && errno != EEXIST) {
            fprintf(stderr, "[wubu_snap] mkdir %s failed: %s\n", dst_path, strerror(errno));
            ctx->errors++;
            return -1;
        }
        /* Preserve timestamps */
        struct timespec times[2] = { {sb->st_atime, 0}, {sb->st_mtime, 0} };
        if (utimensat(AT_FDCWD, dst_path, times, AT_SYMLINK_NOFOLLOW) < 0) {
            /* non-fatal */
        }
        break;

    case FTW_F: /* regular file */
    case FTW_SL: /* symlink */
        {
            /* For symlinks, use readlink + symlink */
            if (typeflag == FTW_SL || typeflag == FTW_SLN) {
                char link_target[WUBU_MAX_PATH];
                ssize_t len = readlink(fpath, link_target, sizeof(link_target) - 1);
                if (len > 0) {
                    link_target[len] = '\0';
                    if (symlink(link_target, dst_path) < 0 && errno != EEXIST) {
                        fprintf(stderr, "[wubu_snap] symlink %s -> %s failed: %s\n", dst_path, link_target, strerror(errno));
                        ctx->errors++;
                        return -1;
                    }
                }
            } else {
                /* Regular file: copy with sendfile for efficiency */
                int src_fd = open(fpath, O_RDONLY);
                if (src_fd < 0) {
                    fprintf(stderr, "[wubu_snap] open %s failed: %s\n", fpath, strerror(errno));
                    ctx->errors++;
                    return -1;
                }
                int dst_fd = open(dst_path, O_WRONLY | O_CREAT | O_TRUNC, sb->st_mode & 0777);
                if (dst_fd < 0) {
                    close(src_fd);
                    fprintf(stderr, "[wubu_snap] create %s failed: %s\n", dst_path, strerror(errno));
                    ctx->errors++;
                    return -1;
                }

                off_t offset = 0;
                ssize_t sent = sendfile(dst_fd, src_fd, &offset, sb->st_size);
                close(src_fd);
                close(dst_fd);

                if (sent != sb->st_size) {
                    fprintf(stderr, "[wubu_snap] sendfile %s incomplete: %zd/%llu\n", fpath, sent, (unsigned long long)sb->st_size);
                    ctx->errors++;
                    return -1;
                }
                ctx->bytes_copied += sent;
                ctx->files_copied++;
            }
            /* Preserve timestamps and metadata */
            struct timespec times[2] = { {sb->st_atime, 0}, {sb->st_mtime, 0} };
            if (utimensat(AT_FDCWD, dst_path, times, AT_SYMLINK_NOFOLLOW) < 0) {
                /* non-fatal */
            }
            /* Preserve ownership (requires root for chown) */
            /* fchownat(AT_FDCWD, dst_path, sb->st_uid, sb->st_gid, AT_SYMLINK_NOFOLLOW); */
        }
        break;

    default:
        break;
    }
    return 0;
}
int copy_tree_nftw(const char *src, const char *dst, uint64_t *out_bytes, int *out_files) {
    if (!src || !dst) return -1;

    /* Ensure destination root exists */
    if (mkdir(dst, 0755) < 0 && errno != EEXIST) {
        return -1;
    }

    CopyContext ctx = {
        .src_root = src,
        .dst_root = dst,
        .bytes_copied = 0,
        .files_copied = 0,
        .errors = 0
    };

    g_copy_ctx = &ctx;

    /* nftw with FTW_PHYS to follow symlinks properly, FTW_DEPTH for post-order */
    int flags = FTW_PHYS | FTW_DEPTH | FTW_MOUNT;
    int rc = nftw(src, copy_ftw_callback, 64, flags);

    g_copy_ctx = NULL;

    if (out_bytes) *out_bytes = ctx.bytes_copied;
    if (out_files) *out_files = ctx.files_copied;

    return (rc == 0 && ctx.errors == 0) ? 0 : -1;
}
uint64_t dir_size(const char *path) {
    /* Real directory size calculation via recursive stat */
    uint64_t total = 0;
    DIR *dir = opendir(path);
    if (!dir) return 0;
    struct dirent *ent;
    char subpath[WUBU_MAX_PATH];
    struct stat st;
    while ((ent = readdir(dir)) != NULL) {
        if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0) continue;
        snprintf(subpath, sizeof(subpath), "%s/%s", path, ent->d_name);
        if (stat(subpath, &st) == 0) {
            if (S_ISDIR(st.st_mode)) total += dir_size(subpath);
            else total += (uint64_t)st.st_size;
        }
    }
    closedir(dir);
    return total;
}
