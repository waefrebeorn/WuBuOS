/* wubu_ntsync_test.c -- gate for the keyed-event pairing primitive.
 * Proves: held-release, block-then-release, key isolation, consume-once
 * (one token wakes one waiter), and N:M concurrent pairing. */
#include "wubu_ntsync.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static int g_pass, g_fail;
#define CHECK(cond, msg) do { \
    if (cond) { g_pass++; } \
    else { g_fail++; printf("FAIL: %s (line %d)\n", msg, __LINE__); } \
} while (0)

typedef struct { wubu_ntsync_t *ke; uint64_t key; int rc; } waiter_arg_t;

static void *waiter(void *p) {
    waiter_arg_t *a = p;
    a->rc = wubu_ntsync_wait(a->ke, a->key, -1);
    return NULL;
}

typedef struct { wubu_ntsync_t *ke; uint64_t key; int rc; } rel_arg_t;
static void *releaser_run(void *p) {
    rel_arg_t *a = p;
    a->rc = wubu_ntsync_release(a->ke, a->key, 1, -1); /* wait=1 */
    return NULL;
}

#define NWAIT 64
static waiter_arg_t g_wa[NWAIT];
static pthread_t     g_th[NWAIT];

int main(void) {
    wubu_ntsync_t *ke = wubu_ntsync_create();
    CHECK(ke != NULL, "create");

    /* 1. Release-before-wait: release holds the token; wait consumes immediately. */
    CHECK(wubu_ntsync_release(ke, 0x111, 0, 0) == WUBU_NTSYNC_OK, "held release offer");
    CHECK(wubu_ntsync_wait(ke, 0x111, 0) == WUBU_NTSYNC_OK, "held release consumed");

    /* Poll after consume must time out (token consumed exactly once). */
    CHECK(wubu_ntsync_wait(ke, 0x111, 0) == WUBU_NTSYNC_TIMEOUT, "consume-once");

    /* 2. Block-then-release: waiter blocks, main releases, waiter acquires. */
    {
        waiter_arg_t a = {ke, 0x222, -99};
        pthread_t t;
        CHECK(pthread_create(&t, NULL, waiter, &a) == 0, "spawn waiter");
        usleep(20000); /* let it block (no token yet) */
        CHECK(wubu_ntsync_release(ke, 0x222, 0, 0) == WUBU_NTSYNC_OK, "release wakes waiter");
        pthread_join(t, NULL);
        CHECK(a.rc == WUBU_NTSYNC_OK, "waiter acquired after block");
    }

    /* 3. Key isolation: a token on A must not satisfy a wait on B. */
    CHECK(wubu_ntsync_release(ke, 0x333, 0, 0) == WUBU_NTSYNC_OK, "offer A");
    CHECK(wubu_ntsync_wait(ke, 0x333, 0) == WUBU_NTSYNC_OK, "consume A");
    CHECK(wubu_ntsync_release(ke, 0x333, 0, 0) == WUBU_NTSYNC_OK, "offer A again");
    CHECK(wubu_ntsync_wait(ke, 0x444, 0) == WUBU_NTSYNC_TIMEOUT, "B does not see A");
    CHECK(wubu_ntsync_wait(ke, 0x333, 0) == WUBU_NTSYNC_OK, "A still there for the right waiter");

    /* 4. N:M concurrent pairing: every waiter must acquire. */
    for (int i = 0; i < NWAIT; i++) {
        g_wa[i].ke = ke; g_wa[i].key = 0x555; g_wa[i].rc = -99;
        pthread_create(&g_th[i], NULL, waiter, &g_wa[i]);
    }
    usleep(30000); /* waiters are all blocked on key 0x555 */
    for (int i = 0; i < NWAIT; i++)
        CHECK(wubu_ntsync_release(ke, 0x555, 0, 0) == WUBU_NTSYNC_OK, "offer");
    int consumed = 0;
    for (int i = 0; i < NWAIT; i++) {
        pthread_join(g_th[i], NULL);
        if (g_wa[i].rc == WUBU_NTSYNC_OK) consumed++;
    }
    CHECK(consumed == NWAIT, "all N waiters paired with N releases");

    /* 5. Blocking releaser (wait=1) unblocks only when a waiter consumes it. */
    {
        struct rel_arg { wubu_ntsync_t *ke; uint64_t key; int rc; } ra = {ke, 0x666, -99};
        pthread_t rt;
        CHECK(pthread_create(&rt, NULL, releaser_run, &ra) == 0, "spawn releaser");
        usleep(20000); /* releaser offered a token and is now blocked */
        CHECK(wubu_ntsync_wait(ke, 0x666, 500) == WUBU_NTSYNC_OK, "waiter consumes held release");
        pthread_join(rt, NULL);
        CHECK(ra.rc == WUBU_NTSYNC_OK, "blocking releaser unblocked after consume");
    }

    wubu_ntsync_destroy(ke);
    printf("wubu_ntsync: PASS %d checks, FAIL %d\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}