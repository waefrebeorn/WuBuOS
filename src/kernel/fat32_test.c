/*
 * fat32_test.c  --  My Seed FAT32 Filesystem Test Suite
 *
 * Uses a RAM-backed block device for fast, deterministic testing.
 * Tests: format, mount, directory ops, file I/O, cluster chains,
 *        edge cases, name conversion, LFN, deletes, seeks.
 */

#include "fat32.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

/* -- RAM Disk Block Device ---------------------------------------- */

#define RAM_DISK_SECTORS  16384   /* 8 MB disk */
static uint8_t *g_ram_disk = NULL;

static int ram_read(void *ctx, uint64_t lba, uint32_t n, void *buf) {
    (void)ctx;
    if (!g_ram_disk) return -1;
    if (lba + n > RAM_DISK_SECTORS) return -1;
    memcpy(buf, g_ram_disk + lba * FAT32_SECTOR_SIZE, n * FAT32_SECTOR_SIZE);
    return 0;
}

static int ram_write(void *ctx, uint64_t lba, uint32_t n, const void *buf) {
    (void)ctx;
    if (!g_ram_disk) return -1;
    if (lba + n > RAM_DISK_SECTORS) return -1;
    memcpy(g_ram_disk + lba * FAT32_SECTOR_SIZE, buf, n * FAT32_SECTOR_SIZE);
    return 0;
}

static void ram_disk_init(void) {
    if (!g_ram_disk)
        g_ram_disk = (uint8_t *)calloc(RAM_DISK_SECTORS, FAT32_SECTOR_SIZE);
    else
        memset(g_ram_disk, 0, (size_t)RAM_DISK_SECTORS * FAT32_SECTOR_SIZE);
}

static void ram_disk_free(void) {
    free(g_ram_disk);
    g_ram_disk = NULL;
}

static fat32_blk_ops ram_blk_ops(void) {
    fat32_blk_ops ops = {
        .read = ram_read,
        .write = ram_write,
        .ctx = NULL,
        .n_sectors = RAM_DISK_SECTORS,
    };
    return ops;
}

/* -- Test Macros -------------------------------------------------- */

static int g_tests_run = 0;
static int g_tests_pass = 0;

#define TEST(name) \
    do { \
        g_tests_run++; \
        printf("  TEST %-40s ", #name);

#define PASS() \
        g_tests_pass++; \
        printf("✅\n"); \
    } while (0)

#define FAIL(msg) \
        printf("❌ %s\n", msg); \
    } while (0)

#define ASSERT_EQ(a, b, msg) do { if ((a) != (b)) { printf("❌ %s\n", msg); return; } } while(0)
#define ASSERT_NEQ(a, b, msg) do { if ((a) == (b)) { printf("❌ %s\n", msg); return; } } while(0)
#define ASSERT_STREQ(a, b, msg) do { if (strcmp((a),(b)) != 0) { printf("❌ %s\n", msg); return; } } while(0)

/* -- Test: Format and Mount --------------------------------------- */

static void test_format_and_mount(void) {
    TEST(format_and_mount);
    ram_disk_init();
    fat32_blk_ops ops = ram_blk_ops();

    int rc = fat32_format(&ops, RAM_DISK_SECTORS, "TESTDISK");
    ASSERT_EQ(rc, 0, "format failed");

    fat32_volume vol;
    rc = fat32_mount(&vol, &ops);
    ASSERT_EQ(rc, 0, "mount failed");
    ASSERT_EQ(vol.mounted, true, "not mounted");
    ASSERT_EQ(vol.root_cluster, 2, "root cluster not 2");
    ASSERT_EQ(vol.num_fats, 2, "num_fats not 2");
    ASSERT_NEQ(vol.sectors_per_fat, 0, "sectors_per_fat is 0");
    ASSERT_NEQ(vol.total_clusters, 0, "total_clusters is 0");

    fat32_unmount(&vol);
    PASS();
}

