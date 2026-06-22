/*
 * styxfs_server.c  --  Styx/9P File Server Implementation
 * 
 * A concrete 9P2000 file server that exports a host directory tree.
 * Implements the styx_server_t callbacks using host filesystem operations.
 * 
 * This enables:
 *   - Mount any host directory as a 9P filesystem
 *   - Share WuBuOS .wubu containers via 9P
 *   - Device files (framebuffer, input, audio) as 9P files
 *   - VSL process namespace as 9P files
 */

#include "styx.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <dirent.h>
#include <time.h>
#include <libgen.h>

/* -- StyxFS Server State ------------------------------------------ */

#define STYXFS_MAX_PATH 4096
#define STYXFS_MAX_OPEN_FILES 256

typedef struct {
    int fd;
    int in_use;
} styxfs_host_file_t;

typedef struct {
    char root[STYXFS_MAX_PATH];
    styxfs_host_file_t open_files[STYXFS_MAX_OPEN_FILES];
    uint64_t next_qid_path;
} styxfs_host_server_t;

/* QID path generator */
static uint64_t styxfs_next_qid(styxfs_host_server_t *fs) {
    return fs->next_qid_path++;
}

/* Convert host stat to styx dir */
static int styxfs_stat_to_dir(const struct stat *st, const char *name,
                               uint64_t qid_path, styx_dir_t *dir) {
    memset(dir, 0, sizeof(*dir));
    
    dir->type = 0;
    dir->dev = 0;
    dir->qid.type = S_ISDIR(st->st_mode) ? STX_QTDIR : STX_QTFILE;
    dir->qid.version = (uint32_t)st->st_mtime;
    dir->qid.path = qid_path;
    
    /* Permissions: map Unix mode to 9P */
    uint32_t mode = 0;
    if (S_ISDIR(st->st_mode)) mode |= STX_DMDIR;
    mode |= (st->st_mode & 0777);  /* rwxrwxrwx */
    dir->mode = mode;
    
    dir->atime = (uint32_t)st->st_atime;
    dir->mtime = (uint32_t)st->st_mtime;
    dir->length = (uint64_t)st->st_size;
    
    strncpy(dir->name, name, STYX_MAX_FNAME - 1);
    strncpy(dir->uid, "wubu", STYX_MAX_FNAME - 1);
    strncpy(dir->gid, "wubu", STYX_MAX_FNAME - 1);
    strncpy(dir->muid, "wubu", STYX_MAX_FNAME - 1);
    
    return 0;
}

/* Find free open file slot */
static int styxfs_alloc_file(styxfs_host_server_t *fs) {
    for (int i = 0; i < STYXFS_MAX_OPEN_FILES; i++) {
        if (!fs->open_files[i].in_use) {
            fs->open_files[i].in_use = 1;
            return i;
        }
    }
    return -1;
}

/* Get open file by index */
static styxfs_host_file_t *styxfs_get_file(styxfs_host_server_t *fs, int idx) {
    if (idx < 0 || idx >= STYXFS_MAX_OPEN_FILES) return NULL;
    if (!fs->open_files[idx].in_use) return NULL;
    return &fs->open_files[idx];
}

/* Free open file slot */
static void styxfs_free_file(styxfs_host_server_t *fs, int idx) {
    if (idx < 0 || idx >= STYXFS_MAX_OPEN_FILES) return;
    if (fs->open_files[idx].in_use && fs->open_files[idx].fd >= 0) {
        close(fs->open_files[idx].fd);
    }
    fs->open_files[idx].in_use = 0;
    fs->open_files[idx].fd = -1;
}

/* Convert fid to open file index (stored in file_state) */
static int styxfs_fid_get_index(styx_fid_t *fid) {
    return (int)(uintptr_t)fid->file_state;
}

/* -- Styx Server Callbacks ---------------------------------------- */

