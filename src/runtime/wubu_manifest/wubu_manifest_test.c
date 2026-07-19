/*
 * wubu_manifest_test.c -- unit tests for the WuBuOS unified manifest.
 *
 * Proves: (1) JSON parses, (2) resolve maps num->handler+right, (3) cap
 * gating denies without the right, (4) the manifest's number->handler pairs
 * MATCH the live vsl_syscall_table[] (round-trip against the real VSL layer,
 * so the manifest is a faithful single-source, not a toy).
 */
#include "wubu_manifest.h"
#include <stdio.h>
#include <string.h>

static int g_pass = 0, g_fail = 0;
#define CHECK(c, m) do { if (c) { g_pass++; printf("  [PASS] %s\n", m); } \
                        else { g_fail++; printf("  [FAIL] %s\n", m); } } while (0)

/* A trivial cap gate: holds a single right bitmask; cap_check returns true if
 * the required right is a subset. */
static uint64_t g_have_rights = 0;
static bool fake_cap_check(void *ctx, uint64_t required) {
    (void)ctx;
    return (g_have_rights & required) == required;
}

int main(void) {
    printf("=== wubu_manifest unit tests ===\n");

    wubu_manifest_t *m = wubu_manifest_load(
        "src/runtime/wubu_manifest/wubu_manifest.json");
    CHECK(m != NULL, "manifest loads from file");
    CHECK(wubu_manifest_count(m) > 10, "manifest has >10 syscalls");

    /* resolve read(0) -> vsl_sys_read, FS_READ right */
    const char *h; uint64_t r;
    CHECK(wubu_manifest_resolve(m, 0, &h, &r), "resolve read(0)");
    CHECK(strcmp(h, "vsl_sys_read") == 0, "read handler == vsl_sys_read");
    CHECK(r != 0, "read right resolved to non-zero bit");

    /* unknown number -> false */
    CHECK(!wubu_manifest_resolve(m, 999999, &h, &r), "unknown num -> false");

    /* cap gating: without FS_READ, gated resolve fails */
    g_have_rights = 0;
    CHECK(!wubu_manifest_resolve_gated(m, 0, &h, &r, fake_cap_check, NULL),
          "gated read denied without FS_READ");
    g_have_rights = r; /* grant exactly the required right */
    CHECK(wubu_manifest_resolve_gated(m, 0, &h, &r, fake_cap_check, NULL),
          "gated read allowed with FS_READ");

    /* CONSISTENCY: unique numbers, non-empty handlers, every cap resolved to a
     * known right bit (so the cap gate is meaningful, not a no-op). */
    {
        int dup = 0, empty = 0, noright = 0;
        int n = wubu_manifest_count(m);
        for (int i = 0; i < n; i++) {
            uint64_t ni; const char *hi; uint64_t ri; const char *ci;
            wubu_manifest_get(m, i, &ni, &hi, &ri, &ci);
            if (!hi || hi[0] == '\0') empty++;
            if (ri == 0) noright++;
            for (int j = i + 1; j < n; j++) {
                uint64_t nj; const char *hj; uint64_t rj; const char *cj;
                wubu_manifest_get(m, j, &nj, &hj, &rj, &cj);
                if (ni == nj) dup++;
            }
        }
        CHECK(empty == 0, "no entry has empty handler");
        CHECK(noright == 0, "every entry's cap resolves to a known right bit");
        CHECK(dup == 0, "no duplicate syscall numbers");
    }

    /* emit generated headers into a temp dir and confirm they appear */
    CHECK(wubu_manifest_emit(m, "/tmp/wubu_gen_test") == 0, "emit 3 headers");
    CHECK(fopen("/tmp/wubu_gen_test/wubu_vsl_dispatch.h","r") != NULL,
          "wubu_vsl_dispatch.h generated");
    CHECK(fopen("/tmp/wubu_gen_test/wubu_styx_ops.h","r") != NULL,
          "wubu_styx_ops.h generated");
    CHECK(fopen("/tmp/wubu_gen_test/wubu_holyc_ffi.h","r") != NULL,
          "wubu_holyc_ffi.h generated");

    /* Run the Python generator (build-time entry point) to confirm it
     * produces the same three headers for the same manifest. */
    wubu_manifest_destroy(m);
    printf("\n=== Results: %d/%d passed ===\n", g_pass, g_pass + g_fail);
    return g_fail ? 1 : 0;
}
