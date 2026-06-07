/*
 * txfs.c — WuBuOS Transactional Filesystem Layer Implementation
 *
 * Cell 100: Journal-based atomic filesystem operations.
 * Write-ahead log ensures crash consistency.
 */

#include "txfs.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ── CRC32 Table ───────────────────────────────────────────── */

static uint32_t crc32_table[256];
static int crc32_initialized = 0;

static void crc32_init_table(void) {
    if (crc32_initialized) return;
    for (uint32_t i = 0; i < 256; i++) {
        uint32_t c = i;
        for (int j = 0; j < 8; j++) {
            c = (c & 1) ? (0xEDB88320 ^ (c >> 1)) : (c >> 1);
        }
        crc32_table[i] = c;
    }
    crc32_initialized = 1;
}

uint32_t txfs_crc32(const void *data, size_t size) {
    crc32_init_table();
    const uint8_t *p = (const uint8_t *)data;
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < size; i++) {
        crc = crc32_table[(crc ^ p[i]) & 0xFF] ^ (crc >> 8);
    }
    return crc ^ 0xFFFFFFFF;
}

/* ── Journal Header Checksum ──────────────────────────────── */

static uint32_t journal_header_checksum(const txfs_journal_header_t *h) {
    /* Checksum over everything except the checksum field itself */
    const uint8_t *p = (const uint8_t *)h;
    return txfs_crc32(p, offsetof(txfs_journal_header_t, checksum));
}

/* ── Lifecycle ─────────────────────────────────────────────── */

int txfs_init(txfs_t *tx, const char *fs_root) {
    memset(tx, 0, sizeof(*tx));

    if (fs_root) {
        strncpy(tx->fs_root, fs_root, TXFS_MAX_PATH - 1);
    } else {
        strcpy(tx->fs_root, "/tmp/wubu_txfs");
    }

    /* Allocate journal buffer */
    tx->journal = (uint8_t *)calloc(1, TXFS_JOURNAL_SIZE);
    if (!tx->journal) return -1;
    tx->journal_size = TXFS_JOURNAL_SIZE;

    /* Initialize journal header */
    tx->header.magic = TXFS_JOURNAL_MAGIC;
    tx->header.version = 1;
    tx->header.txn_id_counter = 1;
    tx->header.entry_count = 0;
    tx->header.committed_count = 0;
    tx->header.applied_count = 0;
    tx->header.checksum = journal_header_checksum(&tx->header);

    /* Check for existing journal (recovery) */
    tx->recovered = 0;
    /* In hosted mode, we always start fresh.
     * In the real kernel, we'd read the journal from disk
     * and call txfs_recover() if entry_count > applied_count. */

    return 0;
}

void txfs_shutdown(txfs_t *tx) {
    if (!tx) return;

    /* If there's an active transaction, abort it */
    if (tx->txn_active) {
        txfs_abort(tx);
    }

    if (tx->journal) {
        free(tx->journal);
        tx->journal = NULL;
    }
    tx->journal_size = 0;
}

/* ── Transaction Management ────────────────────────────────── */

int txfs_begin(txfs_t *tx) {
    if (!tx) return -1;
    if (tx->txn_active) return -1;  /* Already in a transaction */

    memset(&tx->active_txn, 0, sizeof(tx->active_txn));
    tx->active_txn.txn_id = tx->header.txn_id_counter++;
    tx->active_txn.state = TXFS_TXN_OPEN;
    tx->active_txn.op_count = 0;
    tx->txn_active = 1;

    return (int)tx->active_txn.txn_id;
}

/* Write a journal entry to the journal buffer */
static int journal_write_entry(txfs_t *tx, const txfs_journal_entry_t *entry) {
    if (!tx || !tx->journal || !entry) return -1;

    /* Calculate offset for new entry */
    uint32_t offset = sizeof(txfs_journal_header_t) +
                      tx->header.entry_count * sizeof(txfs_journal_entry_t);

    if (offset + sizeof(txfs_journal_entry_t) > tx->journal_size) {
        return -1;  /* Journal full */
    }

    memcpy(tx->journal + offset, entry, sizeof(txfs_journal_entry_t));
    tx->header.entry_count++;

    return 0;
}

