/*
 * wubu_txn_internal.h -- WuBuOS transaction record layout + globals.
 * Internal to the wubu_txn module.
 */
#ifndef WUBU_TXN_INTERNAL_H
#define WUBU_TXN_INTERNAL_H

#include "wubu_txn.h"
#include "../wubu_snapshot.h"   /* WUBU_MAX_PATH */

struct WubuTxn {
    uint64_t        id;
    uint32_t        flags;
    int32_t         creator_pid;
    WubuTxnState    state;
    char            name[WUBU_TXN_NAME_MAX + 1];

    /* Checkpoint: a real on-disk snapshot directory (created via recursive
     * copy of live_dir). */
    char            live_dir[WUBU_MAX_PATH];      /* the caller's live state dir */
    char            checkpoint_dir[WUBU_MAX_PATH]; /* the snapshot copy */
    bool            checkpoint_valid;

    /* Scope: pids considered in-scope (sends to others are buffered). */
    int32_t         scope_pids[WUBU_TXN_MAX_SCOPE_PIDS];
    uint32_t        scope_count;

    /* Buffered external-effect ring (GrahaOS txn_buffer). */
    WubuTxnMsg      *ring;
    uint32_t        ring_cap;     /* entries */
    uint32_t        ring_head;    /* next write slot */
    uint32_t        ring_count;   /* live entries */

    WubuTxn        *parent;       /* nesting */
    uint32_t        nesting_depth;
};

/* Globals (defined in wubu_txn.c). */
extern uint64_t g_wubu_txn_next_id;

/* Scope oracle: true if pid is in t's scope (a buffered send target). */
bool wubu_txn_is_external_peer(const WubuTxn *t, int32_t pid);

/* Checkpoint helpers (real fs work). */
int  wubu_txn_checkpoint_create(const char *src_dir, const char *dst_dir);
int  wubu_txn_checkpoint_restore(const char *snapshot_dir, const char *live_dir);
int  wubu_txn_checkpoint_delete(const char *dir);

#endif /* WUBU_TXN_INTERNAL_H */
