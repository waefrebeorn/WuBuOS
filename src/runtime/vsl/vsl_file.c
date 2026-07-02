/*
 * vsl_file.c  --  VSL File Operations Implementation
 */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "vsl/vsl_internal.h"
#include "vsl/vsl_file.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>

/* Global state */
extern VSL_STATE g_vsl;

/* -- File Operations ---------------------------------------------- */

int vsl_open(const char *path, int flags, int mode) {
    if (!g_vsl.active) return -1;
    if (g_vsl.n_fds >= VSL_MAX_FDS) return -1;
    if (!path) return -2; /* ENOENT */

    int host_fd = open(path, flags, mode);
    if (host_fd < 0) return -errno;

    int vsl_fd = g_vsl.n_fds + 3; /* 0=stdin, 1=stdout, 2=stderr */
    VSL_FD *vfd = &g_vsl.fds[g_vsl.n_fds++];
    vfd->fd = vsl_fd;
    vfd->flags = (uint32_t)flags;
    vfd->mode = (uint32_t)mode;
    vfd->vsl_fd = host_fd; /* Store host fd for delegation */
    strncpy(vfd->path, path, 255);
    vfd->path[255] = '\0';

    return vsl_fd;
}

int vsl_close(int fd) {
    if (fd < 3) return -1; /* Can't close stdin/stdout/stderr */
    for (uint32_t i = 0; i < g_vsl.n_fds; i++) {
        if (g_vsl.fds[i].fd == fd) {
            int host_fd = g_vsl.fds[i].vsl_fd;
            if (host_fd >= 0) close(host_fd);
            for (uint32_t j = i; j < g_vsl.n_fds - 1; j++)
                g_vsl.fds[j] = g_vsl.fds[j + 1];
            g_vsl.n_fds--;
            return 0;
        }
    }
    return -1;
}

int64_t vsl_read(int fd, void *buf, size_t count) {
    if (!g_vsl.active) return -1;
    if (!buf && count) return -1;
    for (uint32_t i = 0; i < g_vsl.n_fds; i++) {
        if (g_vsl.fds[i].fd == fd) {
            int host_fd = g_vsl.fds[i].vsl_fd;
            if (host_fd < 0) return -1;
            ssize_t n = read(host_fd, buf, count);
            return (n >= 0) ? (int64_t)n : -1;
        }
    }
    return -1;
}

int64_t vsl_write(int fd, const void *buf, size_t count) {
    if (!g_vsl.active) return -1;
    if (!buf && count) return -1;
    for (uint32_t i = 0; i < g_vsl.n_fds; i++) {
        if (g_vsl.fds[i].fd == fd) {
            int host_fd = g_vsl.fds[i].vsl_fd;
            if (host_fd < 0) return -1;
            if (fd == 1 || fd == 2) {
                printf("%.*s", (int)count, (const char *)buf);
                return (int64_t)count;
            }
            ssize_t n = write(host_fd, buf, count);
            return (n >= 0) ? (int64_t)n : -1;
        }
    }
    return -1;
}

int64_t vsl_lseek(int fd, int64_t offset, int whence) {
    if (!g_vsl.active) return -1;
    /* For stdin/stdout/stderr, delegate directly */
    if (fd < 3) {
        off_t result = lseek(fd, (off_t)offset, whence);
        return (result < 0) ? -errno : (int64_t)result;
    }
    /* Look up host fd from VSL fd table */
    for (uint32_t i = 0; i < g_vsl.n_fds; i++) {
        if (g_vsl.fds[i].fd == fd) {
            int host_fd = g_vsl.fds[i].vsl_fd;
            if (host_fd < 0) return -9; /* EBADF */
            off_t result = lseek(host_fd, (off_t)offset, whence);
            return (result < 0) ? -errno : (int64_t)result;
        }
    }
    return -9; /* EBADF */
}

/* -- File Descriptor Accessors ------------------------------------ */

int vsl_fd_get_fd(const VSL_FD *vfd) {
    return vfd ? vfd->fd : -1;
}

uint32_t vsl_fd_get_flags(const VSL_FD *vfd) {
    return vfd ? vfd->flags : 0;
}

uint32_t vsl_fd_get_mode(const VSL_FD *vfd) {
    return vfd ? vfd->mode : 0;
}

int vsl_fd_get_host_fd(const VSL_FD *vfd) {
    return vfd ? vfd->vsl_fd : -1;
}

const char *vsl_fd_get_path(const VSL_FD *vfd) {
    return vfd ? vfd->path : NULL;
}

VSL_FD *vsl_get_fd(int fd) {
    for (uint32_t i = 0; i < g_vsl.n_fds; i++) {
        if (g_vsl.fds[i].fd == fd) return &g_vsl.fds[i];
    }
    return NULL;
}