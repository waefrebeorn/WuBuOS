/*
 * dosgui_explorer_test_stub.c  --  Stub functions for dosgui_explorer tests
 *
 * Provides minimal implementations of external dependencies
 * so the explorer test can compile and run without the full WM/Kernel.
 */

#include "dosgui_explorer.h"
#include "dosgui_wm.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>

/* S_IFDIR may not be defined without _POSIX_C_SOURCE */
#ifndef S_IFDIR
#define S_IFDIR 0040000
#endif

/* -- DosGui WM Stubs ---------------------------------------------- */

int dosgui_taskbar_height(void) { return 28; }

void dosgui_taskbar_render(uint32_t *fb, int fb_w, int fb_h) {}

/* -- StyxFS stubs (9P filesystem) ---------------------------------- */

int styxfs_stat(const char *path, struct stat *st) {
    if (!path || !st) return -1;
    memset(st, 0, sizeof(struct stat));
    st->st_mode = S_IFDIR | 0755;
    st->st_size = 4096;
    st->st_mtime = time(NULL);
    st->st_ctime = time(NULL);
    return 0;
}

int styxfs_create(const char *path, int mode, int perm) {
    (void)path; (void)mode; (void)perm;
    return 0;
}

int styxfs_remove(const char *path) {
    (void)path;
    return 0;
}

int styxfs_rename(const char *oldpath, const char *newpath) {
    (void)oldpath; (void)newpath;
    return 0;
}

int styxfs_open(const char *path, int flags) {
    (void)path; (void)flags;
    return 42; /* fake fd */
}

ssize_t styxfs_read(int fd, void *buf, size_t count) {
    (void)fd; (void)buf; (void)count;
    return 0;
}

ssize_t styxfs_write(int fd, const void *buf, size_t count) {
    (void)fd; (void)buf; (void)count;
    return count;
}

int styxfs_close(int fd) {
    (void)fd;
    return 0;
}

int styxfs_readdir(const char *path, struct dirent ***entries) {
    (void)path;
    if (entries) *entries = NULL;
    return 0;
}

DIR *styxfs_opendir(const char *path) {
    (void)path;
    return NULL;
}

/* -- Styx stubs (legacy, for compatibility) --- */

void *styx_open(const char *path, int flags) { return NULL; }
int styx_read(void *fh, void *buf, size_t count) { return 0; }
int styx_write(void *fh, const void *buf, size_t count) { return 0; }
int styx_close(void *fh) { return 0; }
int styx_seek(void *fh, long offset, int whence) { return 0; }
int styx_stat(const char *path, void *stbuf) { return -1; }
int styx_readdir(void *fh, void *dirent) { return 0; }
int styx_mkdir(const char *path, int mode) { return 0; }
int styx_unlink(const char *path) { return 0; }
int styx_rmdir(const char *path) { return 0; }
int styx_rename(const char *oldpath, const char *newpath) { return 0; }

/* -- DosGui WM Stubs for window management ------------------------ */

DosGuiWindow *dosgui_wm_find_by_id(int id) { return NULL; }

DosGuiWindow *dosgui_wm_create(int x, int y, int w, int h, const char *title) { return NULL; }

void dosgui_wm_destroy(DosGuiWindow *win) {}
