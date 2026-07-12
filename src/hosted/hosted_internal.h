/*
 * hosted_internal.h -- WuBuOS hosted-mode shared state
 *
 * Holds the Styx-namespace shared types/globals/declarations used by the
 * hosted FS submodule (hosted_styxfs.c) and hosted.c itself.  This is the
 * C11 opaque-safe "internal header" pattern: public API stays in hosted.h,
 * cross-TU implementation details live here.  No god headers.
 */

#ifndef WUBU_HOSTED_INTERNAL_H
#define WUBU_HOSTED_INTERNAL_H

#include "hosted.h"
#include "styx.h"          /* styx_server_t, styx_fid_t, styx_qid_t, styx_dir_t, STYX_MAX_FIDS, STX_* */
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* In-memory filesystem backing the Styx namespace (registered blobs). */
#define STYXFS_MAX_FILES 64

typedef struct {
    char     name[256];
    uint8_t  qtype;
    uint64_t path;
    uint64_t length;
    uint8_t  data[8192];
    uint32_t data_len;
} styxfs_file_t;

/* Shared filesystem state (defined once, in hosted_styxfs.c). */
extern styxfs_file_t g_fs[STYXFS_MAX_FILES];
extern int           g_nfiles;
extern uint64_t      g_next_path;

/* FS namespace builders (implemented in hosted_styxfs.c). */
int  fs_add_dir(const char *name);
int  fs_add_file(const char *name, const uint8_t *data, uint32_t len);
void fs_reset(void);

/* Styx server callbacks (implemented in hosted_styxfs.c, wired into srv->*). */
int styx_attach_cb(styx_server_t *srv, uint32_t fid, const char *aname);
int styx_walk_cb(styx_server_t *srv, uint32_t fid, uint32_t newfid,
                 const char **wname, int nwname,
                 styx_qid_t *qids, int *nwqid);
int styx_open_cb(styx_server_t *srv, uint32_t fid, int mode, styx_qid_t *qid);
int styx_read_cb(styx_server_t *srv, uint32_t fid, uint64_t offset,
                 uint32_t count, uint8_t *data, uint32_t *nread);
int styx_stat_cb(styx_server_t *srv, uint32_t fid, styx_dir_t *dir);

/* Shared FID lookup used by the callbacks. */
styx_fid_t *find_fid(styx_server_t *srv, uint32_t fid);

#endif /* WUBU_HOSTED_INTERNAL_H */
