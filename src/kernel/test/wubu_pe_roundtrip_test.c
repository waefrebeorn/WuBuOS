/* wubu_pe_roundtrip_test.c -- P2 CLOSING FORCING FUNCTION (cross-repo).
 * The full self-hosting chain: holyd compiles an expression -> hd_write_pe
 * (wubunos emitter) writes a REAL PE32+ EXE to disk -> the in-kernel wubu_pe
 * loader parses/maps/relocates the EMITTED bytes -> wubu_pe_call_entry runs
 * the emitted entry -> the holyd result (42) comes OUT of the loader. This
 * proves WuBuOS can run a from-scratch, on-disk holyd executable on its OWN
 * PE32+ loader (not host Wine), and that holyd output and the loader speak
 * the same 64-bit PE format.
 *
 * Cross-repo link: wubunos front-end (hd_compile) + holyd_pe.c (the emitter)
 * + wubuos wubu_pe.c (loader) + wubu_modsym.c. Kernel loader + holyd must
 * agree on PE32+ layout (magic 0x20B, u64 ImageBase @ opt+32, data dirs at
 * opt+112). See wubu_pe.c parse for the PE32/PE32+ field-location branch. */
#include "../../egojar.h"
#include "../../egojar_net.h"
#include "wubu_pe.h"
#include "holyd.h"
#include "holyd_codegen.h"
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

static int g_pass = 0, g_fail = 0;
#define CHECK(cond, msg) do { if (cond) { g_pass++; } \
    else { g_fail++; printf("  FAIL: %s\n", msg); } } while (0)

static uint8_t *load_file(const char *path, size_t *outsz) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
    uint8_t *buf = malloc((size_t)sz);
    if (buf && fread(buf, 1, (size_t)sz, f) != (size_t)sz) { free(buf); buf = NULL; }
    fclose(f);
    if (outsz) *outsz = (size_t)sz;
    return buf;
}

int main(int argc, char **argv) {
    const char *exe_path = (argc > 1) ? argv[1] : "/tmp/holyd_40p2.exe";
    size_t bsz = 0;

    /* 1. holyd compiles an expression to a runnable blob. */
    void *blob = hd_compile("40+2;", &bsz);
    CHECK(blob && bsz, "hd_compile produced a blob");
    fprintf(stderr, "> holyd blob %zu bytes\n", bsz);

    /* 2. holyd EMITS a real PE file whose .text is the blob payload. */
    size_t po[1] = {0}, pg[1] = {0};
    int rc = hd_write_pe(exe_path, (const uint8_t*)blob, bsz,
                         NULL, 0, po, pg, 0);
    CHECK(rc == 0, "hd_write_pe emitted the PE to disk");
    size_t fsz = 0;
    uint8_t *file = load_file(exe_path, &fsz);
    CHECK(file && fsz > 0, "emitted PE read back from disk");
    fprintf(stderr, "> emitted %s (%zu bytes)\n", exe_path, fsz);

    /* 3. the loader parses the EMITTED PE32+ bytes. */
    wubu_pe_info_t info;
    CHECK(wubu_pe_parse(file, fsz, &info) == 0, "wubu_pe_parse accepts the emitter's PE32+");
    CHECK(info.is_pe32plus == 1, "emitted image is PE32+ (magic 0x20B)");
    wubu_pe_section_t sects[4];
    int ns = wubu_pe_sections(file, fsz, &info, sects, 4);
    CHECK(ns == 1, "one section (.text)");
    fprintf(stderr, "> parsed: machine=%x is_pe32plus=%d entry_rva=%x ns=%d image_base=%x\n",
            info.machine, info.is_pe32plus, info.entry_rva, ns, info.image_base);

    /* 4. map at the emitter's preferred base (image_base field) + load raw. */
    uint8_t *image = mmap((void *)(uintptr_t)info.image_base, 0x2000,
                          PROT_READ | PROT_WRITE | PROT_EXEC,
                          MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
    CHECK(image != MAP_FAILED && image == (void *)(uintptr_t)info.image_base,
          "mapped at the emitter's preferred base");
    memset(image, 0, 0x2000);
    for (int i = 0; i < ns; i++)
        memcpy(image + sects[i].virtual_addr, file + sects[i].raw_ptr, sects[i].raw_size);

    /* 5. relocate (no-op: the emitter emits no .reloc dir; blob is PIC). */
    CHECK(wubu_pe_relocate(&info, sects, ns, file, fsz, image, info.image_base) == 0,
          "relocate returns 0 (no relocation directory)");

    /* 6. run the EMITTED entry -- the holyd machine code inside the file. */
    int res = wubu_pe_call_entry(&info, sects, ns, image);
    fprintf(stderr, "> entry (emitted PE) -> %d\n", res);
    CHECK(res == 42, "holyd-emitted PE executed by the loader returned 42");

    /* 7. PROVE position independence at a SECOND base, like test_holyd_pe. */
    uint8_t *image2 = mmap((void *)0x140000000u + 0x20000u, 0x2000,
                           PROT_READ | PROT_WRITE | PROT_EXEC,
                           MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
    CHECK(image2 != MAP_FAILED, "second image at a shifted base");
    memset(image2, 0, 0x2000);
    for (int i = 0; i < ns; i++)
        memcpy(image2 + sects[i].virtual_addr, file + sects[i].raw_ptr, sects[i].raw_size);
    (void)wubu_pe_relocate(&info, sects, ns, file, fsz, image2, 0x140000000u + 0x20000u);
    int res2 = wubu_pe_call_entry(&info, sects, ns, image2);
    fprintf(stderr, "> entry at shifted base -> %d\n", res2);
    CHECK(res2 == 42, "emitted PE at a shifted base still returns 42");

    free(file);
    printf("\n=== Results: %d passed, %d failed ===\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
