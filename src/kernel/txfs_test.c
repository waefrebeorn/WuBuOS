/*
 * txfs_test.c  --  Test Suite for WuBuOS Transactional Filesystem
 *
 * Cell 100: Tests journal-based atomic operations, commit/abort,
 * recovery, CRC32 validation, and crash consistency.
 */

#include "txfs.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_pass = 0, g_fail = 0, g_total = 0;
#define TEST(name) printf("  TEST %-45s", name); g_total++;
#define PASS() do { printf("✅\n"); g_pass++; } while(0)
#define FAIL(msg) do { printf("❌ %s\n", msg); g_fail++; } while(0)
#define CHECK(cond, msg) do { if (!(cond)) { FAIL(msg); return; } } while(0)

/* -- Lifecycle Tests ----------------------------------------- */

static void test_init(void) {
    TEST("txfs init");
    txfs_t tx;
    int rc = txfs_init(&tx, "/tmp/wubu_test");
    CHECK(rc == 0, "init should succeed");
    CHECK(tx.journal != NULL, "journal should be allocated");
    CHECK(tx.header.magic == TXFS_JOURNAL_MAGIC, "magic should match");
    CHECK(tx.header.version == 1, "version should be 1");
    CHECK(tx.txn_active == 0, "no active txn");
    txfs_shutdown(&tx);
    CHECK(tx.journal == NULL, "journal should be freed");
    PASS();
}

static void test_init_null(void) {
    TEST("txfs init with NULL path uses default");
    txfs_t tx;
    int rc = txfs_init(&tx, NULL);
    CHECK(rc == 0, "init should succeed");
    CHECK(strlen(tx.fs_root) > 0, "fs_root should be set");
    txfs_shutdown(&tx);
    PASS();
}

/* -- Transaction Lifecycle Tests ----------------------------- */

static void test_begin_commit(void) {
    TEST("begin → commit empty transaction");
    txfs_t tx;
    txfs_init(&tx, NULL);

    int txn_id = txfs_begin(&tx);
    CHECK(txn_id > 0, "txn_id should be > 0");
    CHECK(tx.txn_active == 1, "txn should be active");
    CHECK(tx.active_txn.state == TXFS_TXN_OPEN, "state should be OPEN");

    int rc = txfs_commit(&tx);
    CHECK(rc == 0, "commit should succeed");
    CHECK(tx.txn_active == 0, "txn should be inactive after commit");
    CHECK(tx.txns_committed == 1, "committed count should be 1");

    txfs_shutdown(&tx);
    PASS();
}

static void test_begin_abort(void) {
    TEST("begin → abort");
    txfs_t tx;
    txfs_init(&tx, NULL);

    txfs_begin(&tx);
    int rc = txfs_abort(&tx);
    CHECK(rc == 0, "abort should succeed");
    CHECK(tx.txn_active == 0, "txn should be inactive after abort");
    CHECK(tx.txns_aborted == 1, "aborted count should be 1");
    CHECK(tx.txns_committed == 0, "committed should be 0");

    txfs_shutdown(&tx);
    PASS();
}

static void test_double_begin_fails(void) {
    TEST("double begin fails");
    txfs_t tx;
    txfs_init(&tx, NULL);
    txfs_begin(&tx);
    int rc = txfs_begin(&tx);
    CHECK(rc == -1, "second begin should fail");
    txfs_abort(&tx);
    txfs_shutdown(&tx);
    PASS();
}

static void test_commit_without_begin(void) {
    TEST("commit without begin fails");
    txfs_t tx;
    txfs_init(&tx, NULL);
    CHECK(txfs_commit(&tx) == -1, "commit without begin should fail");
    txfs_shutdown(&tx);
    PASS();
}

/* -- Operation Tests ----------------------------------------- */

