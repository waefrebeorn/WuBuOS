/*
 * wubu_proton_test.c — Test Suite for WuBuOS Proton (Windows Compat Layer)
 *
 * Cell 092: Tests PE validation, API translation, DLL management,
 * PE execution pipeline, and diagnostics.
 */

#include "wubu_proton.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_pass = 0, g_fail = 0, g_total = 0;

#define TEST(name) printf("  TEST %-45s", name); g_total++;
#define PASS() do { printf("✅\n"); g_pass++; } while(0)
#define FAIL(msg) do { printf("❌ %s\n", msg); g_fail++; } while(0)
#define CHECK(cond, msg) do { if (!(cond)) { FAIL(msg); return; } } while(0)

/* ── Test PE binary generators ─────────────────────────────── */

/* Build a minimal PE32 binary in a buffer */
static size_t make_minimal_pe32(uint8_t *buf, size_t bufsz) {
    memset(buf, 0, bufsz);

    /* DOS header: MZ + e_lfanew at 0x3C */
    buf[0] = 'M'; buf[1] = 'Z';
    uint32_t pe_offset = 64;
    memcpy(&buf[0x3C], &pe_offset, 4);

    /* PE signature */
    memcpy(&buf[pe_offset], "\x50\x45\x00\x00", 4);

    /* COFF header */
    pe_coff_header_t coff = {0};
    coff.machine = PE_MACHINE_I386;
    coff.num_sections = 1;
    coff.opt_header_size = 128; /* Real PE32 opt header is 96-224 bytes */
    coff.characteristics = 0x0102; /* EXECUTABLE_IMAGE | 32BIT_MACHINE */
    memcpy(&buf[pe_offset + 4], &coff, sizeof(coff));

    /* Optional header */
    pe_opt_header_std_t opt = {0};
    opt.magic = PE_OPT_MAGIC_PE32;
    opt.entry_point = 0x1000;
    opt.base_of_code = 0x1000;
    uint32_t opt_start = pe_offset + 4 + sizeof(pe_coff_header_t);
    memcpy(&buf[opt_start], &opt, sizeof(opt));
    /* ImageBase at offset 28 from optional header start (PE32) */
    uint32_t image_base = 0x00400000;
    memcpy(&buf[opt_start + 28], &image_base, 4);

    /* Section header: .text */
    uint32_t sec_off = pe_offset + 4 + sizeof(pe_coff_header_t) + coff.opt_header_size;
    pe_section_t sec = {0};
    memcpy(sec.name, ".text\0\0\0", 8);
    sec.virtual_size = 0x1000;
    sec.virtual_addr = 0x1000;
    sec.raw_size = 0x200;
    sec.raw_offset = 0x200;
    sec.characteristics = PE_MEM_EXECUTE | PE_MEM_READ;
    memcpy(&buf[sec_off], &sec, sizeof(sec));

    return sec_off + sizeof(sec) + 0x200;
}