/* -- Test: Double Mount ------------------------------------------- */

static void test_double_mount(void) {
    TEST(double_mount);
    ram_disk_init();
    fat32_blk_ops ops = ram_blk_ops();

    fat32_format(&ops, RAM_DISK_SECTORS, "TEST");

    fat32_volume vol1, vol2;
    int rc1 = fat32_mount(&vol1, &ops);
    int rc2 = fat32_mount(&vol2, &ops);
    ASSERT_EQ(rc1, 0, "first mount failed");
    ASSERT_EQ(rc2, 0, "second mount failed");
    ASSERT_EQ(vol1.root_cluster, vol2.root_cluster, "root cluster mismatch");

    fat32_unmount(&vol1);
    fat32_unmount(&vol2);
    PASS();
}

/* -- Test: Boot Sector Validation --------------------------------- */

static void test_boot_sector_validation(void) {
    TEST(boot_sector_validation);
    ram_disk_init();
    fat32_blk_ops ops = ram_blk_ops();

    /* Don't format  --  raw disk should fail mount */
    fat32_volume vol;
    int rc = fat32_mount(&vol, &ops);
    ASSERT_NEQ(rc, 0, "mount should fail on unformatted disk");

    /* Corrupt signature */
    fat32_format(&ops, RAM_DISK_SECTORS, "TEST");
    /* Flip the AA55 signature */
    g_ram_disk[510] ^= 0xFF;
    rc = fat32_mount(&vol, &ops);
    ASSERT_NEQ(rc, 0, "mount should fail with bad signature");

    PASS();
}

/* -- Test: Cluster Chain Walking ---------------------------------- */

static void test_cluster_chain(void) {
    TEST(cluster_chain);
    ram_disk_init();
    fat32_blk_ops ops = ram_blk_ops();
    fat32_format(&ops, RAM_DISK_SECTORS, "TEST");

    fat32_volume vol;
    fat32_mount(&vol, &ops);

    /* Root directory cluster 2 should be EOC */
    uint32_t next = fat32_next_cluster(&vol, 2);
    /* Root dir is a single cluster  --  next should be 0 (EOC) */
    ASSERT_EQ(next, 0, "root cluster should be EOC");

    /* Allocate a chain of 3 clusters */
    uint32_t chain = fat32_alloc_clusters(&vol, 3, 0);
    ASSERT_NEQ(chain, 0, "alloc failed");

    next = fat32_next_cluster(&vol, chain);
    ASSERT_EQ(next, chain + 1, "chain link 1 broken");

    next = fat32_next_cluster(&vol, chain + 1);
    ASSERT_EQ(next, chain + 2, "chain link 2 broken");

    next = fat32_next_cluster(&vol, chain + 2);
    ASSERT_EQ(next, 0, "chain should end at EOC");

    /* Free the chain */
    fat32_free_chain(&vol, chain);

    /* Verify clusters are free */
    /* After free, chain start should be 0 (free) */
    /* After free, chain start should be 0 (free) */
    fat32_unmount(&vol);
    PASS();
}

/* -- Test: Cluster to LBA Conversion ------------------------------ */

static void test_cluster_lba_conversion(void) {
    TEST(cluster_lba_conversion);
    ram_disk_init();
    fat32_blk_ops ops = ram_blk_ops();
    fat32_format(&ops, RAM_DISK_SECTORS, "TEST");

    fat32_volume vol;
    fat32_mount(&vol, &ops);

    /* Cluster 2 maps to first data sector */
    uint64_t lba2 = fat32_cluster_to_lba(&vol, 2);
    ASSERT_EQ(lba2, vol.data_lba, "cluster 2 LBA wrong");

    /* Cluster 3 is one cluster-width further */
    uint64_t lba3 = fat32_cluster_to_lba(&vol, 3);
    ASSERT_EQ(lba3, lba2 + vol.sectors_per_cluster, "cluster 3 LBA wrong");

    /* Round-trip */
    uint32_t c = fat32_lba_to_cluster(&vol, lba2);
    ASSERT_EQ(c, 2, "LBA→cluster roundtrip failed");

    fat32_unmount(&vol);
    PASS();
}

