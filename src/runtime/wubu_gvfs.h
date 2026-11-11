/*
 * wubu_gvfs.h -- GVfs: the async file-operation state machine.
 *
 * GVfs mounts virtual filesystems and performs operations ASYNCHRONOUSLY: an
 * operation is started, may be CANCELLED at any time, and finishes on a
 * callback.  The state machine is where the bugs live, because a cancelled
 * operation that is also allowed to finish double-frees, and an operation
 * started on a finished handle corrupts the mount.
 *
 * The rules:
 *
 *   * IDLE may only MOUNT or OPEN.
 *   * An in-flight operation (MOUNTING/OPENING/READING/WRITING/SEEKING/CLOSING)
 *     may be CANCELLED; a cancel from IDLE, DONE or CANCELLED is an ERROR --
 *     cancelling something that already finished is how a double-completion
 *     callback happens.
 *   * DONE and CANCELLED are TERMINAL: nothing may start from them until the
 *     handle is reset to IDLE.  Starting an operation on a terminal handle is
 *     an ERROR, not an implicit reset.
 *   * Only CLOSING may reach DONE.  A read cannot complete a handle.
 *   * CANCELLED may only be reached from an in-flight state, never from IDLE.
 *   * A cancel is IDEMPOTENT-safe in the sense that the FIRST cancel wins; a
 *     second is an error rather than a silent no-op.
 *
 * Opaque structs, C11, minimal includes.
 */
#ifndef WUBU_GVFS_H
#define WUBU_GVFS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum {
    WUBU_GVFS_IDLE = 0,
    WUBU_GVFS_MOUNTING,
    WUBU_GVFS_OPENING,
    WUBU_GVFS_READING,
    WUBU_GVFS_WRITING,
    WUBU_GVFS_SEEKING,
    WUBU_GVFS_CLOSING,
    WUBU_GVFS_CANCELLED,
    WUBU_GVFS_DONE
} wubu_gvfs_state_t;

const char *wubu_gvfs_state_name(wubu_gvfs_state_t s);

bool wubu_gvfs_is_in_flight(wubu_gvfs_state_t s);
bool wubu_gvfs_is_terminal(wubu_gvfs_state_t s);

/* May `to` be started from `from`? */
bool wubu_gvfs_can_start(wubu_gvfs_state_t from, wubu_gvfs_state_t to);

/* Start an operation.  Returns NULL on success, else a static reason. */
const char *wubu_gvfs_start(wubu_gvfs_state_t *s, wubu_gvfs_state_t to);

/* Cancel.  Returns NULL on success, else a static reason. */
const char *wubu_gvfs_cancel(wubu_gvfs_state_t *s);

/* Finish a CLOSING operation.  Returns NULL on success, else a static reason. */
const char *wubu_gvfs_finish(wubu_gvfs_state_t *s);

/* Reset a terminal handle back to IDLE so it can be reused. */
const char *wubu_gvfs_reset(wubu_gvfs_state_t *s);

/* -- a handle ------------------------------------------------------- */

typedef struct {
    wubu_gvfs_state_t state;
    uint64_t          handle_id;
    uint32_t          cancel_count;   /* how many cancels were issued */
    uint32_t          start_count;
} wubu_gvfs_handle_t;

void wubu_gvfs_handle_init(wubu_gvfs_handle_t *h, uint64_t id);

/* Convenience wrappers that operate on a handle. */
const char *wubu_gvfs_handle_open(wubu_gvfs_handle_t *h);
const char *wubu_gvfs_handle_read(wubu_gvfs_handle_t *h);
const char *wubu_gvfs_handle_write(wubu_gvfs_handle_t *h);
const char *wubu_gvfs_handle_close(wubu_gvfs_handle_t *h);
const char *wubu_gvfs_handle_cancel(wubu_gvfs_handle_t *h);
const char *wubu_gvfs_handle_finish(wubu_gvfs_handle_t *h);
const char *wubu_gvfs_handle_reset(wubu_gvfs_handle_t *h);

#endif /* WUBU_GVFS_H */