/* Build a minimal PE64 binary */
static size_t make_minimal_pe64(uint8_t *buf, size_t bufsz) {
    memset(buf, 0, bufsz);

    buf[0] = 'M'; buf[1] = 'Z';
    uint32_t pe_offset = 64;
    memcpy(&buf[0x3C], &pe_offset, 4);
    memcpy(&buf[pe_offset], "\x50\x45\x00\x00", 4);

    pe_coff_header_t coff = {0};
    coff.machine = PE_MACHINE_AMD64;
    coff.num_sections = 2;
    coff.opt_header_size = 128; /* Real PE32+ opt header is 112-240 bytes */
    memcpy(&buf[pe_offset + 4], &coff, sizeof(coff));

    pe_opt_header_std_t opt = {0};
    opt.magic = PE_OPT_MAGIC_PE32P;
    opt.entry_point = 0x1000;
    opt.base_of_code = 0x1000;
    uint32_t opt_start = pe_offset + 4 + sizeof(pe_coff_header_t);
    memcpy(&buf[opt_start], &opt, sizeof(opt));
    /* ImageBase at offset 24 from optional header start (PE32+, 8 bytes) */
    uint64_t image_base64 = 0x0000000140000000ULL;
    memcpy(&buf[opt_start + 24], &image_base64, 8);

    /* Two sections: .text and .rdata */
    uint32_t sec_off = pe_offset + 4 + sizeof(pe_coff_header_t) + coff.opt_header_size;
    pe_section_t sec1 = {0};
    memcpy(sec1.name, ".text\0\0\0", 8);
    sec1.virtual_size = 0x1000;
    sec1.virtual_addr = 0x1000;
    sec1.raw_size = 0x200;
    sec1.raw_offset = 0x200;
    sec1.characteristics = PE_MEM_EXECUTE | PE_MEM_READ;
    memcpy(&buf[sec_off], &sec1, sizeof(sec1));

    pe_section_t sec2 = {0};
    memcpy(sec2.name, ".rdata\0\0", 8);
    sec2.virtual_size = 0x500;
    sec2.virtual_addr = 0x2000;
    sec2.raw_size = 0x200;
    sec2.raw_offset = 0x400;
    sec2.characteristics = PE_MEM_READ;
    memcpy(&buf[sec_off + sizeof(pe_section_t)], &sec2, sizeof(sec2));

    return sec_off + 2 * sizeof(pe_section_t) + 0x400;
}

/* ── Lifecycle Tests ───────────────────────────────────────── */

static void test_proton_init(void) {
    TEST("proton init");
    wubu_proton_t p;
    int rc = wubu_proton_init(&p);
    CHECK(rc == 0, "init should succeed");
    CHECK(p.state == PROTON_READY, "state should be READY");
    CHECK(p.vsl_connected == 1, "VSL should be connected");
    CHECK(p.api_count > 0, "should have default APIs loaded");
    CHECK(p.num_dlls > 0, "should have built-in DLLs");

    wubu_proton_shutdown(&p);
    CHECK(p.state == PROTON_OFF, "state should be OFF after shutdown");
    PASS();
}

static void test_proton_is_ready(void) {
    TEST("proton is_ready after init");
    wubu_proton_t p;
    wubu_proton_init(&p);
    CHECK(wubu_proton_is_ready(&p) == 1, "should be ready");
    CHECK(wubu_proton_is_ready(NULL) == 0, "NULL not ready");

    wubu_proton_shutdown(&p);
    CHECK(wubu_proton_is_ready(&p) == 0, "not ready after shutdown");
    PASS();
}

/* ── PE Validation Tests ───────────────────────────────────── */

static void test_pe32_validate(void) {
    TEST("validate PE32 binary");
    wubu_proton_t p;
    wubu_proton_init(&p);

    uint8_t buf[4096];
    size_t size = make_minimal_pe32(buf, sizeof(buf));

    int rc = wubu_proton_validate_pe(&p, buf, size);
    CHECK(rc == 0, "PE32 should validate");
    CHECK(p.is_pe64 == 0, "should be PE32");
    CHECK(p.machine == PE_MACHINE_I386, "machine should be i386");
    CHECK(p.entry_point == 0x1000, "entry point should be 0x1000");

    wubu_proton_shutdown(&p);
    PASS();
}

static void test_pe64_validate(void) {
    TEST("validate PE64 binary");
    wubu_proton_t p;
    wubu_proton_init(&p);

    uint8_t buf[4096];
    size_t size = make_minimal_pe64(buf, sizeof(buf));

    int rc = wubu_proton_validate_pe(&p, buf, size);
    CHECK(rc == 0, "PE64 should validate");
    CHECK(p.is_pe64 == 1, "should be PE64");
    CHECK(p.machine == PE_MACHINE_AMD64, "machine should be AMD64");

    wubu_proton_shutdown(&p);
    PASS();
}