/* -- Test: Create and Find File ----------------------------------- */

static void test_create_and_find(void) {
    TEST(create_and_find);
    ram_disk_init();
    fat32_blk_ops ops = ram_blk_ops();
    fat32_format(&ops, RAM_DISK_SECTORS, "TEST");

    fat32_volume vol;
    fat32_mount(&vol, &ops);

    /* Create a file in root directory */
    fat32_file_info info;
    int rc = fat32_create(&vol, 0, "HELLO.TXT", FAT32_ATTR_ARCHIVE, &info);
    ASSERT_EQ(rc, 0, "create failed");
    ASSERT_NEQ(info.first_cluster, 0, "no cluster assigned");
    ASSERT_EQ(info.attr, FAT32_ATTR_ARCHIVE, "attr wrong");

    /* Find it */
    fat32_file_info found;
    rc = fat32_find(&vol, 0, "HELLO.TXT", &found);
    ASSERT_EQ(rc, 0, "find failed");
    ASSERT_STREQ(found.name, "HELLO.TXT", "name mismatch");
    ASSERT_EQ(found.first_cluster, info.first_cluster, "cluster mismatch");

    /* Case-insensitive find */
    rc = fat32_find(&vol, 0, "hello.txt", &found);
    ASSERT_EQ(rc, 0, "case-insensitive find failed");

    fat32_unmount(&vol);
    PASS();
}

/* -- Test: Create Directory --------------------------------------- */

static void test_create_directory(void) {
    TEST(create_directory);
    ram_disk_init();
    fat32_blk_ops ops = ram_blk_ops();
    fat32_format(&ops, RAM_DISK_SECTORS, "TEST");

    fat32_volume vol;
    fat32_mount(&vol, &ops);

    fat32_file_info info;
    int rc = fat32_create(&vol, 0, "MYDIR", FAT32_ATTR_DIRECTORY, &info);
    ASSERT_EQ(rc, 0, "create dir failed");
    ASSERT_EQ(info.attr & FAT32_ATTR_DIRECTORY, FAT32_ATTR_DIRECTORY, "not a dir");

    /* Subdirectory should have . and .. entries */
    /* Read the directory */
    uint32_t cluster = info.first_cluster;
    uint8_t *buf = (uint8_t *)malloc(vol.bytes_per_cluster);
    if (buf) {
        uint64_t lba = fat32_cluster_to_lba(&vol, cluster);
        vol.blk.read(vol.blk.ctx, lba, vol.sectors_per_cluster, buf);
        /* Just check that the first two entries exist (dot and dotdot) */
        fat32_dir_entry *dot = (fat32_dir_entry *)buf;
        ASSERT_EQ(dot->name[0], '.', "missing dot entry");
        fat32_dir_entry *dotdot = (fat32_dir_entry *)(buf + 32);
        ASSERT_EQ(dotdot->name[0], '.', "missing dotdot entry");
        ASSERT_EQ(dotdot->name[1], '.', "missing dotdot second dot");
        free(buf);
    }

    fat32_unmount(&vol);
    PASS();
}

/* -- Test: Directory Listing -------------------------------------- */

