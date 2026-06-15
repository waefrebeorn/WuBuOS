/*
 * iso9660_test.c  --  Test Suite for WuBuOS ISO 9660 / Bootable ISO Builder
 *
 * Cell 060: Tests ISO builder lifecycle, volume descriptors,
 * El Torito boot catalog, path tables, root directory, and validation.
 */

#include "iso9660.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_pass = 0, g_fail = 0, g_total = 0;
#define TEST(name) printf("  TEST %-45s", name); g_total++;
#define PASS() do { printf("✅\n"); g_pass++; } while(0)
#define FAIL(msg) do { printf("❌ %s\n", msg); g_fail++; } while(0)
#define CHECK(cond, msg) do { if (!(cond)) { FAIL(msg); return; } } while(0)

/* -- Lifecycle Tests ----------------------------------------- */

static void test_builder_init(void) {
    TEST("builder init");
    iso_builder_t b;
    int rc = iso_builder_init(&b, "WUBUOS_TEST");
    CHECK(rc == 0, "init should succeed");
    CHECK(strcmp(b.volume_id, "WUBUOS_TEST") == 0, "volume_id should be set");
    CHECK(b.num_files == 0, "no files initially");
    CHECK(b.has_boot == 0, "no boot initially");
    iso_builder_shutdown(&b);
    PASS();
}

static void test_builder_init_null(void) {
    TEST("builder init with NULL volume_id uses default");
    iso_builder_t b;
    int rc = iso_builder_init(&b, NULL);
    CHECK(rc == 0, "init should succeed");
    CHECK(strcmp(b.volume_id, "WUBUOS") == 0, "default volume_id should be WUBUOS");
    iso_builder_shutdown(&b);
    PASS();
}

/* -- File Management Tests ----------------------------------- */

static void test_add_files(void) {
    TEST("add files");
    iso_builder_t b;
    iso_builder_init(&b, "TEST");

    int rc = iso_builder_add_file(&b, "KERNEL.BIN", NULL, 4096);
    CHECK(rc == 0, "add file should succeed");
    CHECK(b.num_files == 1, "should have 1 file");
    CHECK(strcmp(b.files[0].name, "KERNEL.BIN") == 0, "name should match");
    CHECK(b.files[0].size == 4096, "size should match");

    rc = iso_builder_add_file(&b, "README.TXT", NULL, 100);
    CHECK(rc == 0, "add second file");
    CHECK(b.num_files == 2, "should have 2 files");

    iso_builder_shutdown(&b);
    PASS();
}

static void test_add_dir(void) {
    TEST("add directory");
    iso_builder_t b;
    iso_builder_init(&b, "TEST");

    int rc = iso_builder_add_dir(&b, "DOCS");
    CHECK(rc == 0, "add dir should succeed");
    CHECK(b.files[0].flags & ISO_FLAG_DIR, "should have DIR flag");

    iso_builder_shutdown(&b);
    PASS();
}

/* -- Boot Configuration Tests -------------------------------- */

static void test_set_boot(void) {
    TEST("set boot image");
    iso_builder_t b;
    iso_builder_init(&b, "TEST");

    uint8_t boot_image[ISO_SECTOR_SIZE * 4];
    memset(boot_image, 0x90, sizeof(boot_image)); /* NOP sled */

    int rc = iso_builder_set_boot(&b, boot_image, sizeof(boot_image));
    CHECK(rc == 0, "set boot should succeed");
    CHECK(b.has_boot == 1, "should be bootable");
    CHECK(b.boot_image_size == ISO_SECTOR_SIZE * 4, "boot size should match");

    iso_builder_shutdown(&b);
    PASS();
}

/* -- ISO Build Tests ----------------------------------------- */

static void test_build_basic(void) {
    TEST("build basic ISO (no boot)");
    iso_builder_t b;
    iso_builder_init(&b, "WUBUOS");

    iso_builder_add_file(&b, "HELLO.TXT", NULL, 100);

    uint32_t size = iso_builder_build(&b);
    CHECK(size > 0, "build should return non-zero size");
    CHECK(b.image != NULL, "image should be allocated");
    CHECK(b.total_sectors > 18, "should have more than 18 sectors");
    CHECK(b.image_size == b.total_sectors * ISO_SECTOR_SIZE, "image size matches");

    iso_builder_shutdown(&b);
    PASS();
}

