/* Fresh-process gate: page-aligned guest mmap returns at the allocator source.
 *
 * Runs guest_mmap_align.elf once with host backing ON.  The guest forces the
 * legacy bump allocator past a non-page-sized mapping end; a page-aligned
 * return lets BOTH anonymous mmaps be materialized and the guest reach
 * exit(0).  With the pre-fix allocator the second return is unaligned, backing
 * refuses it as "invalid range", and the guest faults on its store.
 *
 * Also prints a machine-readable "ALIGN" line with the two returns, markers,
 * backing count and diagnostic so the exact output can be quoted by the gate.
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

int main(int argc, char **argv) {
    const char *dir = argc > 1 ? argv[1] : "src/runtime/vsl/exec_guests";
    char path[512];
    snprintf(path, sizeof(path), "%s/guest_mmap_align.elf", dir);
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

    VslExecResult r;
    int rc = vsl_exec_elf_bytes(img, (size_t)n, &r);
    uint64_t a1 = r.output_len >= 8  ? *(uint64_t *)(r.output + 0) : 0;
    uint64_t a2 = r.output_len >= 16 ? *(uint64_t *)(r.output + 8) : 0;
    unsigned char m1 = r.output_len >= 17 ? (unsigned char)r.output[16] : 0;
    unsigned char m2 = r.output_len >= 18 ? (unsigned char)r.output[17] : 0;

    printf("ALIGN rc=%d status=%d exit=%d out_len=%zu a1=0x%llx a2=0x%llx "
           "m1=0x%02x m2=0x%02x backed=%u bytes=%llu fault=0x%llx failure=\"%s\"\n",
           rc, r.status, r.exit_code, r.output_len,
           (unsigned long long)a1, (unsigned long long)a2, m1, m2,
           r.mmap_backed, (unsigned long long)r.mmap_backed_bytes,
           (unsigned long long)r.fault_addr, r.mmap_failure);

    CHECK(rc == VSL_EXEC_OK && r.exit_code == 0 && r.fault_addr == 0,
          "both non-page-sized-bump mmaps run to exit(0) with no fault");
    CHECK(a1 > 0 && (a1 & 0xfff) == 0 && a2 > 0 && (a2 & 0xfff) == 0,
          "both guest mmap returns are page-aligned");
    CHECK(a2 != a1 && m1 == 0xA1 && m2 == 0xB2,
          "each aligned mapping is independently backed and dereferenceable");
    CHECK(r.mmap_backed == 2 && r.mmap_backed_bytes == 0x3000 &&
          r.mmap_failure[0] == 0,
          "both mappings counted once against the shared byte budget");

    vsl_dyn_set_mmap_backing(0);
    free(img);
    printf("Results: %d passed, %d failed\n", passed, failed);
    return failed ? 1 : 0;
}
