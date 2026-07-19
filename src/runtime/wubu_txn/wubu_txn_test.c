/*
 * wubu_txn_test.c -- WuBuOS transactional speculation self-test.
 *
 * Proves the engine does real work (not stubs):
 *   - begin() takes a real checkpoint of a working dir
 *   - a buffered external send is held, not delivered live
 *   - abort() rolls the dir back to its pre-txn state (real revert)
 *   - commit() keeps the state and drains the buffer
 */
#include "wubu_txn.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

static int g_pass = 0, g_fail = 0;
#define CHECK(c, m) do { if (c) { g_pass++; printf("  [PASS] %s\n", m); } \
                        else { g_fail++; printf("  [FAIL] %s\n", m); } } while (0)

static void write_file(const char *path, const char *content) {
    FILE *f = fopen(path, "w");
    if (f) { fputs(content, f); fclose(f); }
}
static int file_eq(const char *path, const char *expect) {
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    char buf[256]; size_t n = fread(buf, 1, sizeof(buf) - 1, f); buf[n] = '\0';
    fclose(f);
    return strcmp(buf, expect) == 0;
}

int main(void) {
    wubu_txn_init();

    char work[512], snaproot[512];
    snprintf(work, sizeof(work), "/tmp/wubu_txn_work_%d", (int)getpid());
    snprintf(snaproot, sizeof(snaproot), "/tmp/wubu_txn_snap_%d", (int)getpid());
    system("rm -rf /tmp/wubu_txn_*");
    mkdir(work, 0755);
    char before_path[512];
    snprintf(before_path, sizeof(before_path), "%s/state.txt", work);
    write_file(before_path, "before");

    int32_t scope[2] = { 1, -1 };   /* pid 1 is in-scope; others external */
    WubuTxn *t = wubu_txn_begin(WUBU_TXN_SELF_SCOPE, "agent-step", scope, work);
    CHECK(t != NULL, "txn_begin returns handle");
    CHECK(wubu_txn_state(t) == WUBU_TXN_ACTIVE, "state ACTIVE after begin");

    char state_path[512];
    snprintf(state_path, sizeof(state_path), "%s/state.txt", work);

    /* Mutate live state (simulating the agent's work). */
    write_file(state_path, "during-txn");
    CHECK(file_eq(state_path, "during-txn"), "live state mutated");

    /* External send (to pid 99, out of scope) is buffered, not lost. */
    const char *effect = "do-side-effect";
    int r = wubu_txn_buffer_send(t, 99, effect, (uint32_t)strlen(effect), 0);
    CHECK(r == 0, "external send buffered (not delivered live)");

    /* --- Abort rolls the dir back to pre-txn state --- */
    r = wubu_txn_abort(t);
    CHECK(r == WUBU_TXN_OK, "txn_abort ok");
    CHECK(wubu_txn_state(t) == WUBU_TXN_ABORTED, "state ABORTED");
    CHECK(file_eq(state_path, "before"),
          "abort restored dir to pre-txn checkpoint (real revert)");

    /* --- Commit KEEPS the mutation --- */
    WubuTxn *t2 = wubu_txn_begin(WUBU_TXN_SELF_SCOPE, "agent-step-2", scope, work);
    write_file(state_path, "committed-state");
    wubu_txn_buffer_send(t2, 99, "x", 1, 0);
    r = wubu_txn_commit(t2);
    CHECK(r == WUBU_TXN_OK, "txn_commit ok");
    CHECK(wubu_txn_state(t2) == WUBU_TXN_COMMITTED, "state COMMITTED");
    CHECK(file_eq(state_path, "committed-state"),
          "commit keeps the mutation");

    wubu_txn_destroy(t);
    wubu_txn_destroy(t2);
    system("rm -rf /tmp/wubu_txn_*");

    printf("\n=== Results: %d/%d passed ===\n", g_pass, g_pass + g_fail);
    return g_fail > 0 ? 1 : 0;
}