static void test_build_bootable(void) {
    TEST("build bootable ISO");
    iso_builder_t b;
    iso_builder_init(&b, "WUBUOS");

    uint8_t boot_image[ISO_SECTOR_SIZE * 2];
    memset(boot_image, 0x90, sizeof(boot_image));
    iso_builder_set_boot(&b, boot_image, sizeof(boot_image));
    iso_builder_add_file(&b, "KERNEL.BIN", NULL, 8192);

    uint32_t size = iso_builder_build(&b);
    CHECK(size > 0, "build should succeed");
    CHECK(b.has_boot == 1, "should still be bootable after build");

    iso_builder_shutdown(&b);
    PASS();
}

/* -- Volume Descriptor Validation Tests ---------------------- */

static void test_validate_primary_vd(void) {
    TEST("validate primary volume descriptor");
    iso_builder_t b;
    iso_builder_init(&b, "WUBUOS");
    iso_builder_build(&b);

    int valid = iso_builder_validate(&b);
    CHECK(valid == 1, "should be valid ISO");

    /* Check PVD directly */
    const iso_primary_vd_t *pvd = (const iso_primary_vd_t *)&b.image[16 * ISO_SECTOR_SIZE];
    CHECK(pvd->type == ISO_VD_PRIMARY, "type should be PRIMARY");
    CHECK(memcmp(pvd->id, "CD001", 5) == 0, "id should be CD001");
    CHECK(pvd->version == 1, "version should be 1");
    CHECK(pvd->logical_block_size.le == ISO_SECTOR_SIZE, "block size should be 2048");

    iso_builder_shutdown(&b);
    PASS();
}

static void test_validate_terminator(void) {
    TEST("volume descriptor set terminator");
    iso_builder_t b;
    iso_builder_init(&b, "WUBUOS");
    iso_builder_build(&b);

    const iso_terminator_vd_t *tvd = (const iso_terminator_vd_t *)&b.image[b.terminator_vd_lba * ISO_SECTOR_SIZE];
    CHECK(tvd->type == ISO_VD_TERMINATOR, "type should be TERMINATOR");
    CHECK(memcmp(tvd->id, "CD001", 5) == 0, "id should be CD001");

    iso_builder_shutdown(&b);
    PASS();
}

/* -- El Torito Tests ----------------------------------------- */

static void test_el_torito_bootable(void) {
    TEST("El Torito bootable check");
    iso_builder_t b;
    iso_builder_init(&b, "WUBUOS");

    uint8_t boot_image[ISO_SECTOR_SIZE];
    memset(boot_image, 0x90, ISO_SECTOR_SIZE);
    iso_builder_set_boot(&b, boot_image, ISO_SECTOR_SIZE);
    iso_builder_build(&b);

    int bootable = iso_builder_is_bootable(&b);
    CHECK(bootable == 1, "should be bootable");

    /* Check boot VD */
    const iso_boot_vd_t *bvd = (const iso_boot_vd_t *)&b.image[17 * ISO_SECTOR_SIZE];
    CHECK(bvd->type == ISO_VD_BOOT, "type should be BOOT");
    CHECK(bvd->boot_catalog_lba > 0, "boot catalog LBA should be set");

    iso_builder_shutdown(&b);
    PASS();
}

static void test_el_torito_catalog(void) {
    TEST("El Torito catalog validation entry");
    iso_builder_t b;
    iso_builder_init(&b, "WUBUOS");

    uint8_t boot_image[ISO_SECTOR_SIZE * 2];
    memset(boot_image, 0x90, sizeof(boot_image));
    iso_builder_set_boot(&b, boot_image, sizeof(boot_image));
    iso_builder_build(&b);

    const el_torito_catalog_t *cat = (const el_torito_catalog_t *)&b.image[b.boot_catalog_lba * ISO_SECTOR_SIZE];
    CHECK(cat->validation.header_id == 0x01, "header_id should be 0x01");
    CHECK(cat->validation.signature == EL_TORITO_SIG, "signature should be 0xAA55");
    CHECK(cat->entry.boot_indicator == 0x88, "entry should be bootable (0x88)");
    CHECK(cat->entry.load_rba > 0, "load RBA should be set");

    iso_builder_shutdown(&b);
    PASS();
}