static void test_pe_is_pe_check(void) {
    TEST("is_pe quick check");
    uint8_t buf[4096];
    size_t size = make_minimal_pe32(buf, sizeof(buf));
    CHECK(wubu_proton_is_pe(buf, size) == 1, "should detect PE");

    /* Not PE */
    uint8_t notpe[64] = {0};
    CHECK(wubu_proton_is_pe(notpe, sizeof(notpe)) == 0, "zeroes not PE");

    /* ELF magic */
    notpe[0] = 0x7F; notpe[1] = 'E'; notpe[2] = 'L'; notpe[3] = 'F';
    CHECK(wubu_proton_is_pe(notpe, sizeof(notpe)) == 0, "ELF not PE");

    /* Too small */
    uint8_t tiny[10] = {'M', 'Z'};
    CHECK(wubu_proton_is_pe(tiny, sizeof(tiny)) == 0, "too small for PE");

    PASS();
}

static void test_pe_invalid_machine(void) {
    TEST("reject PE with unsupported machine");
    wubu_proton_t p;
    wubu_proton_init(&p);

    uint8_t buf[4096];
    size_t size = make_minimal_pe32(buf, sizeof(buf));

    /* Corrupt machine type to ARM */
    uint32_t pe_offset = *(uint32_t *)&buf[0x3C];
    buf[pe_offset + 4] = 0xC4; /* ARM machine = 0x01C4 */
    buf[pe_offset + 5] = 0x01;

    int rc = wubu_proton_validate_pe(&p, buf, size);
    CHECK(rc == -1, "ARM PE should fail validation");

    wubu_proton_shutdown(&p);
    PASS();
}

/* ── PE Section Parsing Tests ──────────────────────────────── */

static void test_pe32_parse_sections(void) {
    TEST("parse PE32 sections");
    wubu_proton_t p;
    wubu_proton_init(&p);

    uint8_t buf[4096];
    size_t size = make_minimal_pe32(buf, sizeof(buf));

    wubu_proton_validate_pe(&p, buf, size);
    int nsec = wubu_proton_parse_pe(&p, buf, size);
    CHECK(nsec == 1, "should have 1 section");
    CHECK(p.num_sections == 1, "p.num_sections should be 1");
    CHECK(memcmp(p.sections[0].name, ".text\0\0\0", 8) == 0, "section should be .text");
    CHECK(p.sections[0].virtual_addr == 0x1000, "vaddr should be 0x1000");
    CHECK(p.image_size > 0, "image_size should be > 0");

    wubu_proton_shutdown(&p);
    PASS();
}

static void test_pe64_parse_sections(void) {
    TEST("parse PE64 two sections");
    wubu_proton_t p;
    wubu_proton_init(&p);

    uint8_t buf[4096];
    size_t size = make_minimal_pe64(buf, sizeof(buf));

    wubu_proton_validate_pe(&p, buf, size);
    int nsec = wubu_proton_parse_pe(&p, buf, size);
    CHECK(nsec == 2, "should have 2 sections");
    CHECK(memcmp(p.sections[0].name, ".text\0\0\0", 8) == 0, "first should be .text");
    CHECK(memcmp(p.sections[1].name, ".rdata\0\0", 8) == 0, "second should be .rdata");

    wubu_proton_shutdown(&p);
    PASS();
}

/* ── PE Mapping Tests ──────────────────────────────────────── */

static void test_map_sections(void) {
    TEST("map PE sections returns base address");
    wubu_proton_t p;
    wubu_proton_init(&p);

    uint8_t buf[4096];
    size_t size = make_minimal_pe32(buf, sizeof(buf));
    wubu_proton_validate_pe(&p, buf, size);
    wubu_proton_parse_pe(&p, buf, size);

    uint32_t base = wubu_proton_map_sections(&p, buf, size);
    CHECK(base == 0x00400000, "PE32 base should be 0x00400000");
    CHECK(p.pe_loaded == 1, "pe_loaded should be 1");

    wubu_proton_shutdown(&p);
    PASS();
}

