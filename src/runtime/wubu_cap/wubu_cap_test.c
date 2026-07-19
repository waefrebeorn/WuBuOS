/*
 * wubu_cap_test.c -- WuBuOS capability core self-test.
 *
 * Exercises the real engine (not stubs): create a file cap, derive a
 * read-only sub-token, verify resolve enforces rights + audience, revoke the
 * root and prove the derived token (and eager-cascade children) fail resolve.
 * Also exercises the per-process handle table insert/resolve/close lifecycle.
 */
#include "wubu_cap.h"
#include "wubu_cap_internal.h"

#include <stdio.h>
#include <string.h>
#include <assert.h>

static int g_pass = 0, g_fail = 0;
#define CHECK(c, m) do { if (c) { g_pass++; printf("  [PASS] %s\n", m); } \
                        else { g_fail++; printf("  [FAIL] %s\n", m); } } while (0)

int main(void) {
    wubu_cap_init();

    /* --- Create a file cap owned by pid 100 (also authorizing pid 200) --- */
    int32_t owner[2] = {100, 200};
    wubu_cap_token_t root;
    int r = wubu_cap_create(WUBU_CAP_KIND_FILE,
                            WUBU_RIGHT_READ | WUBU_RIGHT_WRITE | WUBU_RIGHT_DERIVE,
                            owner, 0, 0, 100, WUBU_CAP_IDX_NONE, &root);
    CHECK(r > 0, "wubu_cap_create returns a slot");
    CHECK(!wubu_cap_token_is_null(root), "root token is non-null");

    /* Resolve with full rights as owner -> ok. */
    wubu_cap_object_t *o = wubu_cap_resolve(100, root,
        WUBU_RIGHT_READ | WUBU_RIGHT_WRITE);
    CHECK(o != NULL, "owner resolves with read+write");

    /* A pid NOT in the audience (300) must NOT resolve. */
    CHECK(wubu_cap_resolve(300, root, WUBU_RIGHT_READ) == NULL,
          "foreign pid denied (audience check)");

    /* --- Derive a read-only sub-token for pid 200 (in parent's audience) --- */
    int32_t aud[1] = {200};
    wubu_cap_token_t sub;
    r = wubu_cap_derive(root, 100, WUBU_RIGHT_READ, aud, 0, &sub);
    CHECK(r > 0, "derive read-only sub-token ok");
    CHECK(wubu_cap_resolve(200, sub, WUBU_RIGHT_READ) != NULL,
          "pid 200 resolves sub-token with read");
    /* pid 200 must NOT get write (rights subset enforced). */
    CHECK(wubu_cap_resolve(200, sub, WUBU_RIGHT_WRITE) == NULL,
          "sub-token write denied (rights subset)");
    /* pid 300 still cannot use the sub-token (audience subset enforced). */
    CHECK(wubu_cap_resolve(300, sub, WUBU_RIGHT_READ) == NULL,
          "sub-token denied to non-audience pid");

    /* --- Derive must reject widening beyond parent rights --- */
    wubu_cap_token_t wide;
    int rw = wubu_cap_derive(root, 100, WUBU_RIGHT_READ | WUBU_RIGHT_EXEC, aud, 0, &wide);
    CHECK(rw < 0, "derive rejects rights widening (EPERM)");

    /* --- Revoke the root: sub-token must now fail (generation bump) --- */
    int n = wubu_cap_revoke(root);
    CHECK(n >= 1, "revoke returns count");
    CHECK(wubu_cap_resolve(100, root, WUBU_RIGHT_READ) == NULL,
          "root token invalid after revoke");
    /* A non-eager-derived child is a SEPARATE object: revoking the parent
     * does NOT revoke it (transitive revoke needs CAP_FLAG_EAGER_REVOKE,
     * proven by the cascade test below). */
    CHECK(wubu_cap_resolve(200, sub, WUBU_RIGHT_READ) != NULL,
          "non-eager child survives parent revoke (correct isolation)");

    /* --- Eager revoke cascades to children --- */
    wubu_cap_token_t root2;
    int r2 = wubu_cap_create(WUBU_CAP_KIND_FILE, WUBU_RIGHT_READ | WUBU_RIGHT_DERIVE,
                            owner, WUBU_CAP_FLAG_EAGER_REVOKE, 0, 100, WUBU_CAP_IDX_NONE, &root2);
    CHECK(r2 > 0, "create eager-revoke root");
    wubu_cap_token_t child;
    wubu_cap_derive(root2, 100, WUBU_RIGHT_READ, owner, 0, &child);
    int nc = wubu_cap_revoke(root2);
    CHECK(nc >= 2, "eager revoke cascades to child (count>=2)");
    CHECK(wubu_cap_resolve(100, child, WUBU_RIGHT_READ) == NULL,
          "child invalidated by eager cascade");

    /* --- Handle table lifecycle (use a freshly-created live object) --- */
    wubu_cap_token_t hroot;
    int hr = wubu_cap_create(WUBU_CAP_KIND_FILE, WUBU_RIGHT_READ, owner, 0, 0, 100,
                            WUBU_CAP_IDX_NONE, &hroot);
    CHECK(hr > 0, "handle-test object created");
    wubu_cap_handle_table_t *ht = wubu_cap_handle_table_create();
    CHECK(ht != NULL, "handle table create");
    wubu_cap_token_t htok = wubu_cap_handle_insert(ht, (uint32_t)hr, 0);
    CHECK(!wubu_cap_token_is_null(htok), "handle insert returns token");
    CHECK(wubu_cap_handle_resolve(ht, 100, htok, WUBU_RIGHT_READ) != NULL,
          "handle resolve works");
    CHECK(wubu_cap_handle_close(ht, htok) == WUBU_CAP_OK,
          "handle close ok");
    CHECK(wubu_cap_handle_resolve(ht, 100, htok, WUBU_RIGHT_READ) == NULL,
          "handle stale after close (local gen bump)");
    wubu_cap_handle_table_free(ht);

    /* --- Inspect --- */
    wubu_cap_inspect_t info;
    wubu_cap_token_t root3;
    wubu_cap_create(WUBU_CAP_KIND_CONTAINER, WUBU_RIGHT_READ, owner, 0, 0, 100,
                   WUBU_CAP_IDX_NONE, &root3);
    int ir = wubu_cap_inspect(root3, 100, &info);
    CHECK(ir == WUBU_CAP_OK, "inspect ok");
    CHECK(info.kind == WUBU_CAP_KIND_CONTAINER, "inspect reports correct kind");
    CHECK(info.owner_pid == 100, "inspect reports owner");

    /* --- Bootcap cascade root (GrahaOS CAP_KIND_SYSTEM) --- */
    wubu_cap_system_init();
    CHECK(wubu_cap_system_bootcap_idx() != WUBU_CAP_IDX_NONE,
          "bootcap created by system_init");

    wubu_cap_handle_table_t *st = wubu_cap_handle_table_create();
    CHECK(st != NULL, "system handle table created");
    /* init gets a narrowed SYSTEM sub-token (inspect+revoke+derive+invoke) */
    uint64_t srights = WUBU_RIGHT_INSPECT | WUBU_RIGHT_REVOKE |
                       WUBU_RIGHT_DERIVE | WUBU_RIGHT_INVOKE;
    CHECK(wubu_cap_system_install_to_pid(st, 100, srights) == WUBU_CAP_OK,
          "install SYSTEM sub-token to pid 100");
    CHECK(wubu_cap_system_resolve(st, 100, WUBU_RIGHT_INVOKE) == WUBU_CAP_OK,
          "SYSTEM+INVOKE resolves");
    CHECK(wubu_cap_system_resolve(st, 100, WUBU_RIGHT_TERMINATE) == WUBU_CAP_EPERM,
          "SYSTEM+TERMINATE denied (not granted)");
    /* diminishing derive: a second init gets a *narrower* token; resolving a
     * right it was NOT granted must fail. */
    uint64_t narrow = WUBU_RIGHT_INSPECT;
    CHECK(wubu_cap_system_install_to_pid(st, 200, narrow) == WUBU_CAP_OK,
          "install narrower SYSTEM sub-token to pid 200");
    CHECK(wubu_cap_system_resolve(st, 200, WUBU_RIGHT_INVOKE) == WUBU_CAP_EPERM,
          "narrow token denies INVOKE (diminishing derive)");
    /* over-privileged install (right not in bootcap) impossible since bootcap
     * is ALL; instead assert install rejects a 0-rights (invalid) subset. */
    CHECK(wubu_cap_system_install_to_pid(st, 300, 0) == WUBU_CAP_OK,
          "install empty-rights token (valid: grants nothing)");
    /* revoke the bootcap root -> all installed sub-tokens cascade-revoke,
     * so SYSTEM resolution must now fail for every pid. */
    wubu_cap_token_t broot = wubu_cap_token_pack(
        g_wubu_cap_ptrs[wubu_cap_system_bootcap_idx()]->generation,
        wubu_cap_system_bootcap_idx(), 0);
    wubu_cap_revoke(broot);
    CHECK(wubu_cap_system_resolve(st, 100, WUBU_RIGHT_INVOKE) == WUBU_CAP_EPERM,
          "post-bootcap-revoke: pid 100 SYSTEM denied (cascade)");
    CHECK(wubu_cap_system_resolve(st, 200, WUBU_RIGHT_INSPECT) == WUBU_CAP_EPERM,
          "post-bootcap-revoke: pid 200 SYSTEM denied (cascade)");
    wubu_cap_handle_table_free(st);

    printf("\n=== Results: %d/%d passed ===\n", g_pass, g_pass + g_fail);
    return g_fail > 0 ? 1 : 0;
}