static void test_el_torito_checksum(void) {
    TEST("El Torito validation checksum is correct");
    iso_builder_t b;
    iso_builder_init(&b, "WUBUOS");

    uint8_t boot_image[ISO_SECTOR_SIZE];
    memset(boot_image, 0, ISO_SECTOR_SIZE);
    iso_builder_set_boot(&b, boot_image, ISO_SECTOR_SIZE);
    iso_builder_build(&b);

    const el_torito_catalog_t *cat = (const el_torito_catalog_t *)&b.image[b.boot_catalog_lba * ISO_SECTOR_SIZE];
    /* Verify checksum: sum of all 16-bit words should be 0 */
    const uint16_t *words = (const uint16_t *)&cat->validation;
    uint16_t sum = 0;
    for (int i = 0; i < 16; i++) sum += words[i];
    CHECK(sum == 0, "checksum validation: sum of all words should be 0");

    iso_builder_shutdown(&b);
    PASS();
}

static void test_non_bootable_is_bootable(void) {
    TEST("non-bootable ISO is_bootable returns 0");
    iso_builder_t b;
    iso_builder_init(&b, "WUBUOS");
    iso_builder_add_file(&b, "README.TXT", NULL, 100);
    iso_builder_build(&b);

    CHECK(iso_builder_is_bootable(&b) == 0, "should not be bootable");

    iso_builder_shutdown(&b);
    PASS();
}

/* -- Path Table Tests ---------------------------------------- */

static void test_path_tables(void) {
    TEST("path tables written");
    iso_builder_t b;
    iso_builder_init(&b, "WUBUOS");
    iso_builder_build(&b);

    /* L-path table should exist at the assigned LBA */
    const uint8_t *lpt = &b.image[b.l_path_table_lba * ISO_SECTOR_SIZE];
    CHECK(lpt[0] == 1, "LPT: directory ID length should be 1");
    CHECK(lpt[8] == 1, "LPT: directory number should be 1");

    /* M-path table */
    const uint8_t *mpt = &b.image[b.m_path_table_lba * ISO_SECTOR_SIZE];
    CHECK(mpt[0] == 1, "MPT: directory ID length should be 1");

    iso_builder_shutdown(&b);
    PASS();
}

/* -- Root Directory Tests ------------------------------------ */

static void test_root_directory(void) {
    TEST("root directory has . and .. entries");
    iso_builder_t b;
    iso_builder_init(&b, "WUBUOS");
    iso_builder_build(&b);

    const uint8_t *root = &b.image[b.root_dir_lba * ISO_SECTOR_SIZE];
    /* First record: "." */
    CHECK(root[0] >= 33, "first record length >= 33");
    CHECK(root[25] & ISO_FLAG_DIR, "first record should be DIR (. )");
    CHECK(root[32] == 1, "first record id_len = 1 (.)");

    /* Second record at offset 34: ".." */
    const uint8_t *dotdot = root + 34;
    CHECK(dotdot[0] >= 33, "second record length >= 33");
    CHECK(dotdot[25] & ISO_FLAG_DIR, "second record should be DIR (..)");
    CHECK(dotdot[32] == 1, "second record id_len = 1 (..)");

    iso_builder_shutdown(&b);
    PASS();
}

static void test_root_directory_with_files(void) {
    TEST("root directory contains file entries");
    iso_builder_t b;
    iso_builder_init(&b, "WUBUOS");
    iso_builder_add_file(&b, "HELLO.TXT", NULL, 100);
    iso_builder_add_file(&b, "KERNEL.BIN", NULL, 4096);
    iso_builder_build(&b);

    /* After . and .. (68 bytes), files should follow */
    const uint8_t *root = &b.image[b.root_dir_lba * ISO_SECTOR_SIZE];
    const uint8_t *first_file = root + 68;
    CHECK(first_file[0] > 0, "third record should exist");
    /* Check the file name in the record */
    int id_len = first_file[32];
    CHECK(id_len > 0, "file should have non-zero id_len");

    iso_builder_shutdown(&b);
    PASS();
}