static void test_entry_addr(void) {
    TEST("entry point address = base + RVA");
    wubu_proton_t p;
    wubu_proton_init(&p);

    uint8_t buf[4096];
    size_t size = make_minimal_pe32(buf, sizeof(buf));
    wubu_proton_validate_pe(&p, buf, size);

    uint32_t ea = wubu_proton_entry_addr(&p);
    CHECK(ea == 0x00401000, "entry should be 0x00401000 (0x400000+0x1000)");

    wubu_proton_shutdown(&p);
    PASS();
}

/* ── API Translation Tests ─────────────────────────────────── */

static void test_default_apis_loaded(void) {
    TEST("default APIs loaded");
    wubu_proton_t p;
    wubu_proton_init(&p);
    CHECK(p.api_count > 20, "should have 20+ default APIs");
    CHECK(p.api_table != NULL, "api_table should be allocated");
    wubu_proton_shutdown(&p);
    PASS();
}

static void test_translate_kernel32(void) {
    TEST("translate Kernel32 APIs → VSL syscalls");
    wubu_proton_t p;
    wubu_proton_init(&p);

    int rc = wubu_proton_translate_api(&p, "CreateFileW");
    CHECK(rc == 2, "CreateFileW should map to VSL syscall 2 (open)");

    rc = wubu_proton_translate_api(&p, "ReadFile");
    CHECK(rc == 0, "ReadFile should map to VSL syscall 0 (read)");

    rc = wubu_proton_translate_api(&p, "WriteFile");
    CHECK(rc == 1, "WriteFile should map to VSL syscall 1 (write)");

    rc = wubu_proton_translate_api(&p, "CloseHandle");
    CHECK(rc == 3, "CloseHandle should map to VSL syscall 3 (close)");

    rc = wubu_proton_translate_api(&p, "ExitProcess");
    CHECK(rc == 60, "ExitProcess should map to VSL syscall 60 (exit)");

    CHECK(p.api_translated == 5, "should have translated 5 APIs");

    wubu_proton_shutdown(&p);
    PASS();
}

static void test_translate_vulkan_passthrough(void) {
    TEST("Vulkan APIs flagged as passthrough");
    wubu_proton_t p;
    wubu_proton_init(&p);

    int rc = wubu_proton_translate_api(&p, "vkCreateInstance");
    CHECK(rc == -1, "vkCreateInstance has no direct VSL syscall (-1)");

    /* The passthrough flag is in the api_table entry */
    /* Find the entry and check its flags */
    int found = 0;
    for (int i = 0; i < p.api_count; i++) {
        if (strcmp(p.api_table[i].win32_name, "vkCreateInstance") == 0) {
            CHECK(p.api_table[i].flags & 1, "should have passthrough flag");
            found = 1;
            break;
        }
    }
    CHECK(found == 1, "vkCreateInstance should be in API table");

    wubu_proton_shutdown(&p);
    PASS();
}

static void test_translate_unknown_api(void) {
    TEST("unknown API returns -1");
    wubu_proton_t p;
    wubu_proton_init(&p);

    int rc = wubu_proton_translate_api(&p, "SomeUnknownAPI123");
    CHECK(rc == -1, "unknown API should return -1");

    rc = wubu_proton_translate_api(&p, NULL);
    CHECK(rc == -1, "NULL name should return -1");

    wubu_proton_shutdown(&p);
    PASS();
}

static void test_translate_winsock(void) {
    TEST("Winsock APIs → VSL socket syscalls");
    wubu_proton_t p;
    wubu_proton_init(&p);

    int rc = wubu_proton_translate_api(&p, "socket");
    CHECK(rc == 41, "socket() should map to VSL syscall 41");

    rc = wubu_proton_translate_api(&p, "connect");
    CHECK(rc == 42, "connect() should map to VSL syscall 42");

    rc = wubu_proton_translate_api(&p, "send");
    CHECK(rc == 44, "send() should map to VSL syscall 44");

    rc = wubu_proton_translate_api(&p, "recv");
    CHECK(rc == 45, "recv() should map to VSL syscall 45");

    wubu_proton_shutdown(&p);
    PASS();
}