static void test_write_op(void) {
    TEST("add write operation");
    txfs_t tx;
    txfs_init(&tx, NULL);
    txfs_begin(&tx);

    const char *data = "Hello, TXFS!";
    int rc = txfs_write(&tx, "/test.txt", 0, data, strlen(data));
    CHECK(rc == 0, "write op should succeed");
    CHECK(tx.active_txn.op_count == 1, "should have 1 op");
    CHECK(tx.active_txn.ops[0].type == TXFS_OP_WRITE, "type should be WRITE");
    CHECK(tx.active_txn.ops[0].offset == 0, "offset should be 0");
    CHECK(tx.active_txn.ops[0].size == strlen(data), "size should match");

    txfs_abort(&tx);
    txfs_shutdown(&tx);
    PASS();
}

static void test_create_op(void) {
    TEST("add create operation");
    txfs_t tx;
    txfs_init(&tx, NULL);
    txfs_begin(&tx);

    int rc = txfs_create(&tx, "/newfile.txt");
    CHECK(rc == 0, "create op should succeed");
    CHECK(tx.active_txn.ops[0].type == TXFS_OP_CREATE, "type should be CREATE");

    txfs_abort(&tx);
    txfs_shutdown(&tx);
    PASS();
}

static void test_delete_op(void) {
    TEST("add delete operation");
    txfs_t tx;
    txfs_init(&tx, NULL);
    txfs_begin(&tx);

    int rc = txfs_delete(&tx, "/oldfile.txt");
    CHECK(rc == 0, "delete op should succeed");
    CHECK(tx.active_txn.ops[0].type == TXFS_OP_DELETE, "type should be DELETE");

    txfs_abort(&tx);
    txfs_shutdown(&tx);
    PASS();
}

static void test_mkdir_op(void) {
    TEST("add mkdir operation");
    txfs_t tx;
    txfs_init(&tx, NULL);
    txfs_begin(&tx);

    int rc = txfs_mkdir(&tx, "/mydir");
    CHECK(rc == 0, "mkdir should succeed");
    CHECK(tx.active_txn.ops[0].type == TXFS_OP_MKDIR, "type should be MKDIR");

    txfs_abort(&tx);
    txfs_shutdown(&tx);
    PASS();
}

static void test_rename_op(void) {
    TEST("add rename operation");
    txfs_t tx;
    txfs_init(&tx, NULL);
    txfs_begin(&tx);

    int rc = txfs_rename(&tx, "/old.txt", "/new.txt");
    CHECK(rc == 0, "rename should succeed");
    CHECK(tx.active_txn.ops[0].type == TXFS_OP_RENAME, "type should be RENAME");
    CHECK(strcmp(tx.active_txn.ops[0].path2, "/new.txt") == 0, "dest path should match");

    txfs_abort(&tx);
    txfs_shutdown(&tx);
    PASS();
}

static void test_truncate_op(void) {
    TEST("add truncate operation");
    txfs_t tx;
    txfs_init(&tx, NULL);
    txfs_begin(&tx);

    int rc = txfs_truncate(&tx, "/bigfile.bin", 1024);
    CHECK(rc == 0, "truncate should succeed");
    CHECK(tx.active_txn.ops[0].type == TXFS_OP_TRUNCATE, "type should be TRUNCATE");
    CHECK(tx.active_txn.ops[0].offset == 1024, "size should be 1024");

    txfs_abort(&tx);
    txfs_shutdown(&tx);
    PASS();
}

/* -- Commit with Operations Tests ---------------------------- */

static void test_commit_with_ops(void) {
    TEST("commit with multiple ops writes to journal");
    txfs_t tx;
    txfs_init(&tx, NULL);
    txfs_begin(&tx);

    txfs_create(&tx, "/file.txt");
    const char *data = "WuBuOS transactional write";
    txfs_write(&tx, "/file.txt", 0, data, strlen(data));

    int rc = txfs_commit(&tx);
    CHECK(rc == 0, "commit should succeed");
    CHECK(tx.header.entry_count == 2, "should have 2 journal entries");
    CHECK(tx.header.committed_count == 2, "all entries committed");
    CHECK(tx.header.applied_count == 2, "all entries applied");
    CHECK(tx.txns_committed == 1, "1 txn committed");

    txfs_shutdown(&tx);
    PASS();
}

