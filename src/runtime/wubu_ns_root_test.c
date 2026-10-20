/*
 * wubu_ns_root_test.c -- asserts the per-process /n view contract
 * (NAMESPACE #583, #599, #600, #601, #602).
 */
#include "wubu_ns_root.h"

#include <stdio.h>
#include <string.h>

static int fails;
#define CHECK(cond) do { \
    if (!(cond)) { fails++; printf("FAIL: %s (line %d)\n", #cond, __LINE__); } \
} while (0)

/* Minimal rights stand-ins (the real masks live in wubu_cap.h; a grant
 * only carries the bits it was given). */
#define R_READ  0x1ULL
#define R_WRITE 0x2ULL

int main(void)
{
    /* ---- create / basic accessors ---- */
    wubu_ns_root_t *host = wubu_ns_root_create("host", WUBU_NS_ROOT_HOST);
    CHECK(host != NULL);
    CHECK(strcmp(wubu_ns_root_name(host), "host") == 0);
    CHECK(wubu_ns_root_class(host) == WUBU_NS_ROOT_HOST);
    CHECK(wubu_ns_root_grant_count(host) == 0);
    CHECK(wubu_ns_root_create(NULL, 0) == NULL);

    /* ---- #583: grant + lookup, longest prefix, fail-closed ---- */
    CHECK(wubu_ns_root_grant(host, "/store/dev", "/n/dev", R_READ, 0) == 0);
    CHECK(wubu_ns_root_grant(host, "/store/dev/net", "/n/dev/net",
                             R_READ | R_WRITE, 0) == 0);
    uint64_t rights = 0;
    char src[WUBU_NS_ROOT_MAX_PATH];
    CHECK(wubu_ns_root_lookup(host, "/n/dev/net/eth0", &rights, src) == 0);
    CHECK(rights == (R_READ | R_WRITE));           /* longest prefix wins */
    CHECK(strcmp(src, "/store/dev/net/eth0") == 0);
    CHECK(wubu_ns_root_lookup(host, "/n/dev", &rights, src) == 0);
    CHECK(rights == R_READ);
    CHECK(strcmp(src, "/store/dev") == 0);
    /* component alignment: /n/dev must NOT match /n/devices */
    CHECK(wubu_ns_root_lookup(host, "/n/devices", &rights, src) == -1);
    CHECK(wubu_ns_root_lookup(host, "/n/absent", &rights, src) == -1);
    /* allows(): the effective rights gate */
    CHECK(wubu_ns_root_allows(host, "/n/dev/net/eth0", R_WRITE) == 1);
    CHECK(wubu_ns_root_allows(host, "/n/dev", R_WRITE) == 0);
    CHECK(wubu_ns_root_allows(host, "/n/nope", R_READ) == 0);

    /* duplicate mount point rejected; revoke removes exactly that grant */
    CHECK(wubu_ns_root_grant(host, "/other", "/n/dev", R_READ, 0) == -2);
    CHECK(wubu_ns_root_grant(host, "/other", "/n/tmp", R_READ, 0) == 0);
    CHECK(wubu_ns_root_revoke(host, "/n/tmp") == 0);
    CHECK(wubu_ns_root_revoke(host, "/n/tmp") == -1);
    CHECK(wubu_ns_root_lookup(host, "/n/tmp/x", &rights, src) == -1);

    /* ---- #600: container view hides host control-plane grants ---- */
    wubu_ns_root_t *box = wubu_ns_root_create("box", WUBU_NS_ROOT_CONTAINER);
    CHECK(box != NULL);
    CHECK(wubu_ns_root_grant(box, "/store/dev", "/n/dev", R_READ, 0) == 0);
    CHECK(wubu_ns_root_grant(box, "/host/control", "/n/hostctl", R_READ,
                             WUBU_NS_GRANT_HOST_CP) == 0);
    CHECK(wubu_ns_root_lookup(box, "/n/dev", &rights, src) == 0);
    CHECK(wubu_ns_root_lookup(box, "/n/hostctl", &rights, src) == -1);
    /* the host view still sees its own control plane */
    wubu_ns_root_t *host2 = wubu_ns_root_create("h2", WUBU_NS_ROOT_HOST);
    CHECK(wubu_ns_root_grant(host2, "/host/control", "/n/hostctl", R_READ,
                             WUBU_NS_GRANT_HOST_CP) == 0);
    CHECK(wubu_ns_root_lookup(host2, "/n/hostctl", &rights, src) == 0);

    /* ---- #601: fork inherits; exec drops only EPHEMERAL ---- */
    CHECK(wubu_ns_root_grant(box, "/store/eph", "/n/eph", R_READ,
                             WUBU_NS_GRANT_EPHEMERAL) == 0);
    wubu_ns_root_t *child = wubu_ns_root_fork(box);
    CHECK(child != NULL);
    CHECK(wubu_ns_root_class(child) == WUBU_NS_ROOT_CONTAINER);
    CHECK(wubu_ns_root_grant_count(child) == wubu_ns_root_grant_count(box));
    CHECK(wubu_ns_root_lookup(child, "/n/dev", &rights, src) == 0);
    CHECK(wubu_ns_root_lookup(child, "/n/eph", &rights, src) == 0);
    CHECK(wubu_ns_root_exec(child) == 1);          /* the ephemeral one */
    CHECK(wubu_ns_root_lookup(child, "/n/eph", &rights, src) == -1);
    CHECK(wubu_ns_root_lookup(child, "/n/dev", &rights, src) == 0);
    CHECK(wubu_ns_root_exec(child) == 0);          /* nothing left to drop */

    /* ---- #602: rebase prefixes every mount point ---- */
    CHECK(wubu_ns_root_rebase(child, "/n/jail7") == 0);
    CHECK(wubu_ns_root_lookup(child, "/n/jail7/n/dev", &rights, src) == 0);
    CHECK(strcmp(src, "/store/dev") == 0);
    CHECK(wubu_ns_root_lookup(child, "/n/dev", &rights, src) == -1);

    /* ---- #599: explicit cross-view share; forged handles fail ---- */
    wubu_ns_share_t *sh = wubu_ns_root_share(box, "/n/dev");
    CHECK(sh != NULL);
    wubu_ns_root_t *peer = wubu_ns_root_create("peer", WUBU_NS_ROOT_CONTAINER);
    CHECK(peer != NULL);
    CHECK(wubu_ns_root_adopt(peer, sh, "/n/shared") == 0);
    CHECK(wubu_ns_root_lookup(peer, "/n/shared", &rights, src) == 0);
    CHECK(rights == R_READ);                        /* capped by the handle */
    /* forged / zeroed handle is rejected */
    wubu_ns_share_t forged;
    memset(&forged, 0, sizeof(forged));
    CHECK(wubu_ns_root_adopt(peer, &forged, "/n/no") == -2);
    /* no handle for an ungranted path */
    CHECK(wubu_ns_root_share(box, "/n/ghost") == NULL);
    wubu_ns_share_free(sh);

    /* ---- fail-closed: full table, NULL guards ---- */
    wubu_ns_root_t *full = wubu_ns_root_create("full", WUBU_NS_ROOT_HOST);
    char at[WUBU_NS_ROOT_MAX_PATH];
    int rc = 0;
    for (int i = 0; i < WUBU_NS_ROOT_MAX_GRANTS + 2 && rc != -3; i++) {
        snprintf(at, sizeof(at), "/n/g%d", i);
        rc = wubu_ns_root_grant(full, "/s", at, R_READ, 0);
    }
    CHECK(rc == -3);
    CHECK(wubu_ns_root_grant(NULL, "/s", "/n/x", 0, 0) == -1);
    CHECK(wubu_ns_root_lookup(NULL, "/n", &rights, src) == -1);
    CHECK(wubu_ns_root_allows(NULL, "/n", R_READ) == 0);
    CHECK(wubu_ns_root_fork(NULL) == NULL);
    CHECK(wubu_ns_root_exec(NULL) == -1);
    CHECK(wubu_ns_root_rebase(NULL, "/n") == -1);
    CHECK(wubu_ns_root_share(NULL, "/n") == NULL);
    CHECK(wubu_ns_root_adopt(peer, NULL, "/n") == -1);

    wubu_ns_root_free(full);
    wubu_ns_root_free(peer);
    wubu_ns_root_free(child);
    wubu_ns_root_free(host2);
    wubu_ns_root_free(box);
    wubu_ns_root_free(host);

    if (fails == 0) printf("ALL PASSED\n");
    else printf("%d FAILURES\n", fails);
    return fails ? 1 : 0;
}
