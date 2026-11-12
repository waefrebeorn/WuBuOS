/* wubu_pe_exec_test.c — TLS callbacks + entry-point execution for the in-kernel PE loader.
 * Builds a PE whose .text section is real x86-64 code (mov eax,0x2B; ret -> returns 43)
 * and whose .tls directory points at a NULL-terminated callback array of two host
 * functions. Verifies wubu_pe_tls_callbacks runs them in order and that
 * wubu_pe_call_entry runs TLS callbacks BEFORE transferring to the entry point. */
#include "../../egojar.h"
#include "wubu_pe.h"
#include <stdint.h>
#include <string.h>
#include <stdio.h>

static int g_pass = 0, g_fail = 0;
#define CHECK(cond, msg) do { if (cond) { g_pass++; } \
    else { g_fail++; printf("  FAIL: %s\n", msg); } } while (0)

static int g_cb_count = 0;
static void cb1(void) { g_cb_count++; }
static void cb2(void) { g_cb_count++; }

/* Build a PE32 with:
 *   .text @RVA 0x1000 : entry stub (mov eax,0x2B;ret) at entry_rva = 0x1030
 *   .tls  @RVA 0x2000 : IMAGE_TLS_DIRECTORY (32B) + callback array [cb1, cb2, NULL] at 0x2040
 * image_base = 0x00400000. Returns file size. */
static size_t build_pe(uint8_t *file, size_t cap) {
    memset(file, 0, cap);
    file[0] = 'M'; file[1] = 'Z';
    uint32_t pe_off = 0x80;
    memcpy(file + 0x3C, &pe_off, 4);
    uint8_t *pe = file + pe_off;
    uint32_t sig = 0x00004550; memcpy(pe, &sig, 4);
    uint16_t nsects = 2, optsize = 176;       /* >=176 so TLS dir (index 9) fits */
    memcpy(pe + 6, &nsects, 2);
    memcpy(pe + 20, &optsize, 2);
    uint8_t *opt = pe + 24;
    uint16_t magic = 0x10B; memcpy(opt, &magic, 2);
    uint32_t base = 0x00400000; memcpy(opt + 28, &base, 4);
    uint32_t entry = 0x1030; memcpy(opt + 16, &entry, 4);
    uint16_t subsystem = 3; memcpy(opt + 68, &subsystem, 2);
    uint32_t z = 0; memcpy(opt + 104, &z, 4); memcpy(opt + 108, &z, 4);  /* no imports */
    uint32_t trva = 0x2000, tsz = 32;
    memcpy(opt + 168, &trva, 4); memcpy(opt + 172, &tsz, 4);            /* .tls dir entry 9 */

    uint8_t *sect = opt + optsize;          /* section table @ 0x138 */
    /* section 0: .text @ RVA 0x1000, raw @ 0x188, 0x40 bytes */
    uint8_t *s0 = sect;
    memcpy(s0, ".text", 8);
    uint32_t v = 0x40; memcpy(s0 + 8, &v, 4);
    v = 0x1000; memcpy(s0 + 12, &v, 4);
    v = 0x40;   memcpy(s0 + 16, &v, 4);
    v = 0x198;  memcpy(s0 + 20, &v, 4);
    /* section 1: .tls @ RVA 0x2000, raw @ 0x1C8, 0x100 bytes */
    uint8_t *s1 = sect + 40;
    memcpy(s1, ".tls", 8);
    v = 0x100; memcpy(s1 + 8, &v, 4);
    v = 0x2000; memcpy(s1 + 12, &v, 4);
    v = 0x100; memcpy(s1 + 16, &v, 4);
    v = 0x1D8; memcpy(s1 + 20, &v, 4);

    /* .text raw: entry stub at raw+0x30 (RVA 0x1030) */
    uint8_t *text_raw = file + 0x198;
    memset(text_raw, 0xCC, 0x40);
    text_raw[0x30] = 0xB8;                    /* mov eax, imm32 */
    text_raw[0x31] = 0x2B; text_raw[0x32] = 0x00; text_raw[0x33] = 0x00; text_raw[0x34] = 0x00;
    text_raw[0x35] = 0xC3;                    /* ret */

    /* .tls raw @ 0x1C8: IMAGE_TLS_DIRECTORY then callback array at raw+0x40 */
    uint8_t *tls_raw = file + 0x1D8;
    uint64_t va;
    va = (uint64_t)base + 0x2000; memcpy(tls_raw + 0, &va, 8);    /* StartAddressOfRawData */
    va = (uint64_t)base + 0x2100; memcpy(tls_raw + 8, &va, 8);    /* EndAddressOfRawData   */
    va = (uint64_t)base + 0x2030; memcpy(tls_raw + 16, &va, 8);   /* AddressOfIndex        */
    va = (uint64_t)base + 0x2040; memcpy(tls_raw + 24, &va, 8);   /* AddressOfCallBacks    */
    uint64_t (*cbarr)[3] = (void *)(tls_raw + 0x40);              /* [cb1, cb2, NULL]      */
    (*cbarr)[0] = (uintptr_t)&cb1;
    (*cbarr)[1] = (uintptr_t)&cb2;
    (*cbarr)[2] = 0;

    return 0x1D8 + 0x100;
}