/* Update journal header checksum */
static void journal_update_checksum(txfs_t *tx) {
    tx->header.checksum = journal_header_checksum(&tx->header);
    memcpy(tx->journal, &tx->header, sizeof(txfs_journal_header_t));
}

int txfs_commit(txfs_t *tx) {
    if (!tx || !tx->txn_active) return -1;

    txfs_txn_t *txn = &tx->active_txn;
    if (txn->op_count == 0) {
        /* Empty transaction — just close it */
        txn->state = TXFS_TXN_COMMITTING;
        tx->txns_committed++;
        tx->txn_active = 0;
        return 0;
    }

    txn->state = TXFS_TXN_COMMITTING;

    /* Phase 1: Write all ops to journal (write-ahead) */
    for (int i = 0; i < txn->op_count; i++) {
        txfs_journal_entry_t entry = txn->ops[i];
        entry.txn_id = txn->txn_id;
        entry.op_index = (uint32_t)i;
        entry.data_crc = txfs_crc32(entry.data, entry.size);

        int rc = journal_write_entry(tx, &entry);
        if (rc != 0) {
            /* Journal full — abort */
            txn->state = TXFS_TXN_ABORTED;
            tx->txn_active = 0;
            tx->txns_aborted++;
            return -1;
        }
    }

    /* Phase 2: Mark entries as committed in header */
    tx->header.committed_count = tx->header.entry_count;
    journal_update_checksum(tx);

    /* Phase 3: Apply operations (in hosted mode, we simulate
     * by just marking them applied. In the real kernel, we'd
     * call through to FAT32 to actually perform the writes.) */
    for (int i = 0; i < txn->op_count; i++) {
        /* Apply op — in hosted test mode, this is a no-op
         * because we don't have a real filesystem backend.
         * The journal records are sufficient to prove atomicity. */
        tx->entries_applied++;
    }

    /* Phase 4: Checkpoint — mark applied */
    tx->header.applied_count = tx->header.committed_count;
    journal_update_checksum(tx);

    /* Done */
    tx->txns_committed++;
    txn->state = TXFS_TXN_CLOSED;
    tx->txn_active = 0;

    return 0;
}

int txfs_abort(txfs_t *tx) {
    if (!tx || !tx->txn_active) return -1;

    tx->active_txn.state = TXFS_TXN_ABORTED;
    tx->txn_active = 0;
    tx->txns_aborted++;

    return 0;
}

int txfs_txn_active(const txfs_t *tx) {
    return tx && tx->txn_active;
}

/* ── Transactional Operations ──────────────────────────────── */

static int add_op(txfs_t *tx, txfs_op_type_t type, const char *path,
                  uint64_t offset, const void *data, uint32_t size,
                  const char *path2) {
    if (!tx || !tx->txn_active) return -1;
    if (tx->active_txn.op_count >= TXFS_MAX_TXN_OPS) return -1;

    txfs_journal_entry_t *op = &tx->active_txn.ops[tx->active_txn.op_count];
    memset(op, 0, sizeof(*op));

    op->type = type;
    op->offset = offset;
    op->size = size > 512 ? 512 : size;  /* Cap inline data at 512 bytes */

    if (path) strncpy(op->path, path, TXFS_MAX_PATH - 1);
    if (path2) strncpy(op->path2, path2, TXFS_MAX_PATH - 1);

    if (data && size > 0) {
        uint32_t copy_size = op->size;
        memcpy(op->data, data, copy_size);
    }

    tx->active_txn.op_count++;
    return 0;
}

int txfs_write(txfs_t *tx, const char *path,
               uint64_t offset, const void *data, uint32_t size) {
    return add_op(tx, TXFS_OP_WRITE, path, offset, data, size, NULL);
}

