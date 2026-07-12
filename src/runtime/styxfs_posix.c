/*
 * styxfs_posix.c -- StyxFS POSIX-like file API + global server instance.
 * Extracted from the monolithic styxfs.c. Depends on styxfs_internal.h for
 * the shared file-table + server helpers. C11, no god headers.
 */
#define _GNU_SOURCE
#include "styxfs.h"
#include "styxfs_internal.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
/* -- Global server instance --------------------------------------- */

styxfs_server_t *g_styxfs_server = NULL;

/* -- POSIX-like File API Implementation --------------------------- */

/* stat - get file status */
int styxfs_stat(const char *path, struct stat *st) {
    if (!g_styxfs_server || !path || !st) return -1;
    
    styxfs_file_t *f = styxfs_resolve(g_styxfs_server, path, 0);
    if (!f) return -1;
    
    memset(st, 0, sizeof(struct stat));
    
    if (f->qid_type == STX_QTDIR || f->is_dir) {
        st->st_mode = S_IFDIR | 0755;
    } else {
        st->st_mode = S_IFREG | 0644;
    }
    
    st->st_size = (off_t)f->payload_size;
    st->st_mtime = (time_t)f->mtime;
    st->st_atime = (time_t)f->atime;
    st->st_ctime = (time_t)f->mtime;
    st->st_nlink = 1;
    st->st_uid = 0;
    st->st_gid = 0;
    
    return 0;
}

/* create - create file/directory */
int styxfs_create(const char *path, int mode, int perm) {
    if (!g_styxfs_server || !path) return -1;
    if (g_styxfs_server->readonly) return -1;
    
    int is_dir = (mode & 0xF000) == 0x4000; /* S_IFDIR */
    
    styxfs_file_t *f = styxfs_resolve(g_styxfs_server, path, 1);
    if (f) return -1; /* Already exists */
    
    f = styxfs_file_alloc(g_styxfs_server);
    if (!f) return -1;
    
    f->qid_path = styxfs_next_qid_path(g_styxfs_server);
    f->qid_type = is_dir ? STX_QTDIR : STX_QTFILE;
    f->qid_version = 1;
    strncpy(f->path, path, STYXFS_MAX_PATH - 1);
    f->is_dir = is_dir;
    f->file_mode = (uint16_t)(perm & 0777);
    f->atime = (uint32_t)time(NULL);
    f->mtime = f->atime;
    
    return 0;
}

/* remove - remove file */
int styxfs_remove(const char *path) {
    if (!g_styxfs_server || !path) return -1;
    if (g_styxfs_server->readonly) return -1;
    
    styxfs_file_t *f = styxfs_resolve(g_styxfs_server, path, 0);
    if (!f) return -1;
    
    if (f->container_payload) free(f->container_payload);
    if (f->write_buf) free(f->write_buf);
    f->in_use = 0;
    
    return 0;
}

/* rename - rename/move file */
int styxfs_rename(const char *oldpath, const char *newpath) {
    if (!g_styxfs_server || !oldpath || !newpath) return -1;
    if (g_styxfs_server->readonly) return -1;
    
    styxfs_file_t *f = styxfs_resolve(g_styxfs_server, oldpath, 0);
    if (!f) return -1;
    
    styxfs_file_t *dest = styxfs_resolve(g_styxfs_server, newpath, 0);
    if (dest) return -1; /* Destination exists */
    
    strncpy(f->path, newpath, STYXFS_MAX_PATH - 1);
    f->mtime = (uint32_t)time(NULL);
    f->qid_version++;
    
    return 0;
}

/* open - open file, returns fd index */
int styxfs_open(const char *path, int flags) {
    if (!g_styxfs_server || !path) return -1;
    
    styxfs_file_t *f = styxfs_resolve(g_styxfs_server, path, 0);
    if (!f) {
        /* Try to create if O_CREAT */
        if (flags & 0x40) { /* O_CREAT */
            f = styxfs_file_alloc(g_styxfs_server);
            if (!f) return -1;
            f->qid_path = styxfs_next_qid_path(g_styxfs_server);
            f->qid_type = STX_QTFILE;
            f->qid_version = 1;
            strncpy(f->path, path, STYXFS_MAX_PATH - 1);
            f->is_dir = false;
            f->atime = (uint32_t)time(NULL);
            f->mtime = f->atime;
        } else {
            return -1;
        }
    }
    
    f->mode = flags & 3; /* OREAD, OWRITE, ORDWR */
    
    /* Return index as fd (1-based to distinguish from 0) */
    for (int i = 0; i < STYXFS_MAX_OPEN_FILES; i++) {
        if (&g_styxfs_server->open_files[i] == f) {
            return i + 1;
        }
    }
    return -1;
}