/* -- Layout Tests -------------------------------------------- */

static void test_sector_layout(void) {
    TEST("sector layout is correct");
    iso_builder_t b;
    iso_builder_init(&b, "WUBUOS");
    iso_builder_build(&b);

    /* System area: sectors 0-15 should be all zeros */
    int sys_zero = 1;
    for (int i = 0; i < 16 * ISO_SECTOR_SIZE; i++) {
        if (b.image[i] != 0) { sys_zero = 0; break; }
    }
    CHECK(sys_zero, "system area should be all zeros");

    /* Primary VD at sector 16 */
    CHECK(b.primary_vd_lba == 16, "primary VD at sector 16");

    /* Data should start after VDs + path tables + root dir */
    CHECK(b.next_data_lba >= 18, "data starts at or after sector 18");

    iso_builder_shutdown(&b);
    PASS();
}

static void test_bootable_sector_layout(void) {
    TEST("bootable ISO sector layout");
    iso_builder_t b;
    iso_builder_init(&b, "WUBUOS");

    uint8_t boot[ISO_SECTOR_SIZE];
    memset(boot, 0x90, ISO_SECTOR_SIZE);
    iso_builder_set_boot(&b, boot, ISO_SECTOR_SIZE);
    iso_builder_build(&b);

    CHECK(b.boot_vd_lba == 17, "boot VD at sector 17");
    CHECK(b.terminator_vd_lba == 18, "terminator at sector 18");
    CHECK(b.boot_catalog_lba >= 19, "catalog after VDs");
    CHECK(b.boot_image_lba > b.boot_catalog_lba, "image after catalog");

    iso_builder_shutdown(&b);
    PASS();
}

/* -- Image Access Tests -------------------------------------- */

static void test_image_access(void) {
    TEST("image access functions");
    iso_builder_t b;
    iso_builder_init(&b, "TEST");
    iso_builder_build(&b);

    const uint8_t *img = iso_builder_image(&b);
    CHECK(img != NULL, "image should not be NULL");
    CHECK(img == b.image, "image pointer should match");

    uint32_t size = iso_builder_image_size(&b);
    CHECK(size == b.image_size, "image_size should match");
    CHECK(size > 0, "image_size should be > 0");

    /* NULL checks */
    CHECK(iso_builder_image(NULL) == NULL, "NULL image = NULL");
    CHECK(iso_builder_image_size(NULL) == 0, "NULL size = 0");

    iso_builder_shutdown(&b);
    PASS();
}

static void test_validate_empty(void) {
    TEST("validate empty/NULL builder");
    CHECK(iso_builder_validate(NULL) == 0, "NULL is invalid");
    iso_builder_t b;
    memset(&b, 0, sizeof(b));
    CHECK(iso_builder_validate(&b) == 0, "unbuilt is invalid");
    PASS();
}

/* -- Main ---------------------------------------------------- */

int main(void) {
    printf("+==================================================+\n");
    printf("|  WuBuOS ISO 9660 / Bootable ISO Test Suite        |\n");
    printf("+==================================================+\n\n");

    /* Lifecycle */
    test_builder_init();
    test_builder_init_null();

    /* File Management */
    test_add_files();
    test_add_dir();

    /* Boot Configuration */
    test_set_boot();

    /* Build */
    test_build_basic();
    test_build_bootable();

    /* Validation */
    test_validate_primary_vd();
    test_validate_terminator();

    /* El Torito */
    test_el_torito_bootable();
    test_el_torito_catalog();
    test_el_torito_checksum();
    test_non_bootable_is_bootable();

    /* Path Tables */
    test_path_tables();

    /* Root Directory */
    test_root_directory();
    test_root_directory_with_files();

    /* Layout */
    test_sector_layout();
    test_bootable_sector_layout();

    /* Image Access */
    test_image_access();
    test_validate_empty();

    printf("\n==================================================\n");
    printf("  Results: %d/%d passed, %d failed\n", g_pass, g_total, g_fail);
    printf("==================================================\n");

    return g_fail > 0 ? 1 : 0;
}
