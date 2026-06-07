/*
 * txfs.h — WuBuOS Transactional Filesystem Layer
 *
 * Cell 100: Journal-based transaction layer over FAT32.
 * Provides atomic filesystem operations — crash at any point
 * leaves the filesystem in a consistent state.
 *
 * Design (write-ahead logging):
 *   1. Journal: append-only log of pending operations
 *   2. Commit: all ops in a transaction write to journal first
 *   3. Apply: journal entries are applied to FAT32
 *   4. Checkpoint: after apply, journal is truncated
 *   5. Recovery: on mount, replay any unapplied journal entries
 *
 * Properties:
 *   - Atomicity: all ops in a txn commit together or not at all
 *   - Consistency: journal replay restores consistent state
 *   - Durability: journal is flushed before commit returns
 *   - No isolation (ring-0, single-user by design)
 *
 * All C11, no external deps.
 */

#ifndef WUBU_TXFS_H
#define WUBU_TXFS_H

#include <stdint.h>
#include <stddef.h>

/* ── Constants ─────────────────────────────────────────────── */

#define TXFS_JOURNAL_MAGIC    0x544A4631  /* "TJF1" — Transaction Journal Format 1 */
#define TXFS_MAX_TXN_OPS     64          /* Max ops per transaction */
#define TXFS_JOURNAL_SIZE    (64 * 1024) /* 64KB journal */
#define TXFS_MAX_PATH        256

/* ── Journal Operation Types ───────────────────────────────── */

typedef enum {
    TXFS_OP_WRITE     = 1,  /* Write bytes to a file */
    TXFS_OP_TRUNCATE  = 2,  /* Truncate file to size */
    TXFS_OP_CREATE    = 3,  /* Create new file */
    TXFS_OP_DELETE    = 4,  /* Delete file */
    TXFS_OP_MKDIR     = 5,  /* Create directory */
    TXFS_OP_RMDIR     = 6,  /* Remove directory */
    TXFS_OP_RENAME    = 7,  /* Rename file/dir */
    TXFS_OP_CHMOD     = 8,  /* Change attributes */
} txfs_op_type_t;

/* ── Transaction States ────────────────────────────────────── */

typedef enum {
    TXFS_TXN_CLOSED    = 0,
    TXFS_TXN_OPEN     = 1,  /* Building — ops being added */
    TXFS_TXN_COMMITTING = 2, /* Journal written, applying */
    TXFS_TXN_ABORTED   = 3  /* Rolled back */
} txfs_txn_state_t;

/* ── Journal Entry ─────────────────────────────────────────── */

#pragma pack(push, 1)

/* Single operation in a transaction */
typedef struct {
    txfs_op_type_t type;
    uint32_t       txn_id;      /* Transaction this op belongs to */
    uint32_t       op_index;    /* Op sequence within txn */
    uint64_t       offset;      /* File offset (for write) */
    uint32_t       size;        /* Data size */
    char           path[TXFS_MAX_PATH];  /* Target path */
    char           path2[TXFS_MAX_PATH]; /* Secondary path (rename dest) */
    uint8_t        data[512];   /* Inline data (for small writes) */
    uint32_t       data_crc;    /* CRC32 of data */
} txfs_journal_entry_t;

/* Journal header */
typedef struct {
    uint32_t magic;             /* TXFS_JOURNAL_MAGIC */
    uint32_t version;           /* 1 */
    uint32_t txn_id_counter;    /* Next transaction ID */
    uint32_t entry_count;       /* Total entries in journal */
    uint32_t committed_count;   /* Entries that are committed */
    uint32_t applied_count;     /* Entries that have been applied */
    uint32_t checksum;          /* Header checksum */
    uint8_t  reserved[436];     /* Pad to 512 bytes */
} txfs_journal_header_t;

#pragma pack(pop)

/* ── Transaction Handle ────────────────────────────────────── */

