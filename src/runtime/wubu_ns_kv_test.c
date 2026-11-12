/*
 * wubu_ns_kv_test.c -- prove the /n/kv subtree is publishable and that the
 * tick paths a 9P client would open actually RESOLVE in the KV-FS (the read
 * half of the AGI loop: the Styx server routes /n/kv/... reads to the live
 * tensor, which only works if the path exists and resolves).
 */
#include "../egojar.h"
#include "wubu_ns_kv.h"
#include "wubu_ns_bridge_internal.h"
#include "../kernel/wubu_kvfs.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define NSROOT "/tmp/wubu_kvns"

static int failures;
static int checks;
#define CHECK(c, m) do { \
    checks++; \
    if (!(c)) { printf("  FAIL: %s\n", m); failures++; } \
} while (0)

static int path_is_dir(const char *p)
{
    struct stat st;
    return stat(p, &st) == 0 && S_ISDIR(st.st_mode);
}

static int path_is_file(const char *p)
{
    struct stat st;
    return stat(p, &st) == 0 && S_ISREG(st.st_mode);
}

int main(void)
{
    printf("\n=== /n/kv namespace ===\n");

    char cmd[256];
    snprintf(cmd, sizeof(cmd), "rm -rf %s", NSROOT);
    if (system(cmd) != 0) { /* best effort */ }
    mkdir(NSROOT, 0755);
    *(const char **)&g_ns_root = NSROOT;

    CHECK(wubu_kvfs_namespace_init() == 0, "the KV-FS mounts");
    CHECK(wubu_ns_publish_kv() == 0, "/n/kv publishes");

    /* kv/world must be a DIRECTORY: the tick files are its children. A flat
     * file here is what made /n/kv/world/tick_<N> unaddressable. */
    char p[300];
    snprintf(p, sizeof(p), "%s/kv/world", NSROOT);
    CHECK(path_is_dir(p), "kv/world is a directory");

    /* ticks are created by the play loop as it writes them */
    CHECK(wubu_ns_kv_touch(0) == 0, "tick 0 is touched");
    CHECK(wubu_ns_kv_touch(7) == 0, "tick 7 is touched");
    snprintf(p, sizeof(p), "%s/kv/world/tick_0", NSROOT);
    CHECK(path_is_file(p), "kv/world/tick_0 exists as a file");
    snprintf(p, sizeof(p), "%s/kv/world/tick_7", NSROOT);
    CHECK(path_is_file(p), "kv/world/tick_7 exists as a file");

    /* the snapshot is JSON */
    snprintf(p, sizeof(p), "%s/kv/snapshot", NSROOT);
    FILE *f = fopen(p, "r");
    CHECK(f != NULL, "kv/snapshot exists");
    if (f) {
        char buf[512];
        size_t n = fread(buf, 1, sizeof(buf) - 1, f);
        buf[n] = '\0';
        fclose(f);
        CHECK(strchr(buf, '{') != NULL, "kv/snapshot is JSON");
        CHECK(strstr(buf, "/kv/world") != NULL, "kv/snapshot names the world mount");
    }

    /* THE READ PATH: the exact call the Styx server makes for a 9P read.
     * route_path must find /kv/ under the ns root, and lookup must resolve
     * the tick path to a block+offset in the tensor. */
    char disk[300];
    snprintf(disk, sizeof(disk), "%s/kv/world/tick_0", NSROOT);
    const char *kv = wubu_kvfs_route_path(disk, NSROOT);
    CHECK(kv != NULL, "route_path finds /kv/ under the ns root");
    if (kv) {
        CHECK(strcmp(kv, "/kv/world/tick_0") == 0, "route_path returns the KV path");
        uint32_t blk = 0;
        size_t off = 0;
        CHECK(wubu_kvfs_lookup(g_wubu_kvfs, kv, &blk, &off) == 0,
              "the tick path resolves in the KV-FS");
    }

    /* a path outside /kv/ must not be routed */
    snprintf(disk, sizeof(disk), "%s/world/state", NSROOT);
    CHECK(wubu_kvfs_route_path(disk, NSROOT) == NULL,
          "a non-KV path is not routed");

    printf("%s: %d checks, %d failures\n", failures ? "FAIL" : "PASS", checks, failures);
    return failures ? 1 : 0;
}
