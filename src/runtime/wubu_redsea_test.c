/*
 * wubu_redsea_test.c -- the RedSea filesystem structures.
 *
 * The load-bearing rule: a BOTH-ENDIAN field is four bytes little-endian
 * FOLLOWED BY four bytes big-endian, and the two halves are REQUIRED to agree
 * -- a disagreement is a corrupt or hostile volume, and picking one half would
 * silently accept it.
 */
#include "wubu_redsea.h"

#include <stdio.h>
#include <string.h>

static int failures;
#define CHECK(c, m) do { if (!(c)) { printf("  FAIL: %s\n", m); failures++; } } while (0)

static void put_le32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24);
}

static void put_be32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v >> 24); p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8); p[3] = (uint8_t)v;
}

/* Write a both-endian field: LE then BE. */
static void put_both(uint8_t *p, uint32_t v)
{
    put_le32(p, v);
    put_be32(p + 4, v);
}

static size_t make_pvd(uint8_t *sec, uint32_t volume_sectors, uint32_t root_extent,
                       uint32_t root_size)
{
    memset(sec, 0, WUBU_REDSEA_DESC_SIZE);
    sec[0] = WUBU_REDSEA_DESC_PRIMARY;
    memcpy(sec + WUBU_REDSEA_MAGIC_OFF, WUBU_REDSEA_MAGIC, 5);
    put_both(sec + 80, volume_sectors);
    /* the logical block size is a both-endian UINT16: 2 LE then 2 BE */
    sec[128] = (uint8_t)(WUBU_REDSEA_SECTOR & 0xFF);
    sec[129] = (uint8_t)(WUBU_REDSEA_SECTOR >> 8);
    sec[130] = sec[129];
    sec[131] = sec[128];
    put_both(sec + 132, 1024);
    /* the root directory record */
    uint8_t *root = sec + 156;
    root[0] = 34;
    put_both(root + 2, root_extent);
    put_both(root + 10, root_size);
    root[25] = 0x02;                       /* a directory */
    root[32] = 1;                          /* name length 1 */
    root[33] = 0x00;                       /* "." */
    return WUBU_REDSEA_DESC_SIZE;
}

