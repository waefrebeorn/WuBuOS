/* Fresh-process gate: ELF-image PT_LOAD and brk ranges are in the allocator
 * registry with exact byte accounting and a leak-free host-backing table.
 *
 * Runs guest_image_brk_registry.elf twice with guest host backing OFF:
 *   1. registry ON  (default): the heap's one registered page makes a hinted
 *      mmap skip the heap; image + brk registry bytes are exact, the image's
 *      host pages are charged to the reserved budget, and both tables are
 *      empty again after the run (no host-page / registry leak);
 *   2. registry OFF (negative control, pre-fix behaviour): no ranges are
 *      registered and the hinted mmap aliases the heap at its old address.
 *
 * All allocator controls remain fail-closed; this probe only reads state.
 */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "vsl_exec_engine.h"
#include "vsl/vsl_memory.h"

static int passed, failed;
#define CHECK(cond, msg) do { \
    if (cond) { passed++; printf("  ok: %s\n", (msg)); } \
    else { failed++; printf("  FAIL(line %d): %s\n", __LINE__, (msg)); } \
} while (0)

/* vsl_init is internal to the exec leg but exposed by the VSL objects. */
extern int vsl_init(void);

static uint64_t rd64(const unsigned char *p) {
    uint64_t v = 0;
    for (int i = 0; i < 8; i++) v |= (uint64_t)p[i] << (8 * i);
    return v;
}
static uint16_t rd16(const unsigned char *p) {
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

/* Page-aligned PT_LOAD union for a static ET_EXEC image (bias 0). */
static uint64_t load_union_bytes(const unsigned char *img, size_t n) {
    if (n < 64) return 0;
    uint16_t e_type = rd16(img + 16);
    uint16_t phnum = rd16(img + 56), phentsize = rd16(img + 54);
    uint64_t phoff = rd64(img + 32);
    if (e_type != 2 || phentsize < 56 ||
        phoff + (uint64_t)phnum * phentsize > n) return 0;
    uint64_t lo = UINT64_MAX, hi = 0;
    for (uint16_t i = 0; i < phnum; i++) {
        const unsigned char *cur = img + phoff + (uint64_t)i * phentsize;
        uint32_t typ = (uint32_t)cur[0] | ((uint32_t)cur[1] << 8) |
                       ((uint32_t)cur[2] << 16) | ((uint32_t)cur[3] << 24);
        if (typ != 1) continue;
        uint64_t vaddr = rd64(cur + 16), memsz = rd64(cur + 40);
        if (!memsz) continue;
        uint64_t ps = vaddr & ~0xfffull;
        uint64_t pe = (vaddr + memsz + 0xfffull) & ~0xfffull;
        if (ps < lo) lo = ps;
        if (pe > hi) hi = pe;
    }
    return (lo != UINT64_MAX && hi > lo) ? hi - lo : 0;
}

static void report(const char *tag, int rc, const VslExecResult *r,
                   uint64_t a, uint64_t b, uint64_t d) {
    printf("[%s] rc=%d status=%d exit=%d old=0x%llx new=0x%llx mapped=0x%llx "
           "img_reg=%llu brk_reg=%llu img_back=%u img_bytes=%llu "
           "reg_n=%u reg_bytes=%llu back_n=%u back_bytes=%llu fault=0x%llx\n",
           tag, rc, r->status, r->exit_code,
           (unsigned long long)a, (unsigned long long)b,
           (unsigned long long)d,
           (unsigned long long)r->image_registry_bytes,
           (unsigned long long)r->brk_registry_bytes,
           r->image_backed, (unsigned long long)r->image_backed_bytes,
           vsl_reserved_count(), (unsigned long long)vsl_reserved_bytes(),
           vsl_dyn_image_back_count(),
           (unsigned long long)vsl_dyn_image_back_bytes(),
           (unsigned long long)r->fault_addr);
}

int main(int argc, char **argv) {
    const char *dir = argc > 1 ? argv[1] : "src/runtime/vsl/exec_guests";
    char path[512];
    snprintf(path, sizeof(path), "%s/guest_image_brk_registry.elf", dir);
    FILE *fp = fopen(path, "rb");
    if (!fp) { printf("no fixture %s\n", path); return 2; }
    fseek(fp, 0, SEEK_END); long n = ftell(fp); fseek(fp, 0, SEEK_SET);
    unsigned char *img = (unsigned char *)malloc((size_t)n);
    if (fread(img, 1, (size_t)n, fp) != (size_t)n) { fclose(fp); return 2; }
    fclose(fp);

    uint64_t want_img = load_union_bytes(img, (size_t)n);
    CHECK(want_img > 0 && (want_img & 0xfff) == 0,
          "fixture is a static ET_EXEC with a page-aligned PT_LOAD union");

    /* Direct fail-closed admission controls on the registry API itself.
     * 0x02000000 is VSL_USER_BASE; the probe keeps it local to stay a
     * public-header consumer. */
    (void)vsl_init();
    const uint64_t guest_base = 0x02000000ULL;
    CHECK(vsl_register_reserved(0, 0x1000, VSL_MMAP_RESERVED_BRK) == -12,
          "out-of-arena registration fails closed with ENOMEM");
    CHECK(vsl_register_reserved(guest_base + 1, 0x1000,
                                VSL_MMAP_RESERVED_BRK) == -22,
          "unaligned registration fails closed with EINVAL");
    CHECK(vsl_reserved_count() == 0,
          "refused registrations leave the registry untouched");
    const uint64_t probe_at = guest_base + 0x200000;
    CHECK(vsl_register_reserved(probe_at, 0x1000, VSL_MMAP_RESERVED_BRK) == 0,
          "one-page reserved range registers");
    CHECK(vsl_reserved_bytes() == 0x1000,
          "reserved byte total is exact after registration");
    CHECK(vsl_mmap(probe_at, 0x1000, 3, 0x32, -1, 0) == (uint64_t)-12,
          "MAP_FIXED that would alias the reserved range fails closed");
    CHECK(vsl_munmap(probe_at, 0x1000) == 0 && vsl_reserved_bytes() == 0x1000,
          "guest munmap cannot trim a reserved range");
    CHECK(vsl_clear_reserved(VSL_MMAP_RESERVED_BRK) == 0 &&
          vsl_reserved_bytes() == 0,
          "clearing removes the range and resets exact bytes");

    vsl_dyn_set_interp(0);
    vsl_dyn_set_stdout_model(0);
    vsl_dyn_set_mmap_backing(0);      /* registry-only for the guest mmap */
    vsl_dyn_set_reserved_registry(1); /* ON by default, stated explicitly */

    VslExecResult r;
    int rc = vsl_exec_elf_bytes(img, (size_t)n, &r);
    uint64_t a = r.output_len >= 24 ? rd64(r.output + 0)  : 0;
    uint64_t b = r.output_len >= 24 ? rd64(r.output + 8)  : 0;
    uint64_t d = r.output_len >= 24 ? rd64(r.output + 16) : 0;
    report("registry ON", rc, &r, a, b, d);

    CHECK(rc == VSL_EXEC_OK && r.exit_code == 0 && r.fault_addr == 0,
          "guest runs to exit(0) with no fault");
    CHECK(a > 0 && (a & 0xfff) == 0 && b == a + 0x1000,
          "brk query and one-page growth are exact");
    CHECK(d == b && d != (uint64_t)-12 && (d & 0xfff) == 0,
          "registered brk heap is skipped by the hinted mmap (no alias)");
    CHECK(r.image_registry_bytes == want_img && r.image_registry_bytes != 0,
          "ELF PT_LOAD union registered with its exact page-rounded bytes");
    CHECK(r.brk_registry_bytes == 0x1000,
          "brk heap registered with exact one-page bytes");
    CHECK(r.image_backed == 1 && r.image_backed_bytes == want_img,
          "image host pages charged exactly to the reserved back-budget table");
    CHECK(r.mmap_backed == 0 && r.mmap_backed_bytes == 0,
          "guest mmap backing stayed off; no host pages charged to it");
    CHECK(vsl_reserved_count() == 0 && vsl_reserved_bytes() == 0 &&
          vsl_dyn_image_back_count() == 0 && vsl_dyn_image_back_bytes() == 0,
          "no registry entry or host page leaks after the run");

    /* Negative control: with the switch OFF the engine reproduces the
     * pre-fix behaviour (no image/brk registration) and the hint aliases
     * the heap at its old address. */
    vsl_dyn_set_reserved_registry(0);
    VslExecResult rc_r;
    int rc_ctl = vsl_exec_elf_bytes(img, (size_t)n, &rc_r);
    uint64_t ca = rc_r.output_len >= 24 ? rd64(rc_r.output + 0)  : 0;
    uint64_t cd = rc_r.output_len >= 24 ? rd64(rc_r.output + 16) : 0;
    report("registry OFF (negative control)", rc_ctl, &rc_r, ca, 0, cd);
    CHECK(rc_ctl == VSL_EXEC_OK && rc_r.exit_code == 0,
          "control guest still runs to exit(0)");
    CHECK(rc_r.image_registry_bytes == 0 && rc_r.brk_registry_bytes == 0,
          "control: image/brk stay outside the registry (pre-fix behaviour)");
    CHECK(cd == ca && ca != 0,
          "control: hinted mmap aliases the unregistered brk heap");
    CHECK(vsl_reserved_count() == 0 && vsl_dyn_image_back_count() == 0,
          "control leaves no registry/host state behind");

    vsl_dyn_set_reserved_registry(1);
    vsl_dyn_set_mmap_backing(0);
    free(img);
    printf("Results: %d passed, %d failed\n", passed, failed);
    return failed ? 1 : 0;
}
