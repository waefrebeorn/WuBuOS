/* Fresh-process gate for bounded FILE-BACKED guest mmap.
 *
 * Must run as its own process: the big single-process VSL test accumulates
 * VSL mmap registry entries, and the legacy bump allocator can hand back an
 * unaligned end-of-previous-mapping address.  A real kernel mmap result is
 * page-aligned, so a fresh process (first run) is the correct place to assert
 * byte-exact host-backed reads.  All requests here use a page-aligned length,
 * which keeps the bump base aligned across the subsequent runs of this probe.
 *
 * The freestanding guest opens a real regular file, mmaps it through the VSL
 * syscall and writes back the 8-byte mmap return plus the bytes it read.
 *   - backing ON  -> bytes are byte-exact against the host file;
 *   - misaligned offset, over-length, MAP_SHARED, non-regular file and a zero
 *     budget fail closed with an exact diagnostic and -ENOMEM;
 *   - backing OFF -> the same guest faults (registry-only differential).
 */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/stat.h>
#include <unistd.h>
#include "vsl_exec_engine.h"

static int passed, failed;
#define CHECK(cond, msg) do { \
    if (cond) { passed++; printf("  ok: %s\n", (msg)); } \
    else { failed++; printf("  FAIL(line %d): %s\n", __LINE__, (msg)); } \
} while (0)

static const char *CTRL = "/tmp/vsl_filemap_ctrl.bin";
static const char *DATA = "/tmp/vsl_filemap_data.bin";

static void fm_ctrl(uint64_t off, uint64_t len, uint64_t prot, uint64_t flags) {
    uint64_t p[4] = { off, len, prot, flags };
    FILE *f = fopen(CTRL, "wb");
    if (f) { (void)fwrite(p, 1, sizeof(p), f); fclose(f); }
}
static void fm_data(const unsigned char *b, size_t n) {
    FILE *f = fopen(DATA, "wb");
    if (f) { (void)fwrite(b, 1, n, f); fclose(f); }
}

#define LEN 4096u
#define PROT_READ_F 1u
#define MAP_PRIVATE_F 2u
#define MAP_SHARED_F 1u

static void report(const char *tag, int rc, const VslExecResult *r,
                   uint64_t addr) {
    printf("[%s] rc=%d status=%d exit=%d out_len=%zu addr=0x%llx "
           "backed=%u bytes=%llu failure=\"%s\"\n",
           tag, rc, r->status, r->exit_code, r->output_len,
           (unsigned long long)addr, r->mmap_backed,
           (unsigned long long)r->mmap_backed_bytes, r->mmap_failure);
}