int main(void)
{
    printf("=== test_wubu_redsea ===\n");
    uint8_t sec[WUBU_REDSEA_DESC_SIZE];
    wubu_redsea_pvd_t pvd;
    wubu_redsea_dirent_t de;

    /* -- both-endian fields -- */
    {
        uint8_t be[8];
        put_both(be, 0x01020304u);
        uint32_t v = 0;
        CHECK(wubu_redsea_both_endian_u32(be, &v) && v == 0x01020304u,
              "a both-endian field reads as its value");
        CHECK(be[0] == 0x04 && be[3] == 0x01, "the first half is LITTLE-endian");
        CHECK(be[4] == 0x01 && be[7] == 0x04, "the second half is BIG-endian");
        /* A DISAGREEMENT is refused rather than resolved by picking a half. */
        put_le32(be, 1);
        put_be32(be + 4, 2);
        CHECK(!wubu_redsea_both_endian_u32(be, &v),
              "two halves that DISAGREE are refused (a corrupt volume)");
        CHECK(!wubu_redsea_both_endian_u32(NULL, &v), "a null pointer is refused");
        CHECK(!wubu_redsea_both_endian_u32(be, NULL), "a null out is refused");
    }

    /* -- the primary volume descriptor -- */
    {
        size_t n = make_pvd(sec, 1000, 20, 512);
        CHECK(wubu_redsea_check_pvd(sec, &pvd) == NULL, "a normal PVD validates");
        CHECK(pvd.valid, "and is valid");
        CHECK(pvd.volume_space_size == 1000, "the volume size is read");
        CHECK(pvd.block_size == WUBU_REDSEA_SECTOR, "and the block size");
        CHECK(pvd.root_extent == 20 && pvd.root_size == 512,
              "and the root directory's extent and size");
        CHECK(pvd.type == WUBU_REDSEA_DESC_PRIMARY, "with its type");
        (void)n;
    }
    {
        /* a bad magic */
        make_pvd(sec, 1000, 20, 512);
        sec[WUBU_REDSEA_MAGIC_OFF] = 'X';
        CHECK(strcmp(wubu_redsea_check_pvd(sec, &pvd),
                     "a bad magic (not a RedSea/ISO volume)") == 0,
              "a BAD MAGIC is refused");
    }
    {
        /* an unknown descriptor type */
        make_pvd(sec, 1000, 20, 512);
        sec[0] = 42;
        CHECK(wubu_redsea_check_pvd(sec, &pvd) != NULL,
              "an unknown descriptor type is refused");
    }
    {
        /* a corrupt both-endian field inside the PVD */
        make_pvd(sec, 1000, 20, 512);
        put_le32(sec + 80, 1);
        put_be32(sec + 84, 2);
        CHECK(wubu_redsea_check_pvd(sec, &pvd) != NULL,
              "a disagreeing volume size is refused");
    }
    {
        /* a truncated root record */
        make_pvd(sec, 1000, 20, 512);
        sec[156] = 10;
        CHECK(wubu_redsea_check_pvd(sec, &pvd) != NULL,
              "a truncated root record is refused");
        CHECK(wubu_redsea_check_pvd(NULL, &pvd) != NULL, "a null sector is refused");
        CHECK(wubu_redsea_check_pvd(sec, NULL) != NULL, "a null out is refused");
    }

    /* -- directory records -- */
    {
        /* a normal file record */
        uint8_t rec[WUBU_REDSEA_SECTOR];
        memset(rec, 0, sizeof(rec));
        rec[0] = 33 + 5;
        put_both(rec + 2, 30);          /* extent */
        put_both(rec + 10, 128);        /* size */
        rec[25] = 0x00;                 /* a file */
        rec[32] = 5;
        memcpy(rec + 33, "FOO.;1", 6);
        CHECK(wubu_redsea_check_dirent(rec, sizeof(rec), &de) == NULL,
              "a directory record validates");
        CHECK(de.extent == 30 && de.size == 128, "extent and size");
        CHECK(!de.is_dir, "a file is not a directory");
        CHECK(strcmp(de.name, "FOO.") == 0,
              "and the ';' VERSION SUFFIX is stripped ('FOO.;1' -> 'FOO.')");
    }
    {
        /* a directory record */
        uint8_t rec[WUBU_REDSEA_SECTOR];
        memset(rec, 0, sizeof(rec));
        rec[0] = 33 + 4;
        rec[25] = 0x02;
        rec[32] = 4;
        memcpy(rec + 33, "DIR", 3);
        rec[33+3] = 0;
        CHECK(wubu_redsea_check_dirent(rec, sizeof(rec), &de) == NULL,
              "a directory record validates");
        CHECK(de.is_dir, "and is a directory");
    }
    {
        /* "." and ".." are SINGLE BYTES, not ASCII */
        uint8_t rec[WUBU_REDSEA_SECTOR];
        memset(rec, 0, sizeof(rec));
        rec[0] = 34;
        rec[32] = 1;
        rec[33] = 0x00;
        CHECK(wubu_redsea_check_dirent(rec, sizeof(rec), &de) == NULL,
              "the '.' record validates");
        CHECK(de.is_self && strcmp(de.name, ".") == 0,
              "and is the SELF entry, named '.'");
        CHECK(!de.is_parent, "not the parent");

        memset(rec, 0, sizeof(rec));
        rec[0] = 34;
        rec[32] = 1;
        rec[33] = 0x01;
        CHECK(wubu_redsea_check_dirent(rec, sizeof(rec), &de) == NULL,
              "the '..' record validates");
        CHECK(de.is_parent && strcmp(de.name, "..") == 0,
              "and is the PARENT entry, named '..'");
    }
    {
        /* a name containing '/' would escape the directory */
        uint8_t rec[WUBU_REDSEA_SECTOR];
        memset(rec, 0, sizeof(rec));
        rec[0] = 33 + 9;
        rec[32] = 9;
        memcpy(rec + 33, "../evil..", 9);
        CHECK(wubu_redsea_check_dirent(rec, sizeof(rec), &de) != NULL,
              "a name containing '/' is refused (it would escape the directory)");
    }
    {
        /* a record shorter than its fixed header */
        uint8_t rec[WUBU_REDSEA_SECTOR];
        memset(rec, 0, sizeof(rec));
        rec[0] = 10;
        CHECK(wubu_redsea_check_dirent(rec, sizeof(rec), &de) != NULL,
              "a record shorter than its header is refused");
    }
    {
        /* a name that runs past the record */
        uint8_t rec[WUBU_REDSEA_SECTOR];
        memset(rec, 0, sizeof(rec));
        rec[0] = 34;
        rec[32] = 100;
        CHECK(wubu_redsea_check_dirent(rec, sizeof(rec), &de) != NULL,
              "a name running past the record is refused");
    }
    {
        /* a record longer than the caller's buffer */
        uint8_t rec[8];
        memset(rec, 0, sizeof(rec));
        rec[0] = 200;
        CHECK(wubu_redsea_check_dirent(rec, sizeof(rec), &de) != NULL,
              "a record running past the SECTOR is refused");
        CHECK(wubu_redsea_check_dirent(NULL, 0, &de) != NULL, "a null record is refused");
        CHECK(wubu_redsea_check_dirent(rec, sizeof(rec), NULL) != NULL,
              "a null out is refused");
    }

    /* -- extents must fit the volume -- */
    CHECK(wubu_redsea_extent_fits(10, 100, 1000), "an extent inside the volume");
    CHECK(!wubu_redsea_extent_fits(1000, 1, 1000),
          "an extent AT the volume end is OUTSIDE it");
    CHECK(!wubu_redsea_extent_fits(2000, 1, 1000), "an extent past the volume");
    CHECK(!wubu_redsea_extent_fits(999, WUBU_REDSEA_SECTOR + 1, 1000),
          "a size that runs ONE BYTE past the end");
    CHECK(wubu_redsea_extent_fits(999, 1, 1000), "a size that ends exactly at it");
    /* the overflow case: a huge size must not wrap around and appear to fit */
    CHECK(!wubu_redsea_extent_fits(0, 0xFFFFFFFFu, 1),
          "a huge size does not overflow into looking valid");

    if (failures == 0) printf("ALL WUBU_REDSEA TESTS PASSED\n");
    else printf("%d WUBU_REDSEA FAILURES\n", failures);
    return failures ? 1 : 0;
}