int txfs_truncate(txfs_t *tx, const char *path, uint64_t size) {
    return add_op(tx, TXFS_OP_TRUNCATE, path, size, NULL, 0, NULL);
}

int txfs_create(txfs_t *tx, const char *path) {
    return add_op(tx, TXFS_OP_CREATE, path, 0, NULL, 0, NULL);
}

int txfs_delete(txfs_t *tx, const char *path) {
    return add_op(tx, TXFS_OP_DELETE, path, 0, NULL, 0, NULL);
}

int txfs_mkdir(txfs_t *tx, const char *path) {
    return add_op(tx, TXFS_OP_MKDIR, path, 0, NULL, 0, NULL);
}

int txfs_rename(txfs_t *tx, const char *old_path, const char *new_path) {
    return add_op(tx, TXFS_OP_RENAME, old_path, 0, NULL, 0, new_path);
}

/* ── Recovery ──────────────────────────────────────────────── */

int txfs_recover(txfs_t *tx) {
    if (!tx) return -1;

    /* In the real kernel, we'd read the journal from disk and
     * replay any committed but unapplied entries.
     * In hosted mode, we simulate this by checking the header. */

    uint32_t unapplied = tx->header.committed_count - tx->header.applied_count;
    if (unapplied == 0) return 0;

    /* Replay unapplied entries */
    uint32_t replayed = 0;
    for (uint32_t i = tx->header.applied_count; i < tx->header.committed_count; i++) {
        uint32_t offset = sizeof(txfs_journal_header_t) +
                          i * sizeof(txfs_journal_entry_t);
        if (offset + sizeof(txfs_journal_entry_t) > tx->journal_size) break;

        txfs_journal_entry_t *entry = (txfs_journal_entry_t *)(tx->journal + offset);

        /* Validate CRC */
        uint32_t expected_crc = txfs_crc32(entry->data, entry->size);
        if (entry->data_crc != expected_crc) {
            /* CRC mismatch — entry is corrupt, stop replay */
            break;
        }

        /* Apply entry (no-op in hosted mode) */
        replayed++;
    }

    /* Update applied count */
    tx->header.applied_count += replayed;
    journal_update_checksum(tx);

    if (replayed > 0) {
        tx->recovered = 1;
        tx->recovery_count++;
        tx->entries_applied += replayed;
    }

    return (int)replayed;
}

int txfs_was_recovered(const txfs_t *tx) {
    return tx ? tx->recovered : 0;
}

/* ── Journal Inspection ────────────────────────────────────── */

uint32_t txfs_journal_count(const txfs_t *tx) {
    return tx ? tx->header.entry_count : 0;
}

uint32_t txfs_committed_count(const txfs_t *tx) {
    return tx ? tx->header.committed_count : 0;
}

uint32_t txfs_applied_count(const txfs_t *tx) {
    return tx ? tx->header.applied_count : 0;
}

/* ── Diagnostics ───────────────────────────────────────────── */

void txfs_dump(const txfs_t *tx) {
    if (!tx) return;
    printf("TXFS: magic=0x%08X ver=%d txn_counter=%d\n",
           tx->header.magic, tx->header.version, tx->header.txn_id_counter);
    printf("  Journal: entries=%d committed=%d applied=%d\n",
           tx->header.entry_count, tx->header.committed_count,
           tx->header.applied_count);
    printf("  Active txn: %s (id=%d, ops=%d)\n",
           tx->txn_active ? "YES" : "NO",
           tx->active_txn.txn_id, tx->active_txn.op_count);
    printf("  Stats: committed=%lu aborted=%lu applied=%lu recovered=%d\n",
           (unsigned long)tx->txns_committed, (unsigned long)tx->txns_aborted,
           (unsigned long)tx->entries_applied, tx->recovered);
}

uint64_t txfs_committed_total(const txfs_t *tx) {
    return tx ? tx->txns_committed : 0;
}

uint64_t txfs_aborted_total(const txfs_t *tx) {
    return tx ? tx->txns_aborted : 0;
}
