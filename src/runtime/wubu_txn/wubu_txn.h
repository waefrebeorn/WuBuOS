/*
 * wubu_txn.h -- WuBuOS Transactional Speculation subsystem.
 *
 * Ported from GrahaOS kernel/txn (Phase 25) and wired to WuBuOS's REAL
 * snapshot manager (src/runtime/wubu_snapshot.h) instead of a kernel PML4
 * COW (which is not host-portable). The GrahaOS contract is preserved:
 *
 *   - txn_begin() takes a coherent checkpoint of the caller's state via the
 *     existing snapshot manager, then records every EXTERNAL effect (a send
 *     to an out-of-scope peer) into a buffered-message ring.
 *   - txn_commit() keeps the checkpoint + drains the ring to the real peers.
 *   - txn_abort()  rolls the caller's state back to the checkpoint (real
 *     wubu_snapshot_rollback) and DISCARDS the buffered ring.
 *
 * State machine (GrahaOS-mandated):
 *   ACTIVE -> COMMITTING -> {COMMITTED, COMMITTING_FAILED}
 *   COMMITTING_FAILED -> {COMMITTING, ABORTING}
 *   ACTIVE -> ABORTING -> ABORTED
 *
 * Opaque API, minimal includes, C11. No god headers.
 */
#ifndef WUBU_TXN_H
#define WUBU_TXN_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* ---- Limits (mirror GrahaOS) ---- */
#define WUBU_TXN_MAX_NESTING        4u
#define WUBU_TXN_DEFAULT_BUFFER     4096u   /* default ring capacity (msgs) */
#define WUBU_TXN_NAME_MAX           31u
#define WUBU_TXN_MAX_SCOPE_PIDS     256u

/* ---- Flags (passed to wubu_txn_begin) ---- */
#define WUBU_TXN_SELF_SCOPE  0x00000001u
#define WUBU_TXN_GLOBAL_SCOPE 0x00000002u

/* ---- State machine ---- */
typedef enum {
    WUBU_TXN_INVALID = 0,
    WUBU_TXN_ACTIVE,
    WUBU_TXN_COMMITTING,
    WUBU_TXN_COMMITTED,
    WUBU_TXN_COMMITTING_FAILED,
    WUBU_TXN_ABORTING,
    WUBU_TXN_ABORTED
} WubuTxnState;

/* ---- Errors ---- */
#define WUBU_TXN_OK         0
#define WUBU_TXN_EINVAL    -22
#define WUBU_TXN_EPERM     -1
#define WUBU_TXN_ENOMEM    -3
#define WUBU_TXN_ENESTED   -201

/* ---- Opaque transaction handle (full struct in wubu_txn_internal.h) ---- */
typedef struct WubuTxn WubuTxn;

/* ---- Buffered external-effect record (faithful to GrahaOS header) ---- */
typedef struct WubuTxnMsg {
    uint64_t target_id;     /* peer/channel id the send was destined for */
    uint32_t payload_len;   /* bytes in payload */
    uint32_t flags;         /* kind tag from the original send */
    uint8_t  payload[1024]; /* inline payload (sufficient for agent RPCs) */
} WubuTxnMsg;

/* ---- Lifecycle ---- */
void    wubu_txn_init(void);

/* Begin a transaction. Snapshots caller state, returns handle or NULL.
 * scope_pids[] (terminated by -1) lists peers considered IN-scope; sends to
 * other pids are buffered instead of delivered live. */
WubuTxn *wubu_txn_begin(uint32_t flags, const char *name,
                        const int32_t *scope_pids, const char *checkpoint_dir);

/* Record an external effect. Called by the IPC layer when a send targets an
 * out-of-scope peer while a txn is active for this caller. Returns 0 if
 * buffered, -1 if no active txn / buffer full. */
int  wubu_txn_buffer_send(WubuTxn *t, uint64_t target_id,
                          const void *payload, uint32_t len, uint32_t flags);

/* Commit: keep checkpoint + drain buffered sends to real peers. Returns
 * WUBU_TXN_OK or a negative error. */
int  wubu_txn_commit(WubuTxn *t);

/* Abort: roll caller state back to checkpoint + discard buffered sends. */
int  wubu_txn_abort(WubuTxn *t);

/* Current state. */
WubuTxnState wubu_txn_state(const WubuTxn *t);
const char  *wubu_txn_state_name(WubuTxnState s);

/* Free a handle after commit/abort. */
void wubu_txn_destroy(WubuTxn *t);

/* ---- Per-caller active-txn stack (the agent runtime uses this) ---- */
WubuTxn *wubu_txn_current(int32_t caller_pid);
int      wubu_txn_push(int32_t caller_pid, WubuTxn *t);
int      wubu_txn_pop(int32_t caller_pid);

#endif /* WUBU_TXN_H */
