/* Fresh-process gate: bounded munmap coalescing in the VSL bump allocator.
 *
 * Runs guest_munmap_coalesce.elf once with host backing OFF (registry only).
 * The guest frees two adjacent pages and then asks for one two-page mmap:
 * only a COALESCED free range can satisfy it below the high-water mark.  The
 * legacy allocator bumps past the freed pages instead.
 *
 * The new allocator controls are declared WEAK so this SAME probe binary
 * also links against the pre-fix vsl_memory.o for the negative control: a
 * missing setter leaves coalescing OFF, reproducing pre-fix behaviour.
 */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "vsl_exec_engine.h"

/* Weak: absent in the pre-fix object, present in the fixed one. */
extern void vsl_dyn_set_free_coalesce(int on) __attribute__((weak));
extern uint32_t vsl_free_range_count(void) __attribute__((weak));
extern uint64_t vsl_free_range_bytes(void) __attribute__((weak));

static int passed, failed;
#define CHECK(cond, msg) do { \
    if (cond) { passed++; printf("  ok: %s\n", (msg)); } \
    else { failed++; printf("  FAIL(line %d): %s\n", __LINE__, (msg)); } \
} while (0)

static uint64_t rd64(const unsigned char *p) {
    uint64_t v = 0;
    for (int i = 0; i < 8; i++) v |= (uint64_t)p[i] << (8 * i);
    return v;
}

int main(int argc, char **argv) {
    const char *dir = argc > 1 ? argv[1] : "src/runtime/vsl/exec_guests";
    char path[512];
    snprintf(path, sizeof(path), "%s/guest_munmap_coalesce.elf", dir);
    FILE *fp = fopen(path, "rb");
    if (!fp) { printf("no fixture %s\n", path); return 2; }
    fseek(fp, 0, SEEK_END); long n = ftell(fp); fseek(fp, 0, SEEK_SET);
    unsigned char *img = (unsigned char *)malloc((size_t)n);
    if (fread(img, 1, (size_t)n, fp) != (size_t)n) { printf("short read\n"); return 2; }
    fclose(fp);

    vsl_dyn_set_interp(0);
    vsl_dyn_set_stdout_model(0);
    vsl_dyn_set_mmap_backing(0);        /* registry-only: no host pages */
    if (vsl_dyn_set_free_coalesce) vsl_dyn_set_free_coalesce(1);

    VslExecResult r;
    int rc = vsl_exec_elf_bytes(img, (size_t)n, &r);
    uint64_t a = r.output_len >= 32 ? rd64(r.output + 0)  : 0;
    uint64_t b = r.output_len >= 32 ? rd64(r.output + 8)  : 0;
    uint64_t c = r.output_len >= 32 ? rd64(r.output + 16) : 0;
    uint64_t d = r.output_len >= 32 ? rd64(r.output + 24) : 0;
    uint32_t frc = vsl_free_range_count ? vsl_free_range_count() : 0;
    uint64_t frb = vsl_free_range_bytes ? vsl_free_range_bytes() : 0;

    printf("COALESCE rc=%d status=%d exit=%d out_len=%zu "
           "a=0x%llx b=0x%llx c=0x%llx d=0x%llx "
           "backed=%u bytes=%llu free_n=%u free_bytes=%llu fault=0x%llx\n",
           rc, r.status, r.exit_code, r.output_len,
           (unsigned long long)a, (unsigned long long)b,
           (unsigned long long)c, (unsigned long long)d,
           r.mmap_backed, (unsigned long long)r.mmap_backed_bytes,
           frc, (unsigned long long)frb, (unsigned long long)r.fault_addr);

    CHECK(rc == VSL_EXEC_OK && r.exit_code == 0 && r.fault_addr == 0,
          "guest runs to exit(0) with no fault");
    CHECK(a > 0 && b == a + 0x1000 && c == b + 0x1000 &&
          (a & 0xfff) == 0,
          "three initial bump mmaps are contiguous and page-aligned");
    CHECK(d == b && d != (uint64_t)-12 && (d & 0xfff) == 0,
          "two adjacent unmaps coalesce into one reusable 2-page range");
    CHECK(r.mmap_backed == 0 && r.mmap_backed_bytes == 0 &&
          frc == 0 && frb == 0,
          "coalesced hole fully consumed: exact byte budget, no host pages");

    if (vsl_dyn_set_free_coalesce) vsl_dyn_set_free_coalesce(0);
    free(img);
    printf("Results: %d passed, %d failed\n", passed, failed);
    return failed ? 1 : 0;
}