static void test_abort_discards_ops(void) {
    TEST("abort discards ops, journal unchanged");
    txfs_t tx;
    txfs_init(&tx, NULL);
    txfs_begin(&tx);

    txfs_create(&tx, "/temp.txt");
    txfs_write(&tx, "/temp.txt", 0, "temp", 4);
    txfs_abort(&tx);

    CHECK(tx.header.entry_count == 0, "journal should have 0 entries");
    CHECK(tx.txns_aborted == 1, "1 txn aborted");
    CHECK(tx.txns_committed == 0, "0 committed");

    txfs_shutdown(&tx);
    PASS();
}

/* -- Multi-Transaction Tests --------------------------------- */

static void test_multiple_transactions(void) {
    TEST("multiple sequential transactions");
    txfs_t tx;
    txfs_init(&tx, NULL);

    /* Txn 1 */
    txfs_begin(&tx);
    txfs_create(&tx, "/file1.txt");
    txfs_commit(&tx);

    /* Txn 2 */
    txfs_begin(&tx);
    txfs_create(&tx, "/file2.txt");
    txfs_write(&tx, "/file2.txt", 0, "data", 4);
    txfs_commit(&tx);

    /* Txn 3 (aborted) */
    txfs_begin(&tx);
    txfs_delete(&tx, "/file1.txt");
    txfs_abort(&tx);

    CHECK(tx.txns_committed == 2, "2 committed");
    CHECK(tx.txns_aborted == 1, "1 aborted");
    CHECK(tx.header.entry_count == 3, "3 journal entries (2 create + 1 write)");
    CHECK(tx.header.applied_count == 3, "3 applied");

    txfs_shutdown(&tx);
    PASS();
}

/* -- Max Ops Test -------------------------------------------- */

static void test_max_ops(void) {
    TEST("max ops per transaction");
    txfs_t tx;
    txfs_init(&tx, NULL);
    txfs_begin(&tx);

    char name[32];
    int added = 0;
    for (int i = 0; i < TXFS_MAX_TXN_OPS; i++) {
        snprintf(name, sizeof(name), "/file%d.txt", i);
        if (txfs_create(&tx, name) == 0) added++;
    }
    CHECK(added == TXFS_MAX_TXN_OPS, "should add max ops");

    /* One more should fail */
    int rc = txfs_create(&tx, "/overflow.txt");
    CHECK(rc == -1, "should fail on overflow");

    /* Commit  --  journal may be full before all 64 entries fit.
     * The commit should either succeed (if they fit) or fail gracefully. */
    rc = txfs_commit(&tx);
    /* If commit succeeded, all 64 entries should be in the journal.
     * If it failed, the journal should have fewer entries and txn should be aborted. */
    if (rc == 0) {
        CHECK(tx.header.entry_count == TXFS_MAX_TXN_OPS, "all ops should be committed");
    } else {
        /* Commit failed  --  journal was full. entry_count < 64. */
        CHECK(tx.header.entry_count < TXFS_MAX_TXN_OPS, "partial commit expected");
        CHECK(tx.txns_aborted == 1, "aborted count should be 1");
    }

    txfs_shutdown(&tx);
    PASS();
}

/* -- CRC32 Tests --------------------------------------------- */

static void test_crc32(void) {
    TEST("CRC32 basic");
    uint32_t c1 = txfs_crc32("", 0);
    CHECK(c1 == 0, "CRC32 of empty should be 0");

    uint32_t c2 = txfs_crc32("a", 1);
    CHECK(c2 != 0, "CRC32 of 'a' should be non-zero");

    /* CRC32 is deterministic */
    uint32_t c3 = txfs_crc32("Hello", 5);
    uint32_t c4 = txfs_crc32("Hello", 5);
    CHECK(c3 == c4, "CRC32 should be deterministic");

    /* Different inputs → different CRCs */
    uint32_t c5 = txfs_crc32("World", 5);
    CHECK(c3 != c5, "different inputs should have different CRCs");

    PASS();
}