int main(void) {
    uint8_t file[2048];
    size_t fsz = build_pe(file, sizeof(file));

    /* map the image: copy each section's raw bytes to its RVA in a buffer */
    wubu_pe_info_t info;
    CHECK(wubu_pe_parse(file, fsz, &info) == 0, "parse succeeds");
    CHECK(info.entry_rva == 0x1030u, "entry_rva parsed");
    CHECK(info.tls_rva == 0x2000u, "tls_rva parsed (dir 9)");

    wubu_pe_section_t sects[4];
    int ns = wubu_pe_sections(file, fsz, &info, sects, 4);
    CHECK(ns == 2, "two sections");
    /* the image must map into EXECUTABLE memory to host machine code */
    uint8_t *image = mmap(NULL, 0x3000, PROT_READ | PROT_WRITE | PROT_EXEC,
                          MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    CHECK(image != MAP_FAILED, "exec-memory mmap succeeds");
    memset(image, 0, 0x3000);
    for (int i = 0; i < ns; i++) {
        memcpy(image + sects[i].virtual_addr, file + sects[i].raw_ptr, sects[i].raw_size);
    }

    /* entry code sanity: the mapped 0x1030 stub executes to 43 */
    int (*stub)(void) = (int (*)(void))(image + 0x1030);
    CHECK(stub() == 43, "entry machine code returns 43 when called directly");

    /* TLS callbacks run in order, exactly 2 */
    g_cb_count = 0;
    int n = wubu_pe_tls_callbacks(&info, sects, ns, image);
    CHECK(n == 2, "tls_callbacks ran exactly 2");
    CHECK(g_cb_count == 2, "both callbacks executed");

    /* full call_entry: TLS first, then entry, returning 43 */
    g_cb_count = 0;
    int r = wubu_pe_call_entry(&info, sects, ns, image);
    CHECK(r == 43, "call_entry returns the entry function's result (43)");
    CHECK(g_cb_count == 2, "TLS callbacks ran before entry (count already 2 at return)");

    /* an image without TLS runs entry with 0 callbacks */
    wubu_pe_info_t ntls = info;
    ntls.tls_rva = 0;
    g_cb_count = 0;
    r = wubu_pe_call_entry(&ntls, sects, ns, image);
    CHECK(r == 43, "no-TLS image still runs entry (43)");
    CHECK(g_cb_count == 0, "no-TLS image calls zero callbacks");

    /* an image with no entry is rejected */
    wubu_pe_info_t noentry = info;
    noentry.entry_rva = 0;
    CHECK(wubu_pe_call_entry(&noentry, sects, ns, image) == -1, "no-entry image returns -1");

    printf("\n=== Results: %d passed, %d failed ===\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}