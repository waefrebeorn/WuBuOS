#include <stdlib.h>
/* explorer_fs.c -- 9P/Styx filesystem backend for the WuBuOS Explorer.
 *
 * Self-contained module extracted from explorer.c. Provides the thin
 * 9P/Styx shim (ex_9p_*) that maps POSIX-style file ops onto the Styx 9P
 * filesystem (styxfs_*). No dependency on the explorer UI state -- only the
 * styxfs API via extern decls. Minimal includes, no god headers.
 */

#include "../egojar.h"
#include <dirent.h>
#include <string.h>
#include <stdint.h>

/* -- 9P/Styx File Operations (replacing local filesystem) ------------- */

int ex_9p_stat(const char *path, struct stat *st) {
    /* Use Styx 9P stat via styxfs */
    extern int styxfs_stat(const char *path, struct stat *st);
    return styxfs_stat(path, st);
}

int ex_9p_mkdir(const char *path, mode_t mode) {
    extern int styxfs_create(const char *path, int mode, int perm);
    return styxfs_create(path, 0x80000000 | 0x10000000, mode); /* DMODE | DMDIR */
}

int ex_9p_unlink(const char *path) {
    extern int styxfs_remove(const char *path);
    return styxfs_remove(path);
}

int ex_9p_rename(const char *oldpath, const char *newpath) {
    extern int styxfs_rename(const char *oldpath, const char *newpath);
    return styxfs_rename(oldpath, newpath);
}

int ex_9p_open(const char *path, int flags) {
    extern int styxfs_open(const char *path, int flags);
    return styxfs_open(path, flags);
}

ssize_t ex_9p_read(int fd, void *buf, size_t count) {
    extern ssize_t styxfs_read(int fd, void *buf, size_t count);
    return styxfs_read(fd, buf, count);
}

ssize_t ex_9p_write(int fd, const void *buf, size_t count) {
    extern ssize_t styxfs_write(int fd, const void *buf, size_t count);
    return styxfs_write(fd, buf, count);
}

/* Read a whole file as bytes, preferring the namespace that the 9P callbacks serve.
 * Weak, so builds without the hosted namespace fall back to open/read. */
int ex_9p_close(int fd);   /* defined below */

int ex_9p_read_file(const char *path, unsigned char **out, size_t *out_len) {
    extern int styxfs_ns_read(const char *path, unsigned char **out, size_t *out_len)
        __attribute__((weak));
    if (styxfs_ns_read && styxfs_ns_read(path, out, out_len) == 0) return 0;

    int fd = ex_9p_open(path, O_RDONLY);
    if (fd < 0 && path && path[0] == '/' && path[1]) fd = ex_9p_open(path + 1, O_RDONLY);
    if (fd < 0) return -1;
    size_t cap = 1u << 16, n = 0;
    unsigned char *buf = malloc(cap);
    while (buf) {
        ssize_t r = ex_9p_read(fd, buf + n, cap - n);
        if (r <= 0) break;
        n += (size_t)r;
        if (n < cap) continue;
        unsigned char *bigger = realloc(buf, cap * 2);
        if (!bigger) break;
        buf = bigger;
        cap *= 2;
    }
    ex_9p_close(fd);
    if (!buf || !n) { free(buf); return -1; }
    *out = buf; *out_len = n;
    return 0;
}

int ex_9p_close(int fd) {
    extern int styxfs_close(int fd);
    return styxfs_close(fd);
}

int ex_9p_readdir(const char *path, struct dirent ***entries) {
    extern int styxfs_readdir(const char *path, struct dirent ***entries);
    return styxfs_readdir(path, entries);
}

DIR *ex_9p_opendir(const char *path) {
    extern DIR *styxfs_opendir(const char *path);
    return styxfs_opendir(path);
}