static void test_crc32_entry_validation(void) {
    TEST("journal entry CRC validated on commit");
    txfs_t tx;
    txfs_init(&tx, NULL);
    txfs_begin(&tx);

    const char *data = "CRC test data";
    txfs_write(&tx, "/crc_test.txt", 0, data, strlen(data));
    txfs_commit(&tx);

    /* Verify the entry's data_crc matches recomputed CRC */
    txfs_journal_entry_t *entry = (txfs_journal_entry_t *)
        (tx.journal + sizeof(txfs_journal_header_t));
    uint32_t recomputed = txfs_crc32(entry->data, entry->size);
    CHECK(entry->data_crc == recomputed, "journal CRC should match recomputed");

    txfs_shutdown(&tx);
    PASS();
}

/* -- Recovery Tests ------------------------------------------ */

static void test_recover_nothing(void) {
    TEST("recover with no unapplied entries");
    txfs_t tx;
    txfs_init(&tx, NULL);
    txfs_begin(&tx);
    txfs_create(&tx, "/file.txt");
    txfs_commit(&tx);

    /* All committed entries are applied  --  recovery does nothing */
    int replayed = txfs_recover(&tx);
    CHECK(replayed == 0, "should replay 0 entries");
    CHECK(txfs_was_recovered(&tx) == 0, "should not report recovery");

    txfs_shutdown(&tx);
    PASS();
}

static void test_recover_after_crash(void) {
    TEST("recover after simulated crash (unapplied entries)");
    txfs_t tx;
    txfs_init(&tx, NULL);

    /* Commit a transaction */
    txfs_begin(&tx);
    txfs_create(&tx, "/survived.txt");
    txfs_write(&tx, "/survived.txt", 0, "data", 4);
    txfs_commit(&tx);

    /* Simulate crash: set applied_count < committed_count */
    tx.header.applied_count = 0;  /* Nothing applied yet! */

    /* Recover should replay the committed entries */
    int replayed = txfs_recover(&tx);
    CHECK(replayed == 2, "should replay 2 entries");
    CHECK(txfs_was_recovered(&tx) == 1, "should report recovery");
    CHECK(tx.header.applied_count == 2, "applied should be 2 after recovery");

    txfs_shutdown(&tx);
    PASS();
}

static void test_recover_crc_corruption(void) {
    TEST("recovery stops on CRC corruption");
    txfs_t tx;
    txfs_init(&tx, NULL);

    /* Commit two ops */
    txfs_begin(&tx);
    txfs_create(&tx, "/good.txt");
    const char *data = "good data";
    txfs_write(&tx, "/good.txt", 0, data, strlen(data));
    txfs_commit(&tx);

    /* Corrupt the second entry's data */
    txfs_journal_entry_t *entry2 = (txfs_journal_entry_t *)
        (tx.journal + sizeof(txfs_journal_header_t) + sizeof(txfs_journal_entry_t));
    entry2->data[0] ^= 0xFF;  /* Flip a byte */

    /* Simulate crash */
    tx.header.applied_count = 0;

    /* Recovery should replay first entry then stop on bad CRC */
    int replayed = txfs_recover(&tx);
    CHECK(replayed == 1, "should replay 1 entry, stop on corrupt");
    CHECK(tx.header.applied_count == 1, "applied should be 1");

    txfs_shutdown(&tx);
    PASS();
}

/* -- Journal Full Test --------------------------------------- */