/* read - read from file descriptor */
ssize_t styxfs_read(int fd, void *buf, size_t count) {
    if (!g_styxfs_server) return -1;
    if (fd < 1 || fd > STYXFS_MAX_OPEN_FILES) return -1;
    
    styxfs_file_t *f = &g_styxfs_server->open_files[fd - 1];
    if (!f->in_use) return -1;
    
    if (f->qid_type == STX_QTDIR || f->is_dir) {
        return -1; /* Cannot read directory as file */
    }
    
    if (f->container_payload && f->payload_size > 0) {
        uint64_t file_size = (uint64_t)f->payload_size;
        if (f->offset >= file_size) return 0;
        uint64_t avail = file_size - f->offset;
        size_t to_read = (count < avail) ? count : (size_t)avail;
        if (buf && to_read > 0) {
            memcpy(buf, f->container_payload + f->offset, to_read);
        }
        f->offset += to_read;
        return (ssize_t)to_read;
    }
    
    if (f->write_buf && f->write_offset > 0) {
        uint64_t file_size = (uint64_t)f->write_offset;
        if (f->offset >= file_size) return 0;
        uint64_t avail = file_size - f->offset;
        size_t to_read = (count < avail) ? count : (size_t)avail;
        if (buf && to_read > 0) {
            memcpy(buf, f->write_buf + f->offset, to_read);
        }
        f->offset += to_read;
        return (ssize_t)to_read;
    }
    
    return 0;
}

/* write - write to file descriptor */
ssize_t styxfs_write(int fd, const void *buf, size_t count) {
    if (!g_styxfs_server) return -1;
    if (fd < 1 || fd > STYXFS_MAX_OPEN_FILES) return -1;
    
    styxfs_file_t *f = &g_styxfs_server->open_files[fd - 1];
    if (!f->in_use) return -1;
    if (g_styxfs_server->readonly) return -1;
    
    if (f->qid_type == STX_QTDIR || f->is_dir) {
        return -1; /* Cannot write directory */
    }
    
    uint64_t end = f->offset + count;
    if (end > (uint64_t)f->write_buf_size) {
        size_t new_size = (size_t)(end + 4096);
        uint8_t *new_buf = (uint8_t *)realloc(f->write_buf, new_size);
        if (!new_buf) return -1;
        f->write_buf = new_buf;
        f->write_buf_size = new_size;
    }
    if (buf && count > 0) {
        memcpy(f->write_buf + f->offset, buf, count);
    }
    f->offset = (size_t)end;
    if (f->write_offset < (size_t)end) {
        f->write_offset = (size_t)end;
    }
    f->mtime = (uint32_t)time(NULL);
    f->qid_version++;
    
    return (ssize_t)count;
}

/* close - close file descriptor */
int styxfs_close(int fd) {
    if (!g_styxfs_server) return -1;
    if (fd < 1 || fd > STYXFS_MAX_OPEN_FILES) return -1;
    
    styxfs_file_t *f = &g_styxfs_server->open_files[fd - 1];
    if (!f->in_use) return 0;
    
    /* If there's a write buffer, commit it to payload */
    if (f->write_buf && f->write_offset > 0) {
        if (f->container_payload) free(f->container_payload);
        f->container_payload = f->write_buf;
        f->payload_size = f->write_offset;
        f->write_buf = NULL;
        f->write_buf_size = 0;
        f->write_offset = 0;
    }
    
    f->in_use = 0;
    return 0;
}

/* Resolve a StyxFS namespace path to a backing host directory path.
 * Chooses the LONGEST matching mount prefix (most specific), then joins the
 * remaining components onto the mount's source. If no mount matches, the path
 * is used verbatim (a direct host path). */

/* mkdir - make directory */
int styxfs_mkdir(const char *path, mode_t mode) {
    return styxfs_create(path, S_IFDIR, mode);
}

/* rmdir - remove directory */
int styxfs_rmdir(const char *path) {
    return styxfs_remove(path);
}

