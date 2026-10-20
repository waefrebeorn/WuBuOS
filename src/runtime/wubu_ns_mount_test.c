/*
 * wubu_ns_mount_test.c -- asserts the /n bind/union mount table contract
 * (NAMESPACE #582, #586, #597). Creates real temp dirs for the union
 * readdir merge.
 */
#include "wubu_ns_mount.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static int fails;
#define CHECK(cond) do { \
    if (!(cond)) { fails++; printf("FAIL: %s (line %d)\n", #cond, __LINE__); } \
} while (0)

#define R_WRITE 0x2ULL   /* WUBU_RIGHT_WRITE stand-in */

int main(void)
{
    system("rm -rf /tmp/wubu_ns_mount_t && mkdir -p /tmp/wubu_ns_mount_t/a "
           "/tmp/wubu_ns_mount_t/b");
    system("touch /tmp/wubu_ns_mount_t/a/alpha.txt "
           "/tmp/wubu_ns_mount_t/a/common.txt "
           "/tmp/wubu_ns_mount_t/b/beta.txt "
           "/tmp/wubu_ns_mount_t/b/common.txt");

    /* ---- table lifecycle ---- */
    wubu_ns_mount_table_t *t = wubu_ns_mount_table_new();
    CHECK(t != NULL);
    CHECK(wubu_ns_mount_bind_count(t) == 0);
    CHECK(wubu_ns_mount_table_new() != NULL);

    /* ---- #582: bind + resolve ---- */
    CHECK(wubu_ns_bind(t, "/store/apps", "/n/apps", 0) == 0);
    CHECK(wubu_ns_bind(t, "/store/games", "/n/apps/games", 0) == 0);
    char src[WUBU_NS_MOUNT_MAX_PATH];
    unsigned flags = 0;
    CHECK(wubu_ns_resolve(t, "/n/apps/foo.app", src, &flags) == 0);
    CHECK(strcmp(src, "/store/apps/foo.app") == 0);
    CHECK(wubu_ns_resolve(t, "/n/apps/games/quake", src, &flags) == 0);
    CHECK(strcmp(src, "/store/games/quake") == 0);  /* longest prefix wins */
    CHECK(wubu_ns_resolve(t, "/n/appsx", src, &flags) == -1);  /* aligned */
    CHECK(wubu_ns_resolve(t, "/n/none", src, &flags) == -1);

    /* duplicate non-union bind rejected */
    CHECK(wubu_ns_bind(t, "/other", "/n/apps", 0) == -2);
    /* unbind removes; resolve then fails */
    CHECK(wubu_ns_unbind(t, "/n/apps/games") == 1);
    CHECK(wubu_ns_resolve(t, "/n/apps/games/quake", src, &flags) == 0);
    CHECK(strcmp(src, "/store/apps/games/quake") == 0);
    CHECK(wubu_ns_unbind(t, "/n/ghost") == -1);

    /* ---- #586: read-only enforcement at the callback layer ---- */
    CHECK(wubu_ns_bind(t, "/store/cfg", "/n/cfg", WUBU_NS_MOUNT_RO) == 0);
    CHECK(wubu_ns_resolve(t, "/n/cfg/theme.conf", src, &flags) == 0);
    CHECK(flags & WUBU_NS_MOUNT_RO);
    CHECK(wubu_ns_mount_allows(t, "/n/cfg/theme.conf", R_WRITE) == 0);
    CHECK(wubu_ns_mount_allows(t, "/n/cfg/theme.conf", 0x1 /* READ */) == 1);
    CHECK(wubu_ns_mount_allows(t, "/n/apps/foo.app", R_WRITE) == 1);
    CHECK(wubu_ns_mount_allows(t, "/n/unbound", R_WRITE) == 0);

    /* ---- #597: union readdir merge (stacked layers, deduped) ---- */
    CHECK(wubu_ns_bind(t, "/tmp/wubu_ns_mount_t/a", "/n/union",
                       WUBU_NS_MOUNT_UNION) == 0);
    CHECK(wubu_ns_bind(t, "/tmp/wubu_ns_mount_t/b", "/n/union",
                       WUBU_NS_MOUNT_UNION) == 0);
    char names[WUBU_NS_MOUNT_MAX_ENTRIES][WUBU_NS_MOUNT_MAX_NAME];
    int n = wubu_ns_mount_readdir(t, "/n/union", names,
                                  WUBU_NS_MOUNT_MAX_ENTRIES);
    CHECK(n == 3);                                   /* alpha, common, beta */
    int has_alpha = 0, has_beta = 0, has_common = 0, dup_common = 0;
    for (int i = 0; i < n; i++) {
        if (strcmp(names[i], "alpha.txt") == 0) has_alpha = 1;
        if (strcmp(names[i], "beta.txt") == 0)  has_beta = 1;
        if (strcmp(names[i], "common.txt") == 0) has_common++;
    }
    dup_common = (has_common > 1);
    CHECK(has_alpha && has_beta && has_common == 1);
    CHECK(!dup_common);
    /* union resolution picks the FIRST-bound layer for the path */
    CHECK(wubu_ns_resolve(t, "/n/union/common.txt", src, &flags) == 0);
    CHECK(strcmp(src, "/tmp/wubu_ns_mount_t/a/common.txt") == 0);
    /* a union with one RO layer denies writes anywhere in the stack */
    CHECK(wubu_ns_mount_allows(t, "/n/union/common.txt", R_WRITE) == 1);
    CHECK(wubu_ns_unbind(t, "/n/union") == 2);        /* the whole stack */
    CHECK(wubu_ns_bind(t, "/tmp/wubu_ns_mount_t/a", "/n/ro",
                       WUBU_NS_MOUNT_UNION) == 0);
    CHECK(wubu_ns_bind(t, "/tmp/wubu_ns_mount_t/b", "/n/ro",
                       WUBU_NS_MOUNT_UNION | WUBU_NS_MOUNT_RO) == 0);
    CHECK(wubu_ns_mount_allows(t, "/n/ro/beta.txt", R_WRITE) == 0);
    CHECK(wubu_ns_unbind(t, "/n/ro") == 2);
    /* readdir on a missing union dir: no entries, not an error */
    CHECK(wubu_ns_bind(t, "/tmp/wubu_ns_mount_t/absent", "/n/hole",
                       WUBU_NS_MOUNT_UNION) == 0);
    CHECK(wubu_ns_mount_readdir(t, "/n/hole", names, 8) == 0);
    CHECK(wubu_ns_mount_readdir(t, "/n/unbound2", names, 8) == 0);
    CHECK(wubu_ns_mount_readdir(t, "/n/apps", names, 0) == -1);

    /* ---- fail-closed: full table + NULL guards ---- */
    wubu_ns_mount_table_t *full = wubu_ns_mount_table_new();
    char at[WUBU_NS_MOUNT_MAX_PATH];
    int rc = 0;
    for (int i = 0; i < WUBU_NS_MOUNT_MAX_BINDS + 2 && rc != -3; i++) {
        snprintf(at, sizeof(at), "/n/f%d", i);
        rc = wubu_ns_bind(full, "/s", at, 0);
    }
    CHECK(rc == -3);
    CHECK(wubu_ns_bind(NULL, "/s", "/n/x", 0) == -1);
    CHECK(wubu_ns_resolve(NULL, "/n", src, &flags) == -1);
    CHECK(wubu_ns_mount_allows(NULL, "/n", 0) == 0);
    CHECK(wubu_ns_mount_readdir(NULL, "/n", names, 8) == -1);
    CHECK(wubu_ns_unbind(NULL, "/n") == -1);

    wubu_ns_mount_table_free(full);
    wubu_ns_mount_table_free(t);
    system("rm -rf /tmp/wubu_ns_mount_t");

    if (fails == 0) printf("ALL PASSED\n");
    else printf("%d FAILURES\n", fails);
    return fails ? 1 : 0;
}
