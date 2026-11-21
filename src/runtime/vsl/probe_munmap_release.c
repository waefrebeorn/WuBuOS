/* Fresh-process gate: host backing pages are released on a mid-run guest
 * munmap, with exact byte-budget decrements and no double-free / alias.
 *
 * Runs guest_munmap_release.elf twice against the SAME fixture:
 *   RELEASE ON  (default): the guest mmaps 4 pages, writes all four, munmaps
 *     the middle two (host pages freed, owner table splits), reads the two
 *     surviving pages, frees both, then MAP_FIXED reuses the ORIGINAL address.
 *     That reuse only succeeds if the host pages were really released.  The
 *     engine must report 0x4000 bytes / 3 host ranges released, a LIVE budget
 *     of 0x1000 (the one reused page), and no fault.
 *   RELEASE OFF (negative control): the registry munmaps still succeed and
 *     the guest still runs, but the host pages are NOT freed (pre-change
 *     behaviour), so the MAP_FIXED reuse is refused with -ENOMEM, zero bytes
 *     are charged back, and the live budget stays at 0x4000.
 */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "vsl_exec_engine.h"

static int passed, failed;
#define CHECK(cond, msg) do { \
    if (cond) { passed++; printf("  ok: %s\n", (msg)); } \
    else { failed++; printf("  FAIL(line %d): %s\n", __LINE__, (msg)); } \
} while (0)

static uint64_t rd64(const unsigned char *b) {
    uint64_t v = 0;
    for (int i = 0; i < 8; i++) v |= (uint64_t)b[i] << (8 * i);
    return v;
}