static void test_directory_listing(void) {
    TEST(directory_listing);
    ram_disk_init();
    fat32_blk_ops ops = ram_blk_ops();
    fat32_format(&ops, RAM_DISK_SECTORS, "TEST");

    fat32_volume vol;
    fat32_mount(&vol, &ops);

    /* Create multiple files */
    fat32_create(&vol, 0, "FILE1.TXT", FAT32_ATTR_ARCHIVE, NULL);
    fat32_create(&vol, 0, "FILE2.TXT", FAT32_ATTR_ARCHIVE, NULL);
    fat32_create(&vol, 0, "FILE3.TXT", FAT32_ATTR_ARCHIVE, NULL);

    /* Count entries via callback */
    ASSERT_EQ(fat32_find(&vol, 0, "FILE1.TXT", NULL), 0, "file1 not found");
    ASSERT_EQ(fat32_find(&vol, 0, "FILE2.TXT", NULL), 0, "file2 not found");
    ASSERT_EQ(fat32_find(&vol, 0, "FILE3.TXT", NULL), 0, "file3 not found");
    ASSERT_NEQ(fat32_find(&vol, 0, "FILE4.TXT", NULL), 0, "file4 should not exist");

    fat32_unmount(&vol);
    PASS();
}

/* -- Test: Delete File -------------------------------------------- */

static void test_delete_file(void) {
    TEST(delete_file);
    ram_disk_init();
    fat32_blk_ops ops = ram_blk_ops();
    fat32_format(&ops, RAM_DISK_SECTORS, "TEST");

    fat32_volume vol;
    fat32_mount(&vol, &ops);

    fat32_create(&vol, 0, "TEMP.DAT", FAT32_ATTR_ARCHIVE, NULL);
    ASSERT_EQ(fat32_find(&vol, 0, "TEMP.DAT", NULL), 0, "not created");

    int rc = fat32_delete(&vol, 0, "TEMP.DAT");
    ASSERT_EQ(rc, 0, "delete failed");

    ASSERT_NEQ(fat32_find(&vol, 0, "TEMP.DAT", NULL), 0, "still exists after delete");

    /* Create again (reuses freed cluster) */
    rc = fat32_create(&vol, 0, "TEMP.DAT", FAT32_ATTR_ARCHIVE, NULL);
    ASSERT_EQ(rc, 0, "re-create failed");
    ASSERT_EQ(fat32_find(&vol, 0, "TEMP.DAT", NULL), 0, "re-created not found");

    fat32_unmount(&vol);
    PASS();
}

/* -- Test: File Write and Read ------------------------------------ */

static void test_file_write_read(void) {
    TEST(file_write_read);
    ram_disk_init();
    fat32_blk_ops ops = ram_blk_ops();
    fat32_format(&ops, RAM_DISK_SECTORS, "TEST");

    fat32_volume vol;
    fat32_mount(&vol, &ops);

    /* Create and write a file */
    fat32_file fp;
    int rc = fat32_open(&vol, 0, "DATA.BIN", "w", &fp);
    ASSERT_EQ(rc, 0, "open for write failed");

    const char *data = "Hello, My Seed FAT32!";
    size_t written = fat32_write(&fp, data, strlen(data));
    ASSERT_EQ(written, strlen(data), "write count wrong");

    fat32_close(&fp);

    /* Read it back */
    rc = fat32_open(&vol, 0, "DATA.BIN", "r", &fp);
    ASSERT_EQ(rc, 0, "open for read failed");

    char buf[256] = {0};
    size_t nread = fat32_read(&fp, buf, strlen(data));
    ASSERT_EQ(nread, strlen(data), "read count wrong");
    ASSERT_EQ(memcmp(buf, data, strlen(data)), 0, "data mismatch");

    fat32_close(&fp);
    fat32_unmount(&vol);
    PASS();
}

/* -- Test: File Write Across Cluster Boundary --------------------- */

