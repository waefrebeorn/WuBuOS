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

/* Forward FID lookup used by the callbacks. */
styx_fid_t *find_fid(styx_server_t *srv, uint32_t fid);

/* ── Shared Wayland/launcher globals ────────────────────────────────
 * Defined once (in hosted.c for g_hosted_state; in hosted_wayland.c for
 * the SHM pool + key/pointer state).  Both TUs need the types, so the
 * shm_buffer_t / SHM_BUFFERS definition lives here. */
typedef struct {
    struct wl_buffer *wl_buf;
    uint32_t         *pixels;
    int               width;
    int               height;
    int               stride;
    int               fd;
} shm_buffer_t;

#define SHM_BUFFERS 2

extern hosted_state_t *g_hosted_state;

/* ── Cross-module Wayland sub-state (owned by hosted_wayland_*.c) ────
 * After the monolith split, a few globals/functions are referenced by more
 * than one sub-module. They are defined (strong) in their owning module and
 * declared here so the siblings can see them without leaking into the public
 * hosted.h. Each is prefixed with the owning module for clarity. */
extern shm_buffer_t    g_shm_bufs[SHM_BUFFERS];   /* owned by hosted_wayland_shm.c */
extern int             g_cur_buf;                   /* owned by hosted_wayland_shm.c */
void shm_buffer_create(shm_buffer_t *buf, int w, int h);   /* shm module */
void shm_buffer_destroy(shm_buffer_t *buf);                 /* shm module */

/* touch_listener is defined (const, non-static) in hosted_wayland_input.c and
 * referenced by seat_capabilities() in the same module. */
extern const struct wl_touch_listener touch_listener;

/* ── Entry points driven by the hosted.c launcher core ─────────────── */
int  hosted_wl_connect(hosted_state_t *state);
/* Tear down the Wayland connection (destroy globals, disconnect). */
void hosted_wl_disconnect(void);
/* Pump pending Wayland events. */
void hosted_wl_dispatch(void);
/* Blit the VBE back-buffer into the current SHM buffer + commit. */
void hosted_wl_frame_render(void);

/* ── Sub-module lifecycle hooks (declared so the facade can orchestrate) ── */
void wl_shm_init(hosted_state_t *state);
void wl_shm_term(void);
void wl_input_init(hosted_state_t *state);
void wl_input_term(void);
void wl_surface_init(hosted_state_t *state);
void wl_surface_term(void);

#endif /* WUBU_HOSTED_INTERNAL_H */
