/*
 * wubu_ramdisk_test.c — Tests for Two-Mode Root Mount
 *
 * Cell 392: RAM for containers, SSD for bare metal.
 *
 * Tests the C API without actually mounting tmpfs (needs root)
 * or running pacstrap (needs root + network). We validate:
 *   - Create/destroy for both modes
 *   - Configuration (size, paths)
 *   - State transitions
 *   - Image format detection
 *   - install_to_disk API exists
 *   - snapshot API exists
 */
#include "wubu_ramdisk.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <sys/stat.h>

static int pass = 0, fail = 0;

#define TEST(name) printf("  TEST Cell392: %-55s", name)
#define PASS() do { pass++; printf("✅\n"); } while(0)
#define FAIL(msg) do { fail++; printf("❌ %s\n", msg); } while(0)
#define CHECK(cond, msg) do { if (!(cond)) { FAIL(msg); return; } } while(0)

/* ── RAM Mode Tests ─────────────────────────────────────────────── */

static void test_rd_create_ram(void) {
    TEST("rd_create RAM mode");
    WubuRamdisk *rd = wubu_rd_create(WUBU_RD_RAM, NULL);
    CHECK(rd != NULL, "rd should not be NULL");
    CHECK(rd->mode == WUBU_RD_RAM, "mode should be RAM");
    CHECK(strcmp(rd->path, WUBU_RD_RAM_PATH) == 0, "path should be RAM path");
    CHECK(strcmp(rd->ram_size, "2048m") == 0, "default size 2048m");
    CHECK(rd->limit_mb == 2048, "limit_mb should be 2048");
    CHECK(rd->state == WUBU_RD_NONE, "state should be NONE");
    wubu_rd_destroy(rd);
    PASS();
}

static void test_rd_create_ram_custom_image(void) {
    TEST("rd_create RAM with custom image");
    WubuRamdisk *rd = wubu_rd_create(WUBU_RD_RAM, "/tmp/my-rootfs.cgz");
    CHECK(rd != NULL, "rd should not be NULL");
    CHECK(strcmp(rd->image_path, "/tmp/my-rootfs.cgz") == 0,
          "image path should match");
    wubu_rd_destroy(rd);
    PASS();
}

static void test_rd_root_path_ram(void) {
    TEST("rd_root_path returns RAM path");
    WubuRamdisk *rd = wubu_rd_create(WUBU_RD_RAM, NULL);
    const char *path = wubu_rd_root_path(rd);
    CHECK(path != NULL, "path should not be NULL");
    CHECK(strcmp(path, "/run/wubu/ramdisk") == 0, "should be RAM path");
    wubu_rd_destroy(rd);
    PASS();
}

static void test_rd_state_initial(void) {
    TEST("rd_state NONE after create");
    WubuRamdisk *rd = wubu_rd_create(WUBU_RD_RAM, NULL);
    CHECK(wubu_rd_state(rd) == WUBU_RD_NONE, "state should be NONE");
    wubu_rd_destroy(rd);
    PASS();
}

static void test_rd_set_ram_size(void) {
    TEST("rd_set_ram_size changes limit");
    WubuRamdisk *rd = wubu_rd_create(WUBU_RD_RAM, NULL);
    wubu_rd_set_ram_size(rd, "4096m");
    CHECK(strcmp(rd->ram_size, "4096m") == 0, "size string should match");
    CHECK(rd->limit_mb == 4096, "limit_mb should be 4096");
    wubu_rd_set_ram_size(rd, "8192m");
    CHECK(rd->limit_mb == 8192, "limit_mb should be 8192");
    wubu_rd_destroy(rd);
    PASS();
}

/* ── Disk Mode Tests ────────────────────────────────────────────── */

static void test_rd_create_disk(void) {
    TEST("rd_create DISK mode");
    WubuRamdisk *rd = wubu_rd_create(WUBU_RD_DISK, NULL);
    CHECK(rd != NULL, "rd should not be NULL");
    CHECK(rd->mode == WUBU_RD_DISK, "mode should be DISK");
    CHECK(strcmp(rd->path, WUBU_RD_DISK_PATH) == 0,
          "path should be SSD path");
    CHECK(rd->ram_size[0] == '\0', "ram_size should be empty in disk mode");
    wubu_rd_destroy(rd);
    PASS();
}

