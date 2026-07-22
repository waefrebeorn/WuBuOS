/*
 * vsl_macho_test.c  --  VSL Mach-O Loader Tests
 *
 * Tests for Mach-O binary validation, FAT binary extraction,
 * segment loading, and entry point resolution.
 * C11, self-contained, no external Mach-O binaries needed.
 */
#include "vsl/vsl_macho.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

static int g_tests = 0;
static int g_passed = 0;

#define T(cond, msg) do { \
    g_tests++; \
    if (!(cond)) { \
        fprintf(stderr, "  FAIL [%d] %s\n", g_tests, msg); \
    } else { \
        g_passed++; \
        printf("  \xE2\x9C\x93 %s\n", msg); \
    } \
} while(0)

#define TEST(name) printf("\n[%s]\n", name)

/* Build a minimal valid 64-bit Mach-O executable in memory */
static void *build_minimal_macho(size_t *out_size, uint64_t text_addr,
                                  uint64_t entry_offset, bool is_64) {
    /* For 64-bit: header + one load command (LC_SEGMENT_64 + LC_MAIN) */
    size_t seg_cmd_size = sizeof(macho_segment_command_64_t);
    size_t ep_cmd_size = sizeof(macho_entry_point_command_t);
    size_t total = sizeof(macho_header_64_t) + seg_cmd_size + ep_cmd_size + 16;
    
    uint8_t *buf = calloc(1, total);
    if (!buf) { *out_size = 0; return NULL; }
    
    /* 64-bit header */
    macho_header_64_t *h = (macho_header_64_t*)buf;
    h->magic = MH_MAGIC_64;
    h->cputype = CPU_TYPE_X86_64;
    h->cpusubtype = CPU_SUBTYPE_X86_64_ALL;
    h->filetype = MH_EXECUTE;
    h->ncmds = 2;
    h->sizeofcmds = (uint32_t)(seg_cmd_size + ep_cmd_size);
    h->flags = 0;
    h->reserved = 0;
    
    /* LC_SEGMENT_64 for __TEXT */
    macho_segment_command_64_t *seg = (macho_segment_command_64_t*)(buf + sizeof(macho_header_64_t));
    seg->cmd = LC_SEGMENT_64;
    seg->cmdsize = (uint32_t)seg_cmd_size;
    memcpy(seg->segname, "__TEXT", 6);
    seg->vmaddr = text_addr;
    seg->vmsize = 0x1000;
    seg->fileoff = 0;
    seg->filesize = 0x1000;
    seg->maxprot = 7;  /* rwx */
    seg->initprot = 7; /* rwx */
    seg->nsects = 0;
    seg->flags = 0;
    
    /* LC_MAIN */
    macho_entry_point_command_t *ep = (macho_entry_point_command_t*)(buf + sizeof(macho_header_64_t) + seg_cmd_size);
    ep->cmd = LC_MAIN;
    ep->cmdsize = (uint32_t)ep_cmd_size;
    ep->entry_offset = entry_offset;
    ep->stack_size = 0x800000;  /* 8MB */
    
    *out_size = total;
    return buf;
}

/* Build a FAT binary wrapping a minimal Mach-O */
static void *build_fat_macho(size_t *out_size) {
    size_t inner_size;
    void *inner = build_minimal_macho(&inner_size, 0x100000000ULL, 0x1000, true);
    if (!inner) { *out_size = 0; return NULL; }
    
    size_t fat_hdr_sz = sizeof(fat_header_t) + sizeof(fat_arch_t);
    size_t total = fat_hdr_sz + inner_size;
    uint8_t *buf = calloc(1, total);
    if (!buf) { free(inner); *out_size = 0; return NULL; }
    
    fat_header_t *fh = (fat_header_t*)buf;
    fh->magic = FAT_MAGIC;
    fh->nfat_arch = 1;
    
    fat_arch_t *fa = (fat_arch_t*)(buf + sizeof(fat_header_t));
    fa->cputype = CPU_TYPE_X86_64;
    fa->cpusubtype = CPU_SUBTYPE_X86_64_ALL;
    fa->offset = (uint32_t)fat_hdr_sz;
    fa->size = (uint32_t)inner_size;
    fa->align = 12;  /* 4096 */
    
    memcpy(buf + fat_hdr_sz, inner, inner_size);
    free(inner);
    *out_size = total;
    return buf;
}