int main(int argc, char **argv) {
    const char *dir = argc > 1 ? argv[1] : "src/runtime/vsl/exec_guests";
    char path[512];
    snprintf(path, sizeof(path), "%s/guest_filemap.elf", dir);
    FILE *fp = fopen(path, "rb");
    if (!fp) { printf("no fixture %s\n", path); return 2; }
    fseek(fp, 0, SEEK_END); long n = ftell(fp); fseek(fp, 0, SEEK_SET);
    unsigned char *img = (unsigned char *)malloc((size_t)n);
    if (fread(img, 1, (size_t)n, fp) != (size_t)n) { printf("short read\n"); return 2; }
    fclose(fp);

    unsigned char want[LEN];
    for (size_t i = 0; i < LEN; i++) want[i] = (unsigned char)((i * 37 + 11) & 0xff);

    vsl_dyn_set_interp(0);
    vsl_dyn_set_stdout_model(0);
    vsl_dyn_set_mmap_backing(1);
    vsl_dyn_set_mmap_backing_max(VSL_MMAP_BACK_CEILING);
    fm_data(want, sizeof(want));

    VslExecResult r;
    uint64_t addr;
    int rc;

    /* 1. POSITIVE (first run, clean bump base): byte-exact file bytes. */
    fm_ctrl(0, LEN, PROT_READ_F, MAP_PRIVATE_F);
    rc = vsl_exec_elf_bytes(img, (size_t)n, &r);
    addr = r.output_len >= 8 ? *(uint64_t *)r.output : 0;
    report("positive", rc, &r, addr);
    CHECK(rc == VSL_EXEC_OK && r.exit_code == 0 && r.fault_addr == 0,
          "file-backed guest reaches exit(0) with no fault");
    CHECK(addr > 0 && (addr & 0xfff) == 0, "mmap returned a page-aligned guest address");
    CHECK(r.output_len == 8 + LEN + 1 &&
          memcmp(r.output + 8, want, LEN) == 0 &&
          (unsigned char)r.output[8 + LEN] == want[0],
          "guest reads are byte-exact against the host file (bulk + direct)");
    CHECK(r.mmap_backed == 1 && r.mmap_backed_bytes == LEN &&
          r.mmap_failure[0] == 0,
          "file mapping counted once against the shared byte budget");

    /* 2. Fail-closed: unaligned offset names the exact offset and fd. */
    fm_ctrl(1, LEN, PROT_READ_F, MAP_PRIVATE_F);
    rc = vsl_exec_elf_bytes(img, (size_t)n, &r);
    addr = r.output_len >= 8 ? *(uint64_t *)r.output : 0;
    report("unaligned offset", rc, &r, addr);
    CHECK(addr == (uint64_t)-12 && r.output_len == 8 && r.mmap_backed == 0 &&
          strstr(r.mmap_failure, "guest file mmap: unaligned offset") &&
          strstr(r.mmap_failure, "off=0x1") && strstr(r.mmap_failure, "fd=0x"),
          "unaligned file offset fails closed with exact fd/offset diagnostic");

    /* 3. Fail-closed: length beyond the real file size. */
    fm_ctrl(LEN, LEN, PROT_READ_F, MAP_PRIVATE_F);
    rc = vsl_exec_elf_bytes(img, (size_t)n, &r);
    addr = r.output_len >= 8 ? *(uint64_t *)r.output : 0;
    report("over length", rc, &r, addr);
    CHECK(addr == (uint64_t)-12 && r.mmap_backed == 0 &&
          strstr(r.mmap_failure, "guest file mmap: length exceeds file size"),
          "length beyond EOF fails closed with exact diagnostic");

    /* 4. Fail-closed: unsupported flags (MAP_SHARED). */
    fm_ctrl(0, LEN, PROT_READ_F, MAP_SHARED_F);
    rc = vsl_exec_elf_bytes(img, (size_t)n, &r);
    addr = r.output_len >= 8 ? *(uint64_t *)r.output : 0;
    report("shared flags", rc, &r, addr);
    CHECK(addr == (uint64_t)-12 && r.mmap_backed == 0 &&
          strstr(r.mmap_failure, "guest file mmap: unsupported flags"),
          "MAP_SHARED fails closed with exact diagnostic");

    /* 5. Fail-closed: the SHARED byte budget is zero (owner reused). */
    CHECK(vsl_dyn_set_mmap_backing_max(0) == 0, "budget lowered to zero");
    fm_ctrl(0, LEN, PROT_READ_F, MAP_PRIVATE_F);
    rc = vsl_exec_elf_bytes(img, (size_t)n, &r);
    addr = r.output_len >= 8 ? *(uint64_t *)r.output : 0;
    report("zero budget", rc, &r, addr);
    CHECK(addr == (uint64_t)-12 && r.mmap_backed == 0 &&
          strstr(r.mmap_failure, "guest file mmap: byte budget exhausted"),
          "file map obeys the shared bounded byte budget");
    vsl_dyn_set_mmap_backing_max(VSL_MMAP_BACK_CEILING);

    /* 6. Fail-closed: a non-regular file (directory) is refused. */
    unlink(DATA); rmdir(DATA);
    CHECK(mkdir(DATA, 0700) == 0, "made non-regular directory fixture");
    fm_ctrl(0, LEN, PROT_READ_F, MAP_PRIVATE_F);
    rc = vsl_exec_elf_bytes(img, (size_t)n, &r);
    addr = r.output_len >= 8 ? *(uint64_t *)r.output : 0;
    report("directory file", rc, &r, addr);
    CHECK(addr == (uint64_t)-12 && r.mmap_backed == 0 &&
          strstr(r.mmap_failure, "guest file mmap: unsupported file type"),
          "non-regular file fails closed with exact diagnostic");
    rmdir(DATA);
    fm_data(want, sizeof(want));

    /* 7. Negative control: backing OFF -> registry-only, guest faults on
     * its first read from the mmap address (proves backing is real). */
    vsl_dyn_set_mmap_backing(0);
    fm_ctrl(0, LEN, PROT_READ_F, MAP_PRIVATE_F);
    rc = vsl_exec_elf_bytes(img, (size_t)n, &r);
    addr = r.output_len >= 8 ? *(uint64_t *)r.output : 0;
    report("backing OFF", rc, &r, addr);
    CHECK(rc == VSL_EXEC_ERR_FAULT && r.fault_addr != 0 &&
          r.mmap_backed == 0 && r.mmap_failure[0] == 0,
          "backing OFF reproduces the registry-only fault (differential)");

    /* 8. Restore + re-run: exact boundary returns, no cross-run leak. */
    vsl_dyn_set_mmap_backing(1);
    fm_ctrl(0, LEN, PROT_READ_F, MAP_PRIVATE_F);
    rc = vsl_exec_elf_bytes(img, (size_t)n, &r);
    addr = r.output_len >= 8 ? *(uint64_t *)r.output : 0;
    report("restored", rc, &r, addr);
    CHECK(rc == VSL_EXEC_OK && r.output_len == 8 + LEN + 1 &&
          memcmp(r.output + 8, want, LEN) == 0 &&
          (unsigned char)r.output[8 + LEN] == want[0] &&
          r.mmap_backed == 1 && r.mmap_failure[0] == 0 && r.fault_addr == 0,
          "restored backing reproduces byte-exact file reads (no leak)");

    vsl_dyn_set_mmap_backing(0);
    free(img);
    unlink(CTRL); unlink(DATA);
    printf("Results: %d passed, %d failed\n", passed, failed);
    return failed ? 1 : 0;
}