/* ── DLL Management Tests ──────────────────────────────────── */

static void test_builtin_dlls(void) {
    TEST("built-in DLLs registered");
    wubu_proton_t p;
    wubu_proton_init(&p);
    CHECK(p.num_dlls > 5, "should have 5+ built-in DLLs");

    int idx = wubu_proton_find_dll(&p, "kernel32.dll");
    CHECK(idx >= 0, "kernel32.dll should be found");

    idx = wubu_proton_find_dll(&p, "vulkan-1.dll");
    CHECK(idx >= 0, "vulkan-1.dll should be found");

    idx = wubu_proton_find_dll(&p, "nonexistent.dll");
    CHECK(idx == -1, "nonexistent DLL should return -1");

    wubu_proton_shutdown(&p);
    PASS();
}

static void test_register_custom_dll(void) {
    TEST("register custom DLL");
    wubu_proton_t p;
    wubu_proton_init(&p);

    int before = p.num_dlls;
    int rc = wubu_proton_register_dll(&p, "custom.dll", DLL_NATIVE);
    CHECK(rc == 0, "register should succeed");
    CHECK(p.num_dlls == before + 1, "num_dlls should increase");

    int idx = wubu_proton_find_dll(&p, "custom.dll");
    CHECK(idx >= 0, "custom.dll should be findable");
    CHECK(p.dlls[idx].type == DLL_NATIVE, "type should be NATIVE");

    wubu_proton_shutdown(&p);
    PASS();
}

static void test_dll_case_insensitive(void) {
    TEST("DLL find is case-insensitive");
    wubu_proton_t p;
    wubu_proton_init(&p);

    /* kernel32.dll is registered, search with different case */
    int idx = wubu_proton_find_dll(&p, "KERNEL32.DLL");
    CHECK(idx >= 0, "KERNEL32.DLL should be found (case-insensitive)");

    idx = wubu_proton_find_dll(&p, "Kernel32.Dll");
    CHECK(idx >= 0, "Kernel32.Dll should be found");

    wubu_proton_shutdown(&p);
    PASS();
}

static void test_resolve_deps(void) {
    TEST("DLL dependency resolution");
    wubu_proton_t p;
    wubu_proton_init(&p);

    int rc = wubu_proton_resolve_deps(&p);
    CHECK(rc == 0, "resolve should succeed");

    /* BUILTIN DLLs should be marked as loaded */
    int kidx = wubu_proton_find_dll(&p, "kernel32.dll");
    CHECK(kidx >= 0, "kernel32.dll should exist");
    CHECK(p.dlls[kidx].loaded == 1, "kernel32.dll should be loaded");
    CHECK(p.dlls[kidx].base_addr > 0, "kernel32.dll should have base_addr");

    /* VULKAN (PASSTHROUGH) should also be loaded */
    int vidx = wubu_proton_find_dll(&p, "vulkan-1.dll");
    CHECK(vidx >= 0, "vulkan-1.dll should exist");
    CHECK(p.dlls[vidx].loaded == 1, "vulkan-1.dll should be loaded");

    CHECK(p.dll_resolved > 0, "dll_resolved should be > 0");

    wubu_proton_shutdown(&p);
    PASS();
}

/* ── Execution Pipeline Tests ──────────────────────────────── */

static void test_exec_pe32(void) {
    TEST("exec PE32 through Proton pipeline");
    wubu_proton_t p;
    wubu_proton_init(&p);

    uint8_t buf[4096];
    size_t size = make_minimal_pe32(buf, sizeof(buf));

    int pid = wubu_proton_exec(&p, buf, size, "test.exe");
    CHECK(pid > 0, "should return valid PID");
    CHECK(p.state == PROTON_RUNNING, "state should be RUNNING");
    CHECK(p.pe_loaded == 1, "pe_loaded should be 1");

    wubu_proton_shutdown(&p);
    PASS();
}