static int styxfs_attach(styx_server_t *srv, uint32_t fid, const char *aname) {
    (void)aname;
    styxfs_host_server_t *fs = (styxfs_host_server_t *)srv->user_data;
    if (!fs) return -1;
    
    styx_fid_t *f = styx_fid_alloc(srv, fid);
    if (!f) return -1;
    
    f->qid.type = STX_QTDIR;
    f->qid.version = 1;
    f->qid.path = 1;  /* Root directory */
    f->file_state = NULL;
    
    return 0;
}

static int styxfs_walk(styx_server_t *srv, uint32_t fid, uint32_t newfid,
                        const char **wname, int nwname,
                        styx_qid_t *qids, int *nwqid) {
    styxfs_host_server_t *fs = (styxfs_host_server_t *)srv->user_data;
    if (!fs) return -1;
    
    styx_fid_t *f = styx_fid_lookup(srv, fid);
    if (!f) return -1;
    
    /* Build path from root + walk names */
    char path[STYXFS_MAX_PATH];
    strcpy(path, fs->root);
    
    char *cursor = path + strlen(path);
    
    if (f->file_state) {
        /* Use stored path as base */
        strncpy(path, (char *)f->file_state, STYXFS_MAX_PATH - 1);
        cursor = path + strlen(path);
    } else if (f->qid.path == 1) {
        /* Root fid - use fs->root */
        cursor = path + strlen(path);
    }
    
    int nqid = 0;
    for (int i = 0; i < nwname && nqid < 16; i++) {
        if (*cursor != '/' && cursor > path) *cursor++ = '/';
        size_t nlen = strlen(wname[i]);
        if (cursor + nlen >= path + STYXFS_MAX_PATH - 1) break;
        memcpy(cursor, wname[i], nlen);
        cursor += nlen;
        *cursor = '\0';
        
        struct stat st;
        if (stat(path, &st) != 0) {
            break;
        }
        
        qids[nqid].type = S_ISDIR(st.st_mode) ? STX_QTDIR : STX_QTFILE;
        qids[nqid].version = (uint32_t)st.st_mtime;
        qids[nqid].path = styxfs_next_qid(fs);
        nqid++;
        
        /* Update cursor for next iteration */
        cursor = path + strlen(path) + 1;
    }
    
    /* Allocate new fid */
    styx_fid_t *nf = styx_fid_alloc(srv, newfid);
    if (!nf) return -1;
    
    if (nqid > 0) {
        nf->qid = qids[nqid - 1];
        /* Store full path in file_state for subsequent operations */
        char *stored_path = malloc(strlen(path) + 1);
        if (stored_path) {
            strcpy(stored_path, path);
            nf->file_state = stored_path;
        }
    } else {
        /* Clone fid */
        *nf = *f;
        nf->fid = newfid;
    }
    
    *nwqid = nqid;
    return 0;
}

static int styxfs_open(styx_server_t *srv, uint32_t fid, int mode, styx_qid_t *qid) {
    styxfs_host_server_t *fs = (styxfs_host_server_t *)srv->user_data;
    if (!fs) return -1;
    
    styx_fid_t *f = styx_fid_lookup(srv, fid);
    if (!f) return -1;
    
    const char *path = (const char *)f->file_state;
    if (!path && f->qid.path == 1) {
        path = fs->root;
    }
    if (!path) return -1;
    
    int open_flags = 0;
    switch (mode & 3) {
        case STX_OREAD:  open_flags = O_RDONLY; break;
        case STX_OWRITE: open_flags = O_WRONLY; break;
        case STX_ORDWR:  open_flags = O_RDWR; break;
        case STX_OEXEC:  open_flags = O_RDONLY; break;
    }
    if (mode & STX_OTRUNC) open_flags |= O_TRUNC;
    if (mode & STX_OEXCL) open_flags |= O_EXCL;
    
    int fd = open(path, open_flags);
    if (fd < 0) return -1;
    
    int idx = styxfs_alloc_file(fs);
    if (idx < 0) {
        close(fd);
        return -1;
    }
    
    fs->open_files[idx].fd = fd;
    
    f->open_fid = f->fid;
    f->open_mode = mode;
    f->offset = 0;
    f->file_state = (void *)(uintptr_t)idx;
    
    struct stat st;
    if (fstat(fd, &st) == 0) {
        qid->type = S_ISDIR(st.st_mode) ? STX_QTDIR : STX_QTFILE;
        qid->version = (uint32_t)st.st_mtime;
        qid->path = f->qid.path;
    } else {
        qid->type = STX_QTFILE;
        qid->version = 0;
        qid->path = 0;
    }
    
    return 0;
}