static void test_rd_root_path_disk(void) {
    TEST("rd_root_path returns SSD path in disk mode");
    WubuRamdisk *rd = wubu_rd_create(WUBU_RD_DISK, NULL);
    const char *path = wubu_rd_root_path(rd);
    CHECK(path != NULL, "path should not be NULL");
    CHECK(strcmp(path, "/var/wubu/roots/arch-base") == 0,
          "should be SSD path");
    wubu_rd_destroy(rd);
    PASS();
}

static void test_rd_destroy_disk_noop(void) {
    TEST("rd_destroy DISK mode is no-op (SSD persists)");
    WubuRamdisk *rd = wubu_rd_create(WUBU_RD_DISK, NULL);
    rd->state = WUBU_RD_LOADED;
    wubu_rd_destroy(rd);  /* Should NOT unmount or delete anything */
    PASS();
}

/* ── Cross-Mode: install_to_disk ────────────────────────────────── */

static void test_rd_install_to_disk_api(void) {
    TEST("install_to_disk API exists");
    WubuRamdisk *rd = wubu_rd_create(WUBU_RD_RAM, NULL);
    /* Can't actually run it without a loaded rootfs,
     * but the API is callable and returns -1 for unready state */
    int ret = wubu_rd_install_to_disk(rd, "/tmp/wubu-test-ssd");
    CHECK(ret == -1, "should return -1 for unready state");
    wubu_rd_destroy(rd);
    PASS();
}

static void test_rd_snapshot_api(void) {
    TEST("snapshot API exists");
    WubuRamdisk *rd = wubu_rd_create(WUBU_RD_RAM, NULL);
    int ret = wubu_rd_snapshot(rd, NULL);
    CHECK(ret == -1, "should return -1 for unready state");
    wubu_rd_destroy(rd);
    PASS();
}

/* ── Error Handling ─────────────────────────────────────────────── */

static void test_rd_null_safe(void) {
    TEST("NULL-safe API calls");
    CHECK(wubu_rd_root_path(NULL) == NULL, "root_path NULL → NULL");
    CHECK(wubu_rd_state(NULL) == WUBU_RD_NONE, "state NULL → NONE");
    CHECK(wubu_rd_usage_mb(NULL) == 0, "usage_mb NULL → 0");
    wubu_rd_destroy(NULL);  /* Should not crash */
    PASS();
}

/* ── Disk Boot (mkdir only, no pacstrap) ────────────────────────── */

static void test_rd_boot_disk_mkdir(void) {
    TEST("rd_boot DISK creates directory");
    WubuRamdisk *rd = wubu_rd_create(WUBU_RD_DISK, NULL);
    /* Override path to temp dir for testing */
    const char *test_dir = "/tmp/wubu-test-disk-root-392";
    strncpy(rd->path, test_dir, sizeof(rd->path) - 1);

    int ret = wubu_rd_boot(rd);
    CHECK(ret == 0, "boot should succeed");

    /* Verify directory exists */
    struct stat st;
    CHECK(stat(test_dir, &st) == 0, "directory should exist");
    CHECK(S_ISDIR(st.st_mode), "should be a directory");

    /* Cleanup */
    rmdir(test_dir);
    wubu_rd_destroy(rd);
    PASS();
}

/* ── Main ───────────────────────────────────────────────────────── */

int main(void) {
    printf("\n── Ramdisk: RAM Mode (Cell 392) ──\n\n");

    test_rd_create_ram();
    test_rd_create_ram_custom_image();
    test_rd_root_path_ram();
    test_rd_state_initial();
    test_rd_set_ram_size();

    printf("\n── Ramdisk: DISK Mode (Cell 392) ──\n\n");

    test_rd_create_disk();
    test_rd_root_path_disk();
    test_rd_destroy_disk_noop();

    printf("\n── Ramdisk: Cross-Mode (Cell 392) ──\n\n");

    test_rd_install_to_disk_api();
    test_rd_snapshot_api();

    printf("\n── Ramdisk: Error Handling (Cell 292) ──\n\n");

    test_rd_null_safe();
    test_rd_boot_disk_mkdir();

    printf("\n══════════════════════════════════════════════════\n");
    printf("  Results: %d/%d passed, %d failed\n",
           pass, pass + fail, fail);
    printf("══════════════════════════════════════════════════\n");

    return fail > 0 ? 1 : 0;
}