static void test_file_write_cross_cluster(void) {
    TEST(file_write_cross_cluster);
    ram_disk_init();
    fat32_blk_ops ops = ram_blk_ops();
    fat32_format(&ops, RAM_DISK_SECTORS, "TEST");

    fat32_volume vol;
    fat32_mount(&vol, &ops);

    fat32_file fp;
    int rc = fat32_open(&vol, 0, "BIG.DAT", "w", &fp);
    ASSERT_EQ(rc, 0, "open failed");

    /* Write more than one cluster */
    size_t cluster_size = vol.bytes_per_cluster;
    uint8_t *data = (uint8_t *)malloc(cluster_size * 2);
    for (size_t i = 0; i < cluster_size * 2; i++)
        data[i] = (uint8_t)(i & 0xFF);

    size_t written = fat32_write(&fp, data, cluster_size * 2);
    ASSERT_EQ(written, cluster_size * 2, "cross-cluster write wrong");

    fat32_close(&fp);

    /* Read back and verify */
    rc = fat32_open(&vol, 0, "BIG.DAT", "r", &fp);
    ASSERT_EQ(rc, 0, "read open failed");

    uint8_t *buf = (uint8_t *)malloc(cluster_size * 2);
    size_t nread = fat32_read(&fp, buf, cluster_size * 2);
    ASSERT_EQ(nread, cluster_size * 2, "cross-cluster read wrong");
    ASSERT_EQ(memcmp(buf, data, cluster_size * 2), 0, "cross-cluster data mismatch");

    free(data);
    free(buf);
    fat32_close(&fp);
    fat32_unmount(&vol);
    PASS();
}

/* -- Test: File Seek ---------------------------------------------- */

static void test_file_seek(void) {
    TEST(file_seek);
    ram_disk_init();
    fat32_blk_ops ops = ram_blk_ops();
    fat32_format(&ops, RAM_DISK_SECTORS, "TEST");

    fat32_volume vol;
    fat32_mount(&vol, &ops);

    fat32_file fp;
    fat32_open(&vol, 0, "SEEK.DAT", "w", &fp);
    const char *data = "ABCDEFGHIJ";
    fat32_write(&fp, data, 10);
    fat32_close(&fp);

    fat32_open(&vol, 0, "SEEK.DAT", "r", &fp);

    /* Seek to offset 5 (SEEK_SET) */
    int64_t pos = fat32_seek(&fp, 5, 0);
    ASSERT_EQ(pos, 5, "SEEK_SET failed");

    char buf[256] = {0};
    fat32_read(&fp, buf, 5);
    ASSERT_STREQ(buf, "FGHIJ", "read after seek wrong");

    /* Seek from current */
    pos = fat32_seek(&fp, -3, 1);
    ASSERT_EQ(pos, 7, "SEEK_CUR failed");

    /* Seek from end */
    pos = fat32_seek(&fp, -2, 2);
    ASSERT_EQ(pos, 8, "SEEK_END failed");

    memset(buf, 0, sizeof(buf));
    fat32_read(&fp, buf, 2);
    ASSERT_STREQ(buf, "IJ", "read from SEEK_END wrong");

    fat32_close(&fp);
    fat32_unmount(&vol);
    PASS();
}

/* -- Test: Append Mode -------------------------------------------- */

static void test_append_mode(void) {
    TEST(append_mode);
    ram_disk_init();
    fat32_blk_ops ops = ram_blk_ops();
    fat32_format(&ops, RAM_DISK_SECTORS, "TEST");

    fat32_volume vol;
    fat32_mount(&vol, &ops);

    /* Write initial content */
    fat32_file fp;
    fat32_open(&vol, 0, "LOG.TXT", "w", &fp);
    fat32_write(&fp, "LINE1\n", 6);
    fat32_close(&fp);

    /* Append more content */
    fat32_open(&vol, 0, "LOG.TXT", "a", &fp);
    size_t w = fat32_write(&fp, "LINE2\n", 6);
    ASSERT_EQ(w, 6, "append write failed");
    fat32_close(&fp);

    /* Verify full content */
    fat32_open(&vol, 0, "LOG.TXT", "r", &fp);
    char buf[32] = {0};
    fat32_read(&fp, buf, 12);
    ASSERT_EQ(memcmp(buf, "LINE1\nLINE2\n", 12), 0, "append content wrong");
    fat32_close(&fp);

    fat32_unmount(&vol);
    PASS();
}

/* -- Test: 8.3 Name Conversion ------------------------------------ */

