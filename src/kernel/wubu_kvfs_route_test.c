/*
 * wubu_kvfs_route_test.c — proves THE DOCTRINE end-to-end:
 *
 *   The OS state IS the KV cache, served as 9P.
 *
 *   /kv/world/tick_N is written into the kernel KV tensor, and the same
 *   bytes come back when a Styx/9P read of "<ns_root>/kv/world/tick_N"
 *   routes through wubu_kvfs_route_path + wubu_kvfs_route_read — NOT the
 *   on-disk placeholder. The filesystem and the KV cache are the SAME store.
 *
 * Run: src/kernel/wubu_kvfs_route_test
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>

#include "wubu_kvfs.h"

static int failures = 0;
#define CHECK(cond, msg) \
    do { if (!(cond)) { printf("  FAIL: %s\n", msg); failures++; } \
       else { printf("  ok:  %s\n", msg); } } while(0)

int main(void)
{
    printf("=== wubu_kvfs_route_test (the OS IS the KV cache, served as 9P) ===\n");

    /* init the kernel singleton the route helpers read/write */
    if (g_wubu_kvfs) { wubu_kvfs_free(g_wubu_kvfs); g_wubu_kvfs = NULL; }
    if (g_wubu_kv_base) { free(g_wubu_kv_base); g_wubu_kv_base = NULL; }
    g_wubu_kv_capacity = 0;

    int rc = wubu_kvfs_kernel_init(256, 4096);   /* 4M floats, 16 MB */
    CHECK(rc == 0, "kernel KV-FS init (singleton)");

    rc = wubu_kvfs_mount(g_wubu_kvfs, "/kv/world", 1024, 1024);
    CHECK(rc == 0, "mount /kv/world");

    /* ---- write a world tick into the tensor (the AGI play loop does this) ---- */
    const char *kv_path = "/kv/world/tick_7";
    float vec[4] = { 2.0f, 61.0f, 94.0f, 1.0f };   /* act=2 cpu=61 bat=94 wifi=1 */
    rc = wubu_kvfs_write(g_wubu_kvfs, kv_path, g_wubu_kv_base, vec, 4);
    CHECK(rc == 0, "write tick_7 = [act=2 cpu=61 bat=94 wifi=1] into tensor");

    /* ---- route a namespace read through the SAME store ---- */
    const char *ns_root = "/tmp/wubu-ns";
    const char *disk_path = "/tmp/wubu-ns/kv/world/tick_7";

    const char *routed = wubu_kvfs_route_path(disk_path, ns_root);
    CHECK(routed != NULL && strcmp(routed, "/kv/world/tick_7") == 0,
          "route_path: <ns_root>/kv/world/tick_7 -> /kv/world/tick_7");

    uint8_t out[16];
    uint32_t nread = 0;
    rc = wubu_kvfs_route_read(routed, 0, 16, out, &nread);
    CHECK(rc == 0, "route_read: tensor served");
    CHECK(nread == 16, "route_read: 16 bytes (4 floats)");

    float *got = (float *)out;
    CHECK(got[0] == 2.0f && got[1] == 61.0f && got[2] == 94.0f && got[3] == 1.0f,
          "route_read: tick_7 value matches the tensor (SAME store)");

    /* ---- non-KV path falls through to disk ---- */
    const char *not_kv = wubu_kvfs_route_path("/tmp/wubu-ns/svc/foo/status", ns_root);
    CHECK(not_kv == NULL, "route_path: non-KV path -> NULL (falls through to disk)");

    /* ---- write side routes back into the tensor ---- */
    uint8_t newvec[16];
    float nv[4] = { 3.0f, 88.0f, 12.0f, 0.0f };
    memcpy(newvec, nv, sizeof(nv));
    uint32_t nw = 0;
    rc = wubu_kvfs_route_write("/kv/world/tick_7", 0, 16, newvec, &nw);
    CHECK(rc == 0 && nw == 16, "route_write: Brain writes experience back into tensor");

    /* read back the routed write to confirm the tensor updated */
    rc = wubu_kvfs_route_read("/kv/world/tick_7", 0, 16, out, &nread);
    got = (float *)out;
    CHECK(got[0] == 3.0f && got[2] == 12.0f,
          "route_write persisted: tick_7 now [act=3 cpu=88 bat=12 wifi=0]");

    /* cleanup */
    if (g_wubu_kvfs) wubu_kvfs_free(g_wubu_kvfs);
    if (g_wubu_kv_base) free(g_wubu_kv_base);

    printf("\n=== KV-FS ROUTE TESTS: %s (%d failures) ===\n",
           failures == 0 ? "ALL PASSED" : "FAILED", failures);
    return failures == 0 ? 0 : 1;
}