static void test_exec_invalid(void) {
    TEST("exec invalid binary returns error");
    wubu_proton_t p;
    wubu_proton_init(&p);

    uint8_t notpe[64] = {0};
    int pid = wubu_proton_exec(&p, notpe, sizeof(notpe), "bad.exe");
    CHECK(pid == -1, "should fail for non-PE");

    wubu_proton_shutdown(&p);
    PASS();
}

/* ── Diagnostics Tests ─────────────────────────────────────── */

static void test_state_name(void) {
    TEST("state name strings");
    wubu_proton_t p = {0};
    p.state = PROTON_OFF;
    CHECK(strcmp(wubu_proton_state_name(&p), "OFF") == 0, "OFF");
    p.state = PROTON_READY;
    CHECK(strcmp(wubu_proton_state_name(&p), "READY") == 0, "READY");
    p.state = PROTON_RUNNING;
    CHECK(strcmp(wubu_proton_state_name(&p), "RUNNING") == 0, "RUNNING");
    CHECK(strcmp(wubu_proton_state_name(NULL), "NULL") == 0, "NULL");
    PASS();
}

static void test_dump(void) {
    TEST("proton_dump does not crash");
    wubu_proton_t p;
    wubu_proton_init(&p);

    uint8_t buf[4096];
    size_t size = make_minimal_pe32(buf, sizeof(buf));
    wubu_proton_validate_pe(&p, buf, size);

    /* Just verify it doesn't crash */
    wubu_proton_dump(&p);
    PASS();
}

static void test_stats(void) {
    TEST("stats counters");
    wubu_proton_t p;
    wubu_proton_init(&p);
    CHECK(wubu_proton_pe_count(&p) == 0, "initial pe_count 0");
    CHECK(wubu_proton_api_count(&p) == 0, "initial api_count 0");
    CHECK(wubu_proton_pe_count(NULL) == 0, "NULL pe_count 0");
    CHECK(wubu_proton_api_count(NULL) == 0, "NULL api_count 0");

    /* After translation */
    wubu_proton_translate_api(&p, "CreateFileW");
    CHECK(wubu_proton_api_count(&p) == 1, "api_count 1 after translate");

    wubu_proton_shutdown(&p);
    PASS();
}

/* ── Main ──────────────────────────────────────────────────── */

int main(void) {
    printf("╔══════════════════════════════════════════════════╗\n");
    printf("║  WuBuOS Proton (Windows Compat) Test Suite        ║\n");
    printf("╚══════════════════════════════════════════════════╝\n\n");

    /* Lifecycle */
    test_proton_init();
    test_proton_is_ready();

    /* PE Validation */
    test_pe32_validate();
    test_pe64_validate();
    test_pe_is_pe_check();
    test_pe_invalid_machine();

    /* PE Parsing */
    test_pe32_parse_sections();
    test_pe64_parse_sections();

    /* PE Mapping */
    test_map_sections();
    test_entry_addr();

    /* API Translation */
    test_default_apis_loaded();
    test_translate_kernel32();
    test_translate_vulkan_passthrough();
    test_translate_unknown_api();
    test_translate_winsock();

    /* DLL Management */
    test_builtin_dlls();
    test_register_custom_dll();
    test_dll_case_insensitive();
    test_resolve_deps();

    /* Execution */
    test_exec_pe32();
    test_exec_invalid();

    /* Diagnostics */
    test_state_name();
    test_dump();
    test_stats();

    printf("\n══════════════════════════════════════════════════\n");
    printf("  Results: %d/%d passed, %d failed\n", g_pass, g_total, g_fail);
    printf("══════════════════════════════════════════════════\n");

    return g_fail > 0 ? 1 : 0;
}
