/*
 * styxfs.h  --  StyxFS: 9P2000/Styx Filesystem for .wubu Containers
 *
 * StyxFS provides a real filesystem namespace backed by .wubu containers.
 * Each .wubu container is exposed as a file/directory in the Styx namespace.
 * Mount points allow composing multiple container repositories.
 *
 * Features:
 *   - Mount .wubu containers at arbitrary paths (/wubu, /apps, /dev, etc.)
 *   - Full 9P2000 operation set: walk, open, read, write, create, remove, stat, wstat
 *   - Directory entries for containers with metadata
 *   - Exec payloads can be "run" via Topen+Tread pattern
 *   - Transactional writes (write to temp, atomic rename on clunk)
 *
 * Reference: Inferno OS styxserver, Plan 9 9P2000
 * License: MIT (WuBuOS additions)
 */

#ifndef WUBU_STYXFS_H
#define WUBU_STYXFS_H

#include "styx.h"
#include "wubu_container.h"
#include <stdint.h>
#include <stddef.h>

/* -- Configuration ------------------------------------------------- */

#define STYXFS_MAX_MOUNTS    16
#define STYXFS_MAX_OPEN_FILES 128
#define STYXFS_MAX_PATH      4096
#define STYXFS_DIR_BUF_SIZE  (16 * 1024)

/* -- Mount Entry --------------------------------------------------- */

typedef struct styxfs_mount {
    char              path[STYXFS_MAX_PATH];     /* Mount point path (e.g., "/wubu") */
    char              source[STYXFS_MAX_PATH];   /* Source directory/file */
    int               is_container_repo;         /* 1 = source is dir of .wubu files */
    void             *repo_data;                 /* Opaque repository handle */
    struct styxfs_mount *next;
} styxfs_mount_t;

/* -- Open File State (per-fid) ------------------------------------- */

typedef struct styxfs_file {
    int          in_use;
    uint64_t     qid_path;           /* Unique file identifier */
    uint8_t      qid_type;           /* STX_QTDIR, STX_QTFILE, STX_QTMOUNT */
    uint32_t     qid_version;
    char         path[STYXFS_MAX_PATH];  /* Full path from root */
    int          mode;                /* OREAD, OWRITE, ORDWR */
    uint64_t     offset;

    /* File metadata (for stat/wstat) */
    uint16_t     file_mode;           /* Unix-style permission bits + type */
    int          is_dir;              /* 1 if directory */
    uint32_t     mtime;               /* Modification time */
    uint32_t     atime;               /* Access time */

    /* For .wubu containers */
    WUBU_HEADER  container_hdr;
    uint8_t     *container_payload;   /* Payload data (owned) */
    size_t       payload_size;

    /* For write transactions */
    uint8_t     *write_buf;
    size_t       write_buf_size;
    size_t       write_offset;
    int          is_temp_write;
} styxfs_file_t;

/* -- StyxFS Server State ------------------------------------------- */

typedef struct styxfs_server {
    styx_server_t  base;              /* Embedded Styx protocol server */
    styxfs_mount_t *mounts;           /* Mount list */
    styxfs_file_t  open_files[STYXFS_MAX_OPEN_FILES];
    uint64_t       next_qid_path;     /* Monotonically increasing QID path */
    int            readonly;          /* Global read-only flag */
} styxfs_server_t;

/* -- API Functions ------------------------------------------------- */

/* Initialize a StyxFS server */
void styxfs_init(styxfs_server_t *srv);

/* Mount a .wubu container repository at a namespace path
 * path: mount point in Styx namespace (e.g., "/wubu")
 * source: filesystem path to directory containing .wubu files, or single .wubu file
 * is_repo: 1 if source is a directory to scan for .wubu files, 0 if single file
 * Returns 0 on success, -1 on error */
int styxfs_mount(styxfs_server_t *srv, const char *path, const char *source, int is_repo);

/* Unmount a path */
int styxfs_unmount(styxfs_server_t *srv, const char *path);

/* Set global read-only mode */
void styxfs_set_readonly(styxfs_server_t *srv, int readonly);

/* Scan a directory for .wubu files and register them */
int styxfs_scan_repo(styxfs_server_t *srv, const char *mount_path, const char *fs_path);

/* Serve a Styx message (uses base styx_serve with StyxFS callbacks) */
int styxfs_serve(styxfs_server_t *srv,
                  const uint8_t *inbuf, uint32_t inlen,
                  uint8_t *outbuf, uint32_t *outlen);

/* -- Internal Callbacks (exposed for testing) ---------------------- */

/* Attach: called on Tattach */
int styxfs_attach_cb(styx_server_t *base, uint32_t fid, const char *aname);

/* Walk: called on Twalk */
int styxfs_walk_cb(styx_server_t *base, uint32_t fid, uint32_t newfid,
                    const char **wname, int nwname,
                    styx_qid_t *qids, int *nwqid);

/* Open: called on Topen */
int styxfs_open_cb(styx_server_t *base, uint32_t fid, int mode,
                    styx_qid_t *qid);

/* Create: called on Tcreate */
int styxfs_create_cb(styx_server_t *base, uint32_t fid, const char *name,
                      uint32_t perm, int mode, styx_qid_t *qid);

/* Read: called on Tread */
int styxfs_read_cb(styx_server_t *base, uint32_t fid,
                    uint64_t offset, uint32_t count,
                    uint8_t *data, uint32_t *nread);

/* Write: called on Twrite */
int styxfs_write_cb(styx_server_t *base, uint32_t fid,
                     uint64_t offset, uint32_t count,
                     const uint8_t *data, uint32_t *nwritten);

/* Clunk: called on Tclunk */
int styxfs_clunk_cb(styx_server_t *base, uint32_t fid);

/* Remove: called on Tremove */
int styxfs_remove_cb(styx_server_t *base, uint32_t fid);

/* Stat: called on Tstat */
int styxfs_stat_cb(styx_server_t *base, uint32_t fid,
                    styx_dir_t *dir);

/* Wstat: called on Twstat */
int styxfs_wstat_cb(styx_server_t *base, uint32_t fid,
                     const styx_dir_t *dir);

/* -- Helper Utilities ---------------------------------------------- */

/* Find mount point for a path; returns mount or NULL, sets *rel_path to relative path */
styxfs_mount_t *styxfs_find_mount(styxfs_server_t *srv, const char *path, char *rel_path);

/* Resolve a path to a styxfs_file_t (creates entry if needed for create) */
styxfs_file_t *styxfs_resolve(styxfs_server_t *srv, const char *path, int create_if_missing);

/* Build directory listing in 9P dir format */
int styxfs_build_dirent(styxfs_server_t *srv, const char *path,
                         uint8_t *buf, uint32_t buf_size, uint32_t *out_size,
                         uint64_t offset, uint32_t count);

/* Load .wubu container from file */
int styxfs_load_container(const char *path, WUBU_HEADER *out_hdr, uint8_t **out_payload, size_t *out_size);

/* Check if a file is a .wubu container */
int styxfs_is_wubu_container(const char *path);

#endif /* WUBU_STYXFS_H */