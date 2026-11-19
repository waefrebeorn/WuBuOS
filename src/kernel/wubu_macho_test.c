/*
 * wubu_macho_test.c -- behaviour gate for the in-kernel Mach-O loader.
 *
 * The regression that prompted it: the loader's private MH_MAGIC_64 was
 * 0xFEEDFC02 (wrong), so every valid 64-bit Mach-O was rejected while the
 * VSL layer's copy of the same constant was right. One owner now
 * (include/wubu_macho_fmt.h); this gate drives real headers through the
 * parser, including the exact wrong magic as a must-reject case.
 */
#include "wubu_macho.h"
#include "../../include/wubu_macho_fmt.h"

#include <stdio.h>
#include <string.h>
#include <stdint.h>

static int failures = 0;
#define CHECK(cond, msg) do { \
    if (!(cond)) { printf("FAIL: %s\n", msg); failures++; } \
    else { printf("ok: %s\n", msg); } \
} while (0)

static void put32(uint8_t *p, uint32_t v) { memcpy(p, &v, 4); }

static void thin_header(uint8_t *buf, uint32_t magic, uint32_t filetype,
                        uint32_t ncmds, uint32_t sizeofcmds)
{
    memset(buf, 0, 64);
    put32(buf + 0, magic);
    put32(buf + 4, 0x01000007u);   /* CPU_TYPE_X86_64 */
    put32(buf + 8, 3u);            /* CPU_SUBTYPE_X86_64_ALL */
    put32(buf + 12, filetype);
    put32(buf + 16, ncmds);
    put32(buf + 20, sizeofcmds);
}

int main(void)
{
    uint8_t buf[256];
    wubu_macho_info_t info;

    /* A valid 64-bit Mach-O must parse (the census regression). */
    thin_header(buf, MH_MAGIC_64, 2, 5, 240);
    CHECK(wubu_macho_parse(buf, sizeof buf, &info) == 0,
          "valid MH_MAGIC_64 parses");
    CHECK(info.magic == MH_MAGIC_64 && info.filetype == 2 &&
          info.ncommands == 5 && info.sizeofcmds == 240 &&
          info.cpu_type == 0x01000007u,
          "header fields read correctly");

    /* The exact wrong magic the loader used to accept instead: rejected. */
    thin_header(buf, 0xFEEDFC02u, 2, 5, 240);
    CHECK(wubu_macho_parse(buf, sizeof buf, &info) == -1,
          "the old wrong magic 0xFEEDFC02 is rejected");

    /* Byte-swapped 64-bit: fields come back host-order. */
    thin_header(buf, MH_CIGAM_64, 0x02000000u, 0x05000000u, 0xF0000000u);
    CHECK(wubu_macho_parse(buf, sizeof buf, &info) == 0,
          "MH_CIGAM_64 parses");
    CHECK(info.filetype == 2 && info.ncommands == 5 && info.sizeofcmds == 240,
          "swapped fields are byte-swapped back");

    /* Truncated and NULL inputs fail closed. */
    CHECK(wubu_macho_parse(buf, 16, &info) == -1, "short buffer refused");
    CHECK(wubu_macho_parse(NULL, sizeof buf, &info) == -1, "NULL refused");

    /* FAT universal binary: one x86_64 slice. */
    memset(buf, 0, sizeof buf);
    put32(buf + 0, FAT_MAGIC);
    put32(buf + 4, 1u);
    put32(buf + 8, 0x01000007u);
    put32(buf + 12, 4096u);
    put32(buf + 16, 8192u);
    wubu_fat_slice_t slices[4];
    int n = wubu_fat_slices(buf, sizeof buf, slices, 4);
    CHECK(n == 1, "FAT with one slice returns one");
    CHECK(slices[0].cpu_type == 0x01000007u && slices[0].offset == 4096 &&
          slices[0].size == 8192, "slice fields read correctly");
    put32(buf + 0, MH_MAGIC_64);
    CHECK(wubu_fat_slices(buf, sizeof buf, slices, 4) == -1,
          "thin magic is not FAT");

    if (failures) { printf("wubu_macho: %d FAILURES\n", failures); return 1; }
    printf("wubu_macho: ALL PASS\n");
    return 0;
}