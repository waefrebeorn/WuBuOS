/*
 * wubu_gvfs.c -- GVfs: the async file-operation state machine.
 *
 * See wubu_gvfs.h.
 */
#include "wubu_gvfs.h"

#include <string.h>

const char *wubu_gvfs_state_name(wubu_gvfs_state_t s)
{
    switch (s) {
    case WUBU_GVFS_IDLE:      return "idle";
    case WUBU_GVFS_MOUNTING:  return "mounting";
    case WUBU_GVFS_OPENING:   return "opening";
    case WUBU_GVFS_READING:   return "reading";
    case WUBU_GVFS_WRITING:   return "writing";
    case WUBU_GVFS_SEEKING:   return "seeking";
    case WUBU_GVFS_CLOSING:   return "closing";
    case WUBU_GVFS_CANCELLED: return "cancelled";
    case WUBU_GVFS_DONE:      return "done";
    default:                  return "unknown";
    }
}

bool wubu_gvfs_is_in_flight(wubu_gvfs_state_t s)
{
    switch (s) {
    case WUBU_GVFS_MOUNTING:
    case WUBU_GVFS_OPENING:
    case WUBU_GVFS_READING:
    case WUBU_GVFS_WRITING:
    case WUBU_GVFS_SEEKING:
    case WUBU_GVFS_CLOSING:
        return true;
    default:
        return false;
    }
}

bool wubu_gvfs_is_terminal(wubu_gvfs_state_t s)
{
    return s == WUBU_GVFS_DONE || s == WUBU_GVFS_CANCELLED;
}

bool wubu_gvfs_can_start(wubu_gvfs_state_t from, wubu_gvfs_state_t to)
{
    /* Only MOUNTING and OPENING start from IDLE -- a read on an unopened
     * handle would dereference a null file pointer. */
    if (from == WUBU_GVFS_IDLE)
        return to == WUBU_GVFS_MOUNTING || to == WUBU_GVFS_OPENING;

    /* An open handle can read, write, seek, or close.  It can also re-open
     * (GVfs allows reopening a handle for another file). */
    if (from == WUBU_GVFS_DONE || from == WUBU_GVFS_CANCELLED)
        return false;                 /* terminal: reset first */

    switch (from) {
    case WUBU_GVFS_OPENING:
    case WUBU_GVFS_READING:
    case WUBU_GVFS_WRITING:
    case WUBU_GVFS_SEEKING:
        return to == WUBU_GVFS_OPENING || to == WUBU_GVFS_READING ||
               to == WUBU_GVFS_WRITING || to == WUBU_GVFS_SEEKING ||
               to == WUBU_GVFS_CLOSING;
    case WUBU_GVFS_MOUNTING:
        /* mounting leads to OPENING (or is cancelled) */
        return to == WUBU_GVFS_OPENING || to == WUBU_GVFS_IDLE;
    case WUBU_GVFS_CLOSING:
        return false;                 /* finishing is finish(), not start() */
    default:
        return false;
    }
}

const char *wubu_gvfs_start(wubu_gvfs_state_t *s, wubu_gvfs_state_t to)
{
    if (!s) return "no handle";
    if (wubu_gvfs_is_terminal(*s))
        return "the handle is in a terminal state; reset it before reusing it";
    if (!wubu_gvfs_can_start(*s, to))
        return "that operation cannot start from the current state";
    *s = to;
    return NULL;
}

const char *wubu_gvfs_cancel(wubu_gvfs_state_t *s)
{
    if (!s) return "no handle";
    /* Cancelling something that already finished is how a DOUBLE-completion
     * callback happens, so it is refused rather than ignored. */
    if (*s == WUBU_GVFS_IDLE) return "nothing is in flight to cancel";
    if (*s == WUBU_GVFS_CANCELLED) return "the operation is already cancelled";
    if (*s == WUBU_GVFS_DONE) return "the operation has already finished";
    *s = WUBU_GVFS_CANCELLED;
    return NULL;
}

const char *wubu_gvfs_finish(wubu_gvfs_state_t *s)
{
    if (!s) return "no handle";
    /* Only CLOSING completes a handle.  A read finishing does not close it --
     * that would leak the file descriptor on every partial read. */
    if (*s != WUBU_GVFS_CLOSING)
        return "only a closing operation can finish";
    *s = WUBU_GVFS_DONE;
    return NULL;
}

const char *wubu_gvfs_reset(wubu_gvfs_state_t *s)
{
    if (!s) return "no handle";
    if (!wubu_gvfs_is_terminal(*s))
        return "only a terminal handle can be reset";
    *s = WUBU_GVFS_IDLE;
    return NULL;
}

/* -- handles --------------------------------------------------------- */

void wubu_gvfs_handle_init(wubu_gvfs_handle_t *h, uint64_t id)
{
    if (!h) return;
    memset(h, 0, sizeof(*h));
    h->state = WUBU_GVFS_IDLE;
    h->handle_id = id;
}

#define HANDLE_OP(fn, target)                                    \
    const char *fn(wubu_gvfs_handle_t *h)                        \
    {                                                            \
        if (!h) return "no handle";                              \
        const char *e = wubu_gvfs_start(&h->state, (target));    \
        if (!e) h->start_count++;                                \
        return e;                                                \
    }

HANDLE_OP(wubu_gvfs_handle_open,  WUBU_GVFS_OPENING)
HANDLE_OP(wubu_gvfs_handle_read,  WUBU_GVFS_READING)
HANDLE_OP(wubu_gvfs_handle_write, WUBU_GVFS_WRITING)
HANDLE_OP(wubu_gvfs_handle_close, WUBU_GVFS_CLOSING)

const char *wubu_gvfs_handle_cancel(wubu_gvfs_handle_t *h)
{
    if (!h) return "no handle";
    const char *e = wubu_gvfs_cancel(&h->state);
    if (!e) h->cancel_count++;
    return e;
}

const char *wubu_gvfs_handle_finish(wubu_gvfs_handle_t *h)
{
    if (!h) return "no handle";
    return wubu_gvfs_finish(&h->state);
}

const char *wubu_gvfs_handle_reset(wubu_gvfs_handle_t *h)
{
    if (!h) return "no handle";
    return wubu_gvfs_reset(&h->state);
}