static int styxfs_read(styx_server_t *srv, uint32_t fid, uint64_t offset,
                        uint32_t count, uint8_t *data, uint32_t *nread) {
    styxfs_host_server_t *fs = (styxfs_host_server_t *)srv->user_data;
    if (!fs) return -1;
    
    styx_fid_t *f = styx_fid_lookup(srv, fid);
    if (!f) return -1;
    
    int idx = styxfs_fid_get_index(f);
    if (idx < 0) return -1;
    
    styxfs_host_file_t *of = styxfs_get_file(fs, idx);
    if (!of) return -1;
    
    ssize_t r = pread(of->fd, data, count, (off_t)offset);
    if (r < 0) return -1;
    
    *nread = (uint32_t)r;
    return 0;
}

static int styxfs_write(styx_server_t *srv, uint32_t fid, uint64_t offset,
                         uint32_t count, const uint8_t *data, uint32_t *nwritten) {
    styxfs_host_server_t *fs = (styxfs_host_server_t *)srv->user_data;
    if (!fs) return -1;
    
    styx_fid_t *f = styx_fid_lookup(srv, fid);
    if (!f) return -1;
    
    int idx = styxfs_fid_get_index(f);
    if (idx < 0) return -1;
    
    styxfs_host_file_t *of = styxfs_get_file(fs, idx);
    if (!of) return -1;
    
    ssize_t w = pwrite(of->fd, data, count, (off_t)offset);
    if (w < 0) return -1;
    
    *nwritten = (uint32_t)w;
    return 0;
}

static int styxfs_clunk(styx_server_t *srv, uint32_t fid) {
    styxfs_host_server_t *fs = (styxfs_host_server_t *)srv->user_data;
    if (!fs) return 0;
    
    styx_fid_t *f = styx_fid_lookup(srv, fid);
    if (!f) return 0;
    
    int idx = styxfs_fid_get_index(f);
    if (idx >= 0) {
        styxfs_free_file(fs, idx);
    }
    
    /* Free stored path */
    if (f->file_state && (uintptr_t)f->file_state > 256) {
        free(f->file_state);
    }
    f->file_state = NULL;
    
    return 0;
}

static int styxfs_remove(styx_server_t *srv, uint32_t fid) {
    styxfs_host_server_t *fs = (styxfs_host_server_t *)srv->user_data;
    if (!fs) return -1;
    
    styx_fid_t *f = styx_fid_lookup(srv, fid);
    if (!f) return -1;
    
    const char *path = (const char *)f->file_state;
    if (!path && f->qid.path == 1) path = fs->root;
    if (!path) return -1;
    
    int result = unlink(path);
    if (result != 0 && errno == EISDIR) {
        result = rmdir(path);
    }
    
    return result == 0 ? 0 : -1;
}

static int styxfs_stat(styx_server_t *srv, uint32_t fid, styx_dir_t *dir) {
    styxfs_host_server_t *fs = (styxfs_host_server_t *)srv->user_data;
    if (!fs) return -1;
    
    styx_fid_t *f = styx_fid_lookup(srv, fid);
    if (!f) return -1;
    
    const char *path = (const char *)f->file_state;
    if (!path && f->qid.path == 1) path = fs->root;
    if (!path) return -1;
    
    /* Get filename from path */
    const char *name = strrchr(path, '/');
    name = name ? name + 1 : path;
    
    struct stat st;
    if (stat(path, &st) != 0) return -1;
    
    return styxfs_stat_to_dir(&st, name, f->qid.path, dir);
}