int main(int argc, char **argv) {
    const char *dir = argc > 1 ? argv[1] : "src/runtime/vsl/exec_guests";
    char path[512];
    snprintf(path, sizeof(path), "%s/guest_munmap_release.elf", dir);
    FILE *fp = fopen(path, "rb");
    if (!fp) { printf("no fixture %s\n", path); return 2; }
    fseek(fp, 0, SEEK_END); long n = ftell(fp); fseek(fp, 0, SEEK_SET);
    unsigned char *img = (unsigned char *)malloc((size_t)n);
    if (fread(img, 1, (size_t)n, fp) != (size_t)n) { printf("short read\n"); return 2; }
    fclose(fp);

    vsl_dyn_set_interp(0);
    vsl_dyn_set_stdout_model(0);
    vsl_dyn_set_mmap_backing(1);
    vsl_dyn_set_mmap_backing_max(VSL_MMAP_BACK_CEILING);
    vsl_dyn_set_munmap_release(1);

    VslExecResult r;
    int rc = vsl_exec_elf_bytes(img, (size_t)n, &r);
    uint64_t p      = r.output_len >= 8  ? rd64(r.output + 0)  : 0;
    uint64_t rc_mid = r.output_len >= 16 ? rd64(r.output + 8)  : 0;
    uint64_t p2     = r.output_len >= 24 ? rd64(r.output + 16) : 0;
    uint64_t v0     = r.output_len >= 32 ? rd64(r.output + 24) : 0;
    uint64_t v3     = r.output_len >= 40 ? rd64(r.output + 32) : 0;
    uint64_t rcl    = r.output_len >= 48 ? rd64(r.output + 40) : 0;
    uint64_t rcr    = r.output_len >= 56 ? rd64(r.output + 48) : 0;
    uint64_t v2     = r.output_len >= 64 ? rd64(r.output + 56) : 0;

    printf("RELEASE-ON rc=%d status=%d exit=%d out_len=%zu p=0x%llx rc_mid=%lld "
           "p2=0x%llx v0=0x%llx v3=0x%llx rcl=%lld rcr=%lld v2=0x%llx "
           "rel_bytes=%llu rel_ranges=%u backed=%u bytes=%llu fault=0x%llx "
           "failure=\"%s\"\n",
           rc, r.status, r.exit_code, r.output_len,
           (unsigned long long)p, (long long)rc_mid,
           (unsigned long long)p2, (unsigned long long)v0,
           (unsigned long long)v3, (long long)rcl, (long long)rcr,
           (unsigned long long)v2,
           (unsigned long long)r.mmap_released_bytes, r.mmap_released_ranges,
           r.mmap_backed, (unsigned long long)r.mmap_backed_bytes,
           (unsigned long long)r.fault_addr, r.mmap_failure);

    CHECK(rc == VSL_EXEC_OK && r.exit_code == 0 && r.fault_addr == 0,
          "release-on: guest runs to exit(0) with no fault");
    CHECK(p > 0 && (p & 0xfff) == 0 &&
          rc_mid == 0 && rcl == 0 && rcr == 0,
          "release-on: 4-page mmap and all three munmaps succeed");
    CHECK(v0 == 0x1111111111111111ULL && v3 == 0x4444444444444444ULL,
          "release-on: outer pages survive the middle-page release with markers intact");
    CHECK(p2 == p,
          "release-on: MAP_FIXED reuses the ORIGINAL address (host pages truly freed)");
    CHECK(v2 == 0x5555555555555555ULL,
          "release-on: reused page is backed and writable");
    CHECK(r.mmap_released_bytes == 0x4000 && r.mmap_released_ranges == 3,
          "release-on: exact 0x4000 bytes freed across 3 host ranges");
    CHECK(r.mmap_backed == 1 && r.mmap_backed_bytes == 0x1000 &&
          r.mmap_failure[0] == 0,
          "release-on: live budget is the single reused page, no leak/alias");

    /* ---- negative control: release switch OFF (pre-change behaviour) ---- */
    vsl_dyn_set_munmap_release(0);
    VslExecResult r2;
    int rc2 = vsl_exec_elf_bytes(img, (size_t)n, &r2);
    uint64_t p2b = r2.output_len >= 24 ? rd64(r2.output + 16) : 0;
    uint64_t v0b = r2.output_len >= 32 ? rd64(r2.output + 24) : 0;
    uint64_t v3b = r2.output_len >= 40 ? rd64(r2.output + 32) : 0;

    printf("RELEASE-OFF rc=%d status=%d exit=%d p2=0x%llx v0=0x%llx v3=0x%llx "
           "rel_bytes=%llu rel_ranges=%u backed=%u bytes=%llu failure=\"%s\"\n",
           rc2, r2.status, r2.exit_code, (unsigned long long)p2b,
           (unsigned long long)v0b, (unsigned long long)v3b,
           (unsigned long long)r2.mmap_released_bytes, r2.mmap_released_ranges,
           r2.mmap_backed, (unsigned long long)r2.mmap_backed_bytes,
           r2.mmap_failure);

    CHECK(rc2 == VSL_EXEC_OK && r2.exit_code == 0 && r2.fault_addr == 0,
          "release-off: guest still runs (registry munmaps succeed, pages stay mapped)");
    CHECK(v0b == 0x1111111111111111ULL && v3b == 0x4444444444444444ULL,
          "release-off: surviving pages readable because host pages were NOT freed");
    CHECK(p2b == (uint64_t)-12,
          "release-off: MAP_FIXED reuse refused with -ENOMEM (host range still occupied)");
    CHECK(r2.mmap_released_bytes == 0 && r2.mmap_released_ranges == 0 &&
          r2.mmap_backed == 1 && r2.mmap_backed_bytes == 0x4000,
          "release-off: no bytes charged back, live budget unchanged (non-vacuous control)");
    CHECK(strstr(r2.mmap_failure, "host range unavailable") != NULL,
          "release-off: refusal names the exact host-range-unavailable cause");

    vsl_dyn_set_munmap_release(1);
    vsl_dyn_set_mmap_backing(0);
    free(img);
    printf("Results: %d passed, %d failed\n", passed, failed);
    return failed ? 1 : 0;
}