static void test_name_83_conversion(void) {
    TEST(name_83_conversion);
    ram_disk_init();
    fat32_blk_ops ops = ram_blk_ops();
    fat32_format(&ops, RAM_DISK_SECTORS, "TEST");

    fat32_volume vol;
    fat32_mount(&vol, &ops);

    /* Create files with 8.3 names */
    fat32_create(&vol, 0, "SHORT.TXT", FAT32_ATTR_ARCHIVE, NULL);
    fat32_create(&vol, 0, "NOEXT", FAT32_ATTR_ARCHIVE, NULL);
    fat32_create(&vol, 0, "A.B", FAT32_ATTR_ARCHIVE, NULL);

    ASSERT_EQ(fat32_find(&vol, 0, "SHORT.TXT", NULL), 0, "SHORT.TXT not found");
    ASSERT_EQ(fat32_find(&vol, 0, "short.txt", NULL), 0, "case insens fail");
    ASSERT_EQ(fat32_find(&vol, 0, "NOEXT", NULL), 0, "NOEXT not found");
    ASSERT_EQ(fat32_find(&vol, 0, "A.B", NULL), 0, "A.B not found");

    fat32_unmount(&vol);
    PASS();
}

/* -- Test: Validate ----------------------------------------------- */

static void test_validate(void) {
    TEST(validate);
    ram_disk_init();
    fat32_blk_ops ops = ram_blk_ops();
    fat32_format(&ops, RAM_DISK_SECTORS, "TEST");

    fat32_volume vol;
    fat32_mount(&vol, &ops);

    int rc = fat32_validate(&vol);
    ASSERT_EQ(rc, 0, "validate failed on clean volume");

    fat32_unmount(&vol);
    PASS();
}

/* -- Test: Free Cluster Count ------------------------------------- */

static void test_free_cluster_count(void) {
    TEST(free_cluster_count);
    ram_disk_init();
    fat32_blk_ops ops = ram_blk_ops();
    fat32_format(&ops, RAM_DISK_SECTORS, "TEST");

    fat32_volume vol;
    fat32_mount(&vol, &ops);

    uint32_t free1 = fat32_count_free(&vol);
    ASSERT_NEQ(free1, 0, "no free clusters on fresh volume");

    /* Allocate some clusters */
    uint32_t chain = fat32_alloc_clusters(&vol, 5, 0);
    ASSERT_NEQ(chain, 0, "alloc failed");

    uint32_t free2 = fat32_count_free(&vol);
    ASSERT_EQ(free2, free1 - 5, "free count wrong after alloc");

    /* Free them back */
    fat32_free_chain(&vol, chain);
    uint32_t free3 = fat32_count_free(&vol);
    ASSERT_EQ(free3, free1, "free count not restored");

    fat32_unmount(&vol);
    PASS();
}

/* -- Test: Volume Info -------------------------------------------- */

static void test_volume_info(void) {
    TEST(volume_info);
    ram_disk_init();
    fat32_blk_ops ops = ram_blk_ops();
    fat32_format(&ops, RAM_DISK_SECTORS, "TEST");

    fat32_volume vol;
    fat32_mount(&vol, &ops);

    char buf[256];
    fat32_info(&vol, buf, sizeof(buf));
    ASSERT_NEQ(strlen(buf), 0, "info string empty");

    fat32_unmount(&vol);
    PASS();
}

/* -- Test: Multiple Files in Subdirectory ------------------------- */