static int styxfs_wstat(styx_server_t *srv, uint32_t fid, const styx_dir_t *dir) {
    styxfs_host_server_t *fs = (styxfs_host_server_t *)srv->user_data;
    if (!fs) return -1;
    
    styx_fid_t *f = styx_fid_lookup(srv, fid);
    if (!f) return -1;
    
    const char *path = (const char *)f->file_state;
    if (!path && f->qid.path == 1) path = fs->root;
    if (!path) return -1;
    
    /* Handle mode changes (chmod) */
    if (dir->mode != 0xFFFFFFFF) {
        int mode = dir->mode & 0777;
        if (chmod(path, mode) != 0) return -1;
    }
    
    /* Handle mtime/atime changes */
    if (dir->mtime != 0xFFFFFFFF || dir->atime != 0xFFFFFFFF) {
        struct timespec ts[2];
        ts[0].tv_sec = (dir->atime != 0xFFFFFFFF) ? dir->atime : 0;
        ts[0].tv_nsec = 0;
        ts[1].tv_sec = (dir->mtime != 0xFFFFFFFF) ? dir->mtime : 0;
        ts[1].tv_nsec = 0;
        if (utimensat(AT_FDCWD, path, ts, 0) != 0) return -1;
    }
    
    /* Handle name change (rename) */
    if (dir->name[0] != '\0') {
        char newpath[STYXFS_MAX_PATH];
        char *last_slash = strrchr(path, '/');
        if (last_slash) {
            size_t prefix_len = last_slash - path + 1;
            strncpy(newpath, path, prefix_len);
            newpath[prefix_len] = '\0';
            strncat(newpath, dir->name, STYXFS_MAX_PATH - prefix_len - 1);
            if (rename(path, newpath) != 0) return -1;
            
            /* Update stored path */
            if (f->file_state && (uintptr_t)f->file_state > 256) {
                free(f->file_state);
            }
            f->file_state = strdup(newpath);
        }
    }
    
    return 0;
}

/* -- Server Creation / Destruction -------------------------------- */

styx_server_t *styxfs_server_create(const char *root_path) {
    if (!root_path) return NULL;
    
    struct stat st;
    if (stat(root_path, &st) != 0 || !S_ISDIR(st.st_mode)) return NULL;
    
    styx_server_t *srv = calloc(1, sizeof(styx_server_t));
    if (!srv) return NULL;
    
    styxfs_host_server_t *fs = calloc(1, sizeof(styxfs_host_server_t));
    if (!fs) {
        free(srv);
        return NULL;
    }
    
    /* Initialize open files */
    for (int i = 0; i < STYXFS_MAX_OPEN_FILES; i++) {
        fs->open_files[i].fd = -1;
        fs->open_files[i].in_use = 0;
    }
    
    realpath(root_path, fs->root);
    fs->next_qid_path = 2;  /* Start after root (1) */
    
    styx_init(srv);
    srv->user_data = fs;
    
    /* Set callbacks */
    srv->attach = styxfs_attach;
    srv->walk = styxfs_walk;
    srv->open = styxfs_open;
    srv->read = styxfs_read;
    srv->write = styxfs_write;
    srv->clunk = styxfs_clunk;
    srv->remove = styxfs_remove;
    srv->stat = styxfs_stat;
    srv->wstat = styxfs_wstat;
    
    return srv;
}

void styxfs_server_destroy(styx_server_t *srv) {
    if (!srv) return;
    
    styxfs_host_server_t *fs = (styxfs_host_server_t *)srv->user_data;
    if (fs) {
        /* Close all open files */
        for (int i = 0; i < STYXFS_MAX_OPEN_FILES; i++) {
            if (fs->open_files[i].in_use && fs->open_files[i].fd >= 0) {
                close(fs->open_files[i].fd);
            }
        }
        
        /* Free any stored paths in fids */
        for (int i = 0; i < STYX_MAX_FIDS; i++) {
            if (srv->fids[i].in_use && srv->fids[i].file_state &&
                (uintptr_t)srv->fids[i].file_state > 256) {
                free(srv->fids[i].file_state);
            }
        }
        
        free(fs);
    }
    
    free(srv);
}
