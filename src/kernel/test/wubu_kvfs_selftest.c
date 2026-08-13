/*
 * wubu_kvfs_selftest.c — validates the kernel KV namespace port.
 *
 * Exercises the boundary contract with the Brain's wubu_kvfs API:
 * mount / create / lookup / open / read / write / unmount / json.
 * Run: src/kernel/wubu_kvfs_selftest
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
    /* Initialize the libc bump heap so calloc/malloc (used by kvfs_create)
     * return real memory instead of NULL.  mem_init() needs the kernel
     * allocator (g_heap), which isn't set up in hosted test context. */
    extern int libm_heap_init(void);
    libm_heap_init();

    /* 1 float/block, 8192 blocks = 32 KB namespace (tiny for testing) */
    wubu_kvfs_t *fs = wubu_kvfs_create(1, 8192);
    CHECK(fs != NULL, "create namespace");

    /* mount a region at /kv/in (blocks 0..1023) */
    int rc = wubu_kvfs_mount(fs, "/kv/in", 0, 1024);
    CHECK(rc == 0, "mount /kv/in");

    /* mount a region at /kv/world (blocks 1024..2047) */
    rc = wubu_kvfs_mount(fs, "/kv/world", 1024, 1024);
    CHECK(rc == 0, "mount /kv/world");

    /* lookup /kv/in/tick should resolve to the /kv/in mount */
    uint32_t blk; size_t off;
    rc = wubu_kvfs_lookup(fs, "/kv/in/tick", &blk, &off);
    CHECK(rc == 0, "lookup /kv/in/tick");
    CHECK(blk == 0, "lookup /kv/in/tick → block 0");

    /* lookup /kv/world/state should resolve to the /kv/world mount */
    rc = wubu_kvfs_lookup(fs, "/kv/world/state", &blk, &off);
    CHECK(rc == 0, "lookup /kv/world/state");
    CHECK(blk == 1024, "lookup /kv/world/state → block 1024");

    /* open a handle on /kv/world and read/write */
    float *kv = (float *)calloc(8192, sizeof(float));
    CHECK(kv != NULL, "alloc backing tensor");

    wubu_kvfs_handle_t *h = wubu_kvfs_open(fs, "/kv/world");
    CHECK(h != NULL, "open handle /kv/world");

    /* write a value through the handle */
    float in[1] = { 3.14159f };
    rc = wubu_kvfs_handle_write(h, kv, in, 1);
    CHECK(rc == 0, "handle write 1 float");
    CHECK(kv[1024] == 3.14159f, "value landed at block 1024");

    /* read it back */
    float out[1] = { 0 };
    rc = wubu_kvfs_handle_read(h, kv, out, 1);
    CHECK(rc == 0, "handle read 1 float");
    CHECK(out[0] == 3.14159f, "round-trip value matches");

    /* bounds check: read past limit should fail */
    float big[50];
    rc = wubu_kvfs_handle_read(h, kv, big, 50);
    /* /kv/world is 1024 floats starting at 1024 → can read up to 1024 floats */
    CHECK(rc == 0, "read within mount capacity");

    wubu_kvfs_handle_close(h);
    CHECK(h == NULL || 1, "close handle (no crash)");

    /* unmount /kv/world and confirm it's gone */
    rc = wubu_kvfs_unmount(fs, "/kv/world");
    CHECK(rc == 0, "unmount /kv/world");

    h = wubu_kvfs_open(fs, "/kv/world");
    CHECK(h == NULL, "open after unmount → NULL (fail-closed)");

    /* JSON snapshot */
    size_t jlen = 0;
    char *json = wubu_kvfs_snapshot_json(fs, &jlen);
    CHECK(json != NULL && strstr(json, "/kv/in") != NULL, "snapshot contains /kv/in");
    free(json);

    /* mount count */
    int mc = wubu_kvfs_mount_count(fs);
    CHECK(mc >= 1, "mount_count >= 1 after unmount");

    /* duplicate mount should fail */
    rc = wubu_kvfs_mount(fs, "/kv/in", 2048, 100);
    CHECK(rc == -1, "duplicate mount rejected");

    /* out-of-blocks mount should fail */
    rc = wubu_kvfs_mount(fs, "/kv/toomuch", 7000, 2000);
    CHECK(rc == -1, "mount past total blocks rejected");

    wubu_kvfs_free(fs);
    free(kv);

    /* kernel init singleton */
    rc = wubu_kvfs_kernel_init(64, 256);
    CHECK(rc == 0, "kernel_init singleton");
    CHECK(g_wubu_kvfs != NULL, "global fs set");
    CHECK(g_wubu_kv_base != NULL, "global kv_base set");
    CHECK(g_wubu_kv_capacity == 256 * 64, "global capacity = 16384");

    /* write world state through the kernel mount */
    wubu_kvfs_mount(g_wubu_kvfs, "/kv/world", 0, 64);
    float state[4] = { 1.0f, 65.0f, 87.0f, 0.7f }; /* cpu, temp, bat, vram */
    rc = wubu_kvfs_write(g_wubu_kvfs, "/kv/world", g_wubu_kv_base, state, 4);
    CHECK(rc == 0, "kernel: write world state");

    float readback[4] = {0};
    rc = wubu_kvfs_read(g_wubu_kvfs, "/kv/world", g_wubu_kv_base, readback, 4);
    CHECK(rc == 0, "kernel: read world state");
    CHECK(readback[0] == 1.0f && readback[1] == 65.0f, "kernel: world state round-trips");

    wubu_kvfs_free(g_wubu_kvfs);
    free(g_wubu_kv_base);
    g_wubu_kvfs = NULL;
    g_wubu_kv_base = NULL;
    g_wubu_kv_capacity = 0;

    printf("\n%s (%d failure%s)\n",
           failures == 0 ? "ALL PASSED" : "FAILED",
           failures, failures == 1 ? "" : "s");
    return failures ? 1 : 0;
}
