/*
 * test_fat2.c -- host tests for the FS-B FAT family frontier (100 gaps).
 */
#include <stdio.h>
#include <string.h>
#include "wubu_fat2.h"

static int failures = 0;
#define CHECK(c, m) do { if (!(c)) { printf("  FAIL: %s\n", m); failures++; } } while (0)

int main(void)
{
    printf("=== test_fat2 (FS-B FAT family, complete) ===\n");

    /* B01-B04: FAT16 read + chain */
    {
        uint8_t fat16[4096];
        memset(fat16, 0, sizeof(fat16));
        wubu_fat2_set_next(fat16, sizeof(fat16), 2, 3);
        wubu_fat2_set_next(fat16, sizeof(fat16), 3, 4);
        wubu_fat2_set_next(fat16, sizeof(fat16), 4, 0xFFF8);  /* EOF */
        uint32_t next = 0;
        CHECK(wubu_fat2_read(NULL, fat16, sizeof(fat16), 2, &next) == 0 && next == 3, "fat16 read");
        CHECK(wubu_fat2_chain_len(fat16, sizeof(fat16), 2, 100) == 3, "chain len");
        CHECK(wubu_fat2_type(1000) == 12, "type 12");
        CHECK(wubu_fat2_type(10000) == 16, "type 16");
        CHECK(wubu_fat2_type(100000) == 32, "type 32");
    }

    /* B05-B09: free, frag, atomic, recover */
    {
        uint8_t fat16[4096];
        memset(fat16, 0, sizeof(fat16));
        for (uint32_t c = 2; c < 100; c++) wubu_fat2_set_next(fat16, sizeof(fat16), c, c + 1);
        CHECK(wubu_fat2_free_count(fat16, sizeof(fat16), 100) == 0, "no free");
        memset(fat16, 0, sizeof(fat16));
        CHECK(wubu_fat2_free_count(fat16, sizeof(fat16), 100) == 98, "all free");
        wubu_fat2_set_next(fat16, sizeof(fat16), 2, 4);
        wubu_fat2_set_next(fat16, sizeof(fat16), 4, 6);
        CHECK(wubu_fat2_fragmentation(fat16, sizeof(fat16), 2) == 2, "fragmented");
        CHECK(wubu_fat2_atomic_write(fat16, sizeof(fat16), 7, 0xFFF8) == 0, "atomic");
        uint32_t n = 0;
        wubu_fat2_read(NULL, fat16, sizeof(fat16), 7, &n);
        CHECK(n == 0xFFF8, "atomic readback");
        wubu_fat2_set_next(fat16, sizeof(fat16), 50, 5000);  /* dangling */
        CHECK(wubu_fat2_recover(fat16, sizeof(fat16), 100) == 1, "recover dangling");
    }

    /* B10-B14: time, attr, 8.3, utf16 */
    {
        uint16_t d = 0, t = 0;
        CHECK(wubu_fat2_dos_time(&d, &t, 0) == 0, "dos time");
        CHECK(wubu_fat2_epoch(&d, &t) == 0, "epoch 0");
        CHECK(wubu_fat2_attr(0x22, 0x02) == 1, "hidden attr");
        CHECK(wubu_fat2_attr(0x22, 0x01) == 0, "ro not set");
        CHECK(wubu_fat2_is_83("HELLO.TXT") == 1, "8.3 ok");
        CHECK(wubu_fat2_is_83("hello world.txt") == 0, "8.3 too long");
        uint16_t u16[3] = { 0x48, 0x69, 0x21 };  /* "Hi!" */
        char utf8[16];
        CHECK(wubu_fat2_utf16(u16, 3, utf8, sizeof(utf8)) == 3, "utf16->utf8");
        CHECK(strcmp(utf8, "Hi!") == 0, "utf8 text");
    }

    /* B15-B20: boot, BPB, mirror, dirty, chkdsk, cluster size */
    {
        uint8_t bs[512];
        memset(bs, 0, sizeof(bs));
        bs[0] = 0xEB; bs[510] = 0x55; bs[511] = 0xAA;
        CHECK(wubu_fat2_boot_verify(bs) == 1, "boot verify");
        bs[510] = 0; CHECK(wubu_fat2_boot_verify(bs) == 0, "boot bad sig");
        uint8_t bpb[64];
        memset(bpb, 0, sizeof(bpb));
        bpb[11] = 0x00; bpb[12] = 0x02;   /* 512 bytes */
        bpb[13] = 1;                       /* 1 sector/cluster */
        bpb[16] = 2;                       /* 2 FATs */
        bpb[17] = 0x20; bpb[18] = 0x00;   /* 32 root entries */
        bpb[19] = 0x00; bpb[20] = 0x10;   /* 4096 total */
        uint32_t bps, spc, nf, re;
        CHECK(wubu_fat2_bpb_sectors(bpb, &bps, &spc, &nf, &re) == 4096, "bpb total");
        CHECK(bps == 512 && spc == 1 && nf == 2 && re == 32, "bpb fields");
        uint8_t fa[128], fb[128];
        memset(fa, 0xAB, sizeof(fa)); memcpy(fb, fa, sizeof(fa));
        CHECK(wubu_fat2_mirror(fa, fb, 128) == 1, "mirror match");
        fb[3] ^= 0xFF;
        CHECK(wubu_fat2_mirror(fa, fb, 128) == 0, "mirror mismatch");
        CHECK(wubu_fat2_dirty(bs, 1) == 0 && (bs[0x41] & 1), "dirty set");
        CHECK(wubu_fat2_dirty(bs, 0) == 0 && !(bs[0x41] & 1), "dirty clear");
        uint8_t fat16[4096];
        memset(fat16, 0, sizeof(fat16));
        wubu_fat2_set_next(fat16, sizeof(fat16), 2, 9999);  /* bad */
        uint32_t lost = 0, bad = 0;
        wubu_fat2_chkdsk(fat16, sizeof(fat16), 100, &lost, &bad);
        CHECK(bad == 1, "chkdsk bad");
        CHECK(wubu_fat2_cluster_size(1ULL << 34, 0) == 64, "cluster size");
    }

    /* B21-B40: the engineering close */
    {
        uint8_t bpb[64];
        memset(bpb, 0, sizeof(bpb));
        bpb[11] = 0x00; bpb[12] = 0x02;
        bpb[13] = 1; bpb[14] = 0x01; bpb[15] = 0x00;   /* 1 reserved */
        bpb[16] = 1; bpb[17] = 0x20; bpb[18] = 0x00;   /* 32 root entries */
 bpb[19] = 0x00; bpb[20] = 0x20;
 bpb[22] = 0x08; bpb[23] = 0x00;                /* 8-sector FAT */
 CHECK(wubu_fat2_first_data_sector(bpb) == 1 + 8 + 2, "first data");
        CHECK(wubu_fat2_lfn_checksum("HELLO    TXT") != 0, "lfn checksum");
        uint16_t chars[13] = { 'h','e','l','l','o',' ','w','o','r','l','d' };
        uint8_t entry[32];
        CHECK(wubu_fat2_lfn_entry(chars, 11, 1, entry) == 0, "lfn entry");
        CHECK(entry[11] == 0x0F, "lfn attr");
        uint8_t fat16[4096];
        memset(fat16, 0, sizeof(fat16));
        uint32_t start = 0;
        CHECK(wubu_fat2_alloc_chain(fat16, sizeof(fat16), 100, 3, &start) == 3, "alloc chain");
        CHECK(start == 2, "first free");
        uint32_t n = 0;
        wubu_fat2_read(NULL, fat16, sizeof(fat16), 2, &n);
        CHECK(n == 3, "chain linked");
        CHECK(wubu_fat2_file_size(fat16, sizeof(fat16), 2, 512) == 3 * 512, "file size");
        CHECK(wubu_fat2_truncate(fat16, sizeof(fat16), 2) == 0, "truncate");
        CHECK(wubu_fat2_clear_chain(fat16, sizeof(fat16), 2) == 0, "clear chain");
        uint8_t de[32];
        CHECK(wubu_fat2_dir_entry("HELLO.TXT", 0x20, 5, 1024, de) == 0, "dir entry");
        CHECK(de[0] == 'H' && de[11] == 0x20, "dir entry fields");
        uint8_t dir[256];
        memset(dir, 0, sizeof(dir));
        memcpy(dir, de, 32);
        uint32_t off = 0;
        CHECK(wubu_fat2_dir_find(dir, sizeof(dir), "HELLO", &off) == 0 && off == 0, "dir find");
        char sn[12];
        wubu_fat2_short_name("MyLongName.TXT", sn);
        CHECK(sn[0] == 'M', "short name");
        CHECK(wubu_fat2_validate_cluster(2, 100) == 1, "valid cluster");
        CHECK(wubu_fat2_validate_cluster(150, 100) == 0, "invalid cluster");
        CHECK(wubu_fat2_reserved_cluster(0xFFF5) == 1, "reserved");
        CHECK(wubu_fat2_next_free_hint(fat16, sizeof(fat16), 2) >= 2, "free hint");
        CHECK(wubu_fat2_fat_size_sectors(bpb) == 8, "fat size");
        CHECK(wubu_fat2_root_dir_sectors(bpb) == 2, "root dir sectors");
        CHECK(wubu_fat2_total_data_sectors(bpb) > 0, "data sectors");
        uint64_t lba = 0;
        CHECK(wubu_fat2_cluster_lba(bpb, 2, &lba) == 0 && lba > 0, "cluster lba");
        CHECK(wubu_fat2_media_byte(bpb) == 0, "media byte");
    }

    if (failures == 0) printf("ALL FAT2 TESTS PASSED\n");
    else printf("%d FAT2 FAILURES\n", failures);
    return failures ? 1 : 0;
}