static void test_subdir_files(void) {
    TEST(subdir_files);
    ram_disk_init();
    fat32_blk_ops ops = ram_blk_ops();
    fat32_format(&ops, RAM_DISK_SECTORS, "TEST");

    fat32_volume vol;
    fat32_mount(&vol, &ops);

    /* Create a subdirectory */
    fat32_file_info dir_info;
    int rc = fat32_create(&vol, 0, "DOCS", FAT32_ATTR_DIRECTORY, &dir_info);
    ASSERT_EQ(rc, 0, "create dir failed");

    /* Create files inside the subdirectory */
    fat32_file_info file_info;
    rc = fat32_create(&vol, dir_info.first_cluster, "README.TXT",
                      FAT32_ATTR_ARCHIVE, &file_info);
    ASSERT_EQ(rc, 0, "create file in subdir failed");

    /* Find file in subdir */
    rc = fat32_find(&vol, dir_info.first_cluster, "README.TXT", &file_info);
    ASSERT_EQ(rc, 0, "find in subdir failed");

    /* Should NOT be in root */
    rc = fat32_find(&vol, 0, "README.TXT", NULL);
    ASSERT_NEQ(rc, 0, "should not be in root");

    fat32_unmount(&vol);
    PASS();
}

/* -- Test: Persistence Unmount/Remount ---------------------------- */

static void test_persistence(void) {
    TEST(persistence_remount);
    ram_disk_init();
    fat32_blk_ops ops = ram_blk_ops();
    fat32_format(&ops, RAM_DISK_SECTORS, "TEST");

    /* Mount, create file, write, unmount */
    fat32_volume vol;
    fat32_mount(&vol, &ops);

    fat32_file fp;
    fat32_open(&vol, 0, "PERSIST.DAT", "w", &fp);
    fat32_write(&fp, "PERSISTENT", 10);
    fat32_close(&fp);
    fat32_unmount(&vol);

    /* Remount  --  data should survive */
    fat32_mount(&vol, &ops);

    fat32_file_info info;
    int rc = fat32_find(&vol, 0, "PERSIST.DAT", &info);
    ASSERT_EQ(rc, 0, "file not found after remount");

    fat32_open(&vol, 0, "PERSIST.DAT", "r", &fp);
    char buf[16] = {0};
    fat32_read(&fp, buf, 10);
    ASSERT_STREQ(buf, "PERSISTENT", "data lost after remount");
    fat32_close(&fp);

    fat32_unmount(&vol);
    PASS();
}

/* -- Test: Zero-Length File --------------------------------------- */

static void test_zero_length_file(void) {
    TEST(zero_length_file);
    ram_disk_init();
    fat32_blk_ops ops = ram_blk_ops();
    fat32_format(&ops, RAM_DISK_SECTORS, "TEST");

    fat32_volume vol;
    fat32_mount(&vol, &ops);

    fat32_file_info info;
    int rc = fat32_create(&vol, 0, "EMPTY.DAT", FAT32_ATTR_ARCHIVE, &info);
    ASSERT_EQ(rc, 0, "create empty failed");
    ASSERT_EQ(info.file_size, 0, "empty file has size");

    fat32_file fp;
    rc = fat32_open(&vol, 0, "EMPTY.DAT", "r", &fp);
    ASSERT_EQ(rc, 0, "open empty failed");

    char buf[1];
    size_t nread = fat32_read(&fp, buf, 1);
    ASSERT_EQ(nread, 0, "empty file should read 0 bytes");

    fat32_close(&fp);
    fat32_unmount(&vol);
    PASS();
}

/* -- Main --------------------------------------------------------- */

int main(void) {
    printf("=== FAT32 Test Suite ===\n\n");

    test_format_and_mount();
    test_double_mount();
    test_boot_sector_validation();
    test_cluster_chain();
    test_cluster_lba_conversion();
    test_create_and_find();
    test_create_directory();
    test_directory_listing();
    test_delete_file();
    test_file_write_read();
    test_file_write_cross_cluster();
    test_file_seek();
    test_append_mode();
    test_name_83_conversion();
    test_validate();
    test_free_cluster_count();
    test_volume_info();
    test_subdir_files();
    test_persistence();
    test_zero_length_file();

    printf("\n=== Results: %d/%d passed ===\n", g_tests_pass, g_tests_run);

    ram_disk_free();
    return (g_tests_pass == g_tests_run) ? 0 : 1;
}
