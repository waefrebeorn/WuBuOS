/*
 * test_styxfs_path.c -- the path utilities (GOALS-1000 #732: styxfs_path
 * had no test target). Primary entries: build_path (namespace path join),
 * path_is_mounted (mount coverage with the boundary rule), count_children
 * (direct mount children only).
 */
#include "styxfs.h"

#include <stdio.h>
#include <string.h>

static int fails;
#define CHECK(cond) do { \
    if (!(cond)) { fails++; printf("FAIL: %s (line %d)\n", #cond, __LINE__); } \
} while (0)

int main(void)
{
    char buf[256];

    /* build_path: root joins without a double slash, others with one */
    build_path(buf, sizeof(buf), "/", "n");
    CHECK(strcmp(buf, "/n") == 0);
    build_path(buf, sizeof(buf), "/n", "kv");
    CHECK(strcmp(buf, "/n/kv") == 0);
    build_path(buf, sizeof(buf), "/n/kv", "x");
    CHECK(strcmp(buf, "/n/kv/x") == 0);

    /* a server with a root mount, a nested mount, and a sibling */
    styxfs_server_t srv;
    memset(&srv, 0, sizeof(srv));
    styxfs_mount_t m_root  = { "/n",     (char *)"/tmp/a", 0, NULL };
    styxfs_mount_t m_kv    = { "/n/kv",  (char *)"/tmp/b", 0, NULL };
    styxfs_mount_t m_deep  = { "/n/kv/x/y", (char *)"/tmp/c", 0, NULL };
    styxfs_mount_t m_side  = { "/n/other", (char *)"/tmp/d", 0, NULL };
    srv.mounts = &m_root;
    m_root.next = &m_kv;
    m_kv.next = &m_deep;
    m_deep.next = &m_side;

    /* path_is_mounted: exact and nested hits */
    CHECK(path_is_mounted(&srv, "/n") == 1);
    CHECK(path_is_mounted(&srv, "/n/kv") == 1);
    CHECK(path_is_mounted(&srv, "/n/kv/x/y/z") == 1);
    CHECK(path_is_mounted(&srv, "/n/other") == 1);
    /* boundary rule: /n/kv2 is NOT under /n/kv */
    CHECK(path_is_mounted(&srv, "/n/kv2") == 1);   /* still under /n */
    /* a path outside every mount */
    CHECK(path_is_mounted(&srv, "/elsewhere") == 0);
    CHECK(path_is_mounted(&srv, "/n3") == 0);

    /* the ROOT mount covers the whole namespace */
    styxfs_server_t rsrv;
    memset(&rsrv, 0, sizeof(rsrv));
    styxfs_mount_t m_only_root = { "/", (char *)"/tmp/root", 0, NULL };
    rsrv.mounts = &m_only_root;
    CHECK(path_is_mounted(&rsrv, "/anything") == 1);
    CHECK(path_is_mounted(&rsrv, "/deep/path") == 1);

    /* count_children: direct children of /n are kv + other; the deep
     * mount /n/kv/x/y is NOT a direct child of /n */
    CHECK(count_children(&srv, "/n") == 2);
    CHECK(count_children(&srv, "/n/kv") == 0);
    /* /n/kv/x/y IS a direct child of /n/kv/x (one level below it) */
    CHECK(count_children(&srv, "/n/kv/x") == 1);
    CHECK(count_children(&srv, "/n/kv/x/y") == 0);
    /* an empty mount list counts nothing */
    styxfs_server_t esrv;
    memset(&esrv, 0, sizeof(esrv));
    CHECK(count_children(&esrv, "/n") == 0);
    CHECK(path_is_mounted(&esrv, "/n") == 0);

    if (fails == 0) printf("ALL PASSED\n");
    else printf("%d FAILURES\n", fails);
    return fails ? 1 : 0;
}