static void test_journal_full(void) {
    TEST("journal full aborts commit");
    txfs_t tx;
    txfs_init(&tx, NULL);

    /* Calculate how many entries fit in the journal */
    uint32_t jmax = (TXFS_JOURNAL_SIZE - sizeof(txfs_journal_header_t))
                         / sizeof(txfs_journal_entry_t);

    /* Fill the journal */
    for (uint32_t i = 0; i < jmax; i++) {
        txfs_begin(&tx);
        char name[32];
        snprintf(name, sizeof(name), "/f%u.txt", i);
        txfs_create(&tx, name);
        txfs_commit(&tx);
    }

    /* Next transaction should fail */
    txfs_begin(&tx);
    txfs_create(&tx, "/overflow.txt");
    int rc = txfs_commit(&tx);
    CHECK(rc == -1, "commit should fail when journal full");
    printf("  INFO: entry_count=%u jmax=%u\n", tx.header.entry_count, jmax);
    fflush(stdout);
    CHECK(tx.header.entry_count == jmax, "journal should be full");

    txfs_shutdown(&tx);
    PASS();
}

/* -- Header Checksum Tests ----------------------------------- */

static void test_header_checksum(void) {
    TEST("journal header checksum is correct");
    txfs_t tx;
    txfs_init(&tx, NULL);
    txfs_begin(&tx);
    txfs_create(&tx, "/test.txt");
    txfs_commit(&tx);

    /* Verify header checksum matches recomputed */
    const uint8_t *p = (const uint8_t *)&tx.header;
    uint32_t computed = txfs_crc32(p, offsetof(txfs_journal_header_t, checksum));
    CHECK(tx.header.checksum == computed, "header checksum should match");

    txfs_shutdown(&tx);
    PASS();
}

/* -- Stats Tests --------------------------------------------- */

static void test_stats(void) {
    TEST("stats counters");
    txfs_t tx;
    txfs_init(&tx, NULL);
    CHECK(txfs_committed_total(&tx) == 0, "initial committed 0");
    CHECK(txfs_aborted_total(&tx) == 0, "initial aborted 0");

    txfs_begin(&tx);
    txfs_create(&tx, "/a.txt");
    txfs_commit(&tx);
    CHECK(txfs_committed_total(&tx) == 1, "1 committed");

    txfs_begin(&tx);
    txfs_abort(&tx);
    CHECK(txfs_aborted_total(&tx) == 1, "1 aborted");

    CHECK(txfs_committed_total(NULL) == 0, "NULL committed 0");
    CHECK(txfs_aborted_total(NULL) == 0, "NULL aborted 0");

    txfs_shutdown(&tx);
    PASS();
}

static void test_dump(void) {
    TEST("txfs_dump does not crash");
    txfs_t tx;
    txfs_init(&tx, NULL);
    txfs_begin(&tx);
    txfs_create(&tx, "/dump.txt");
    txfs_commit(&tx);
    txfs_dump(&tx);
    txfs_dump(NULL);
    txfs_shutdown(&tx);
    PASS();
}

/* -- Main ---------------------------------------------------- */

int main(void) {
    printf("+==================================================+\n");
    printf("|  WuBuOS Transactional FS Test Suite                |\n");
    printf("+==================================================+\n\n");

    /* Lifecycle */
    test_init();
    test_init_null();

    /* Transaction lifecycle */
    test_begin_commit();
    test_begin_abort();
    test_double_begin_fails();
    test_commit_without_begin();

    /* Operations */
    test_write_op();
    test_create_op();
    test_delete_op();
    test_mkdir_op();
    test_rename_op();
    test_truncate_op();

    /* Commit with ops */
    test_commit_with_ops();
    test_abort_discards_ops();

    /* Multi-transaction */
    test_multiple_transactions();
    test_max_ops();

    /* CRC32 */
    test_crc32();
    test_crc32_entry_validation();

    /* Recovery */
    test_recover_nothing();
    test_recover_after_crash();
    test_recover_crc_corruption();

    /* Journal limits */
    test_journal_full();

    /* Header checksum */
    test_header_checksum();

    /* Stats */
    test_stats();
    test_dump();

    printf("\n==================================================\n");
    printf("  Results: %d/%d passed, %d failed\n", g_pass, g_total, g_fail);
    printf("==================================================\n");

    return g_fail > 0 ? 1 : 0;
}