static void test_is_macho(void) {
    TEST("Mach-O Detection");
    T(!vsl_macho_is_macho(NULL, 0), "NULL is not Mach-O");
    
    uint32_t magic_macho = MH_MAGIC_64;
    T(vsl_macho_is_macho(&magic_macho, 4), "MH_MAGIC_64 detected");
    
    uint32_t magic_fat = FAT_MAGIC;
    T(vsl_macho_is_macho(&magic_fat, 4), "FAT_MAGIC detected");
    
    uint32_t not_macho = 0xDEADBEAF;
    T(!vsl_macho_is_macho(&not_macho, 4), "random data not detected");
}

static void test_macho_validate(void) {
    TEST("Mach-O Validation");
    
    /* Test with minimal valid Mach-O */
    size_t size;
    void *macho = build_minimal_macho(&size, 0x100000000ULL, 0x1000, true);
    T(macho != NULL, "minimal Mach-O built");
    
    if (macho) {
        uint64_t entry;
        uint32_t filetype;
        int ret = vsl_macho_validate(macho, size, &entry, &filetype);
        T(ret == 0, "validation passes for valid Mach-O");
        T(filetype == MH_EXECUTE, "filetype is MH_EXECUTE");
        T(entry == (0x100000000ULL + 0x1000), "entry = text_base + entry_offset");
        free(macho);
    }
    
    /* Test with FAT binary */
    void *fat = build_fat_macho(&size);
    T(fat != NULL, "FAT binary built");
    if (fat) {
        uint64_t entry;
        uint32_t filetype;
        int ret = vsl_macho_validate(fat, size, &entry, &filetype);
        T(ret == 0, "FAT binary validation extracts x86-64 slice");
        free(fat);
    }
}

static void test_macho_find_command(void) {
    TEST("Mach-O Find Command");
    
    size_t size;
    void *macho = build_minimal_macho(&size, 0x100000000ULL, 0x1000, true);
    T(macho != NULL, "minimal Mach-O built");
    
    if (macho) {
        const macho_entry_point_command_t *ep =
            (const macho_entry_point_command_t*)
                vsl_macho_find_command(macho, size, LC_MAIN);
        T(ep != NULL, "LC_MAIN found");
        T(ep->cmd == LC_MAIN, "command type is LC_MAIN");
        
        const macho_segment_command_64_t *seg =
            (const macho_segment_command_64_t*)
                vsl_macho_find_command(macho, size, LC_SEGMENT_64);
        T(seg != NULL, "LC_SEGMENT_64 found");
        T(strncmp(seg->segname, "__TEXT", 16) == 0, "__TEXT segment found");
        
        /* Non-existent command returns NULL */
        const void *uuid = vsl_macho_find_command(macho, size, LC_UUID);
        T(uuid == NULL, "non-existent LC_UUID returns NULL");
        
        free(macho);
    }
}

static void test_macho_fat_extract(void) {
    TEST("FAT Binary Extraction");
    
    size_t size;
    void *fat = build_fat_macho(&size);
    T(fat != NULL, "FAT binary built");
    
    if (fat) {
        size_t inner_size;
        const void *inner = vsl_macho_fat_extract(fat, size,
                                                   CPU_TYPE_X86_64,
                                                   CPU_SUBTYPE_X86_64_ALL,
                                                   &inner_size);
        T(inner != NULL, "x86-64 slice extracted");
        T(inner_size > 0, "extracted slice has size > 0");
        
        /* Wrong arch returns NULL */
        const void *arm_slice = vsl_macho_fat_extract(fat, size,
                                                       CPU_TYPE_ARM64, 0,
                                                       &inner_size);
        T(arm_slice == NULL, "ARM64 slice returns NULL (not in FAT)");
        
        free(fat);
    }
}

static void test_macho_load(void) {
    TEST("Mach-O Load (segment mapping)");
    
    size_t size;
    void *macho = build_minimal_macho(&size, 0x100000000ULL, 0x1000, true);
    T(macho != NULL, "minimal Mach-O built");
    
    if (macho) {
        uint64_t entry = 0;
        uint64_t stack_size = 0;
        int ret = vsl_macho_load(macho, size, 0x0, &entry, &stack_size);
        T(ret == 0, "Mach-O loaded successfully");
        T(entry != 0, "entry point resolved");
        T(stack_size == 0x800000, "stack size from LC_MAIN preserved");
        free(macho);
    }
}

int main(void) {
    printf("=== VSL Mach-O Loader Tests ===\n");
    
    test_is_macho();
    test_macho_validate();
    test_macho_find_command();
    test_macho_fat_extract();
    test_macho_load();
    
    printf("\n=== Results: %d/%d passed ===\n", g_passed, g_tests);
    return g_passed == g_tests ? 0 : 1;
}