typedef struct {
    uint32_t           txn_id;
    txfs_txn_state_t   state;
    int                op_count;
    txfs_journal_entry_t ops[TXFS_MAX_TXN_OPS];
} txfs_txn_t;

/* ── TXFS State ────────────────────────────────────────────── */

typedef struct {
    /* Journal storage (heap-allocated for hosted, disk for kernel) */
    uint8_t            *journal;
    uint32_t           journal_size;
    txfs_journal_header_t header;

    /* Active transaction (only one at a time — ring-0 single-user) */
    txfs_txn_t         active_txn;
    int                txn_active;   /* 1 if a transaction is open */

    /* Filesystem path prefix (for hosted mode) */
    char               fs_root[TXFS_MAX_PATH];

    /* Stats */
    uint64_t           txns_committed;
    uint64_t           txns_aborted;
    uint64_t           entries_applied;
    uint64_t           recovery_count;
    int                recovered;    /* 1 if recovery happened on mount */
} txfs_t;

/* ── Lifecycle ─────────────────────────────────────────────── */

/* Initialize TXFS layer. fs_root is the root directory path. */
int  txfs_init(txfs_t *tx, const char *fs_root);

/* Shutdown TXFS. Flushes and releases journal. */
void txfs_shutdown(txfs_t *tx);

/* ── Transaction Management ────────────────────────────────── */

/* Begin a new transaction. Returns txn_id, or -1 on error. */
int  txfs_begin(txfs_t *tx);

/* Commit the active transaction. Journal is flushed first,
 * then operations are applied, then journal is checkpointed.
 * Returns 0 on success, -1 on error. */
int  txfs_commit(txfs_t *tx);

/* Abort the active transaction. All ops are discarded.
 * Journal entries are marked as aborted. */
int  txfs_abort(txfs_t *tx);

/* Is a transaction active? */
int  txfs_txn_active(const txfs_t *tx);

/* ── Transactional Operations ──────────────────────────────── */

/* Add a write operation to the active transaction.
 * Writes 'size' bytes from 'data' at 'offset' in 'path'.
 * Data is journaled before being applied. */
int  txfs_write(txfs_t *tx, const char *path,
                uint64_t offset, const void *data, uint32_t size);

/* Add a truncate operation. */
int  txfs_truncate(txfs_t *tx, const char *path, uint64_t size);

/* Add a create-file operation. */
int  txfs_create(txfs_t *tx, const char *path);

/* Add a delete operation. */
int  txfs_delete(txfs_t *tx, const char *path);

/* Add a mkdir operation. */
int  txfs_mkdir(txfs_t *tx, const char *path);

/* Add a rename operation. */
int  txfs_rename(txfs_t *tx, const char *old_path, const char *new_path);

/* ── Recovery ──────────────────────────────────────────────── */

/* Replay any unapplied journal entries. Called automatically
 * on txfs_init if journal has uncommitted entries.
 * Returns number of entries replayed, or -1 on error. */
int  txfs_recover(txfs_t *tx);

/* Check if recovery was needed on last mount. */
int  txfs_was_recovered(const txfs_t *tx);

/* ── Journal Inspection ────────────────────────────────────── */

/* Get journal entry count. */
uint32_t txfs_journal_count(const txfs_t *tx);

/* Get committed entry count. */
uint32_t txfs_committed_count(const txfs_t *tx);

/* Get applied entry count. */
uint32_t txfs_applied_count(const txfs_t *tx);

/* ── CRC32 ─────────────────────────────────────────────────── */

/* Compute CRC32 of data. */
uint32_t txfs_crc32(const void *data, size_t size);

/* ── Diagnostics ───────────────────────────────────────────── */

/* Print TXFS state. */
void txfs_dump(const txfs_t *tx);

/* Stats. */
uint64_t txfs_committed_total(const txfs_t *tx);
uint64_t txfs_aborted_total(const txfs_t *tx);

#endif /* WUBU_TXFS_H */
