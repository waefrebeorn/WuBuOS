/*
 * weight_check_test.c — Test Suite for Vision Weight Verification
 *
 * Cell 051: Tests weight checking, shard path generation, validation.
 */

#include "weight_check.h"
#include <stdio.h>
#include <string.h>

static int g_pass = 0, g_fail = 0, g_total = 0;
#define TEST(name) printf("  TEST %-45s", name); g_total++;
#define PASS() do { printf("✅\n"); g_pass++; } while(0)
#define FAIL(msg) do { printf("❌ %s\n", msg); g_fail++; } while(0)
#define CHECK(cond, msg) do { if (!(cond)) { FAIL(msg); return; } } while(0)

/* ── Shard Path Tests ──────────────────────────────────────── */

static void test_shard_path(void) {
    TEST("shard path generation");
    char buf[256];
    int rc = weight_shard_path(0, buf, sizeof(buf));
    CHECK(rc == 0, "path 0 should succeed");
    CHECK(strstr(buf, "model-00001-of-00004") != NULL, "should contain shard 1 name");

    rc = weight_shard_path(3, buf, sizeof(buf));
    CHECK(rc == 0, "path 3 should succeed");
    CHECK(strstr(buf, "model-00004-of-00004") != NULL, "should contain shard 4 name");
    PASS();
}

static void test_shard_path_invalid(void) {
    TEST("shard path invalid index");
    char buf[256];
    CHECK(weight_shard_path(-1, buf, sizeof(buf)) == -1, "negative index fails");
    CHECK(weight_shard_path(4, buf, sizeof(buf)) == -1, "index 4 fails");
    CHECK(weight_shard_path(99, buf, sizeof(buf)) == -1, "index 99 fails");
    PASS();
}

/* ── Validate File Tests ──────────────────────────────────── */

static void test_validate_existing_file(void) {
    TEST("validate existing file");
    /* Use this source file as a test file — it exists and is > 0 bytes */
    uint64_t size = weight_validate_file(__FILE__, 1);
    CHECK(size > 0, "this source file should validate with min_size=1");
    PASS();
}

static void test_validate_nonexistent(void) {
    TEST("validate nonexistent file");
    uint64_t size = weight_validate_file("/nonexistent/path/to/file", 1);
    CHECK(size == 0, "nonexistent file should return 0");
    PASS();
}

static void test_validate_too_small(void) {
    TEST("validate file too small");
    /* This file exists but is way smaller than 4.9GB */
    uint64_t size = weight_validate_file(__FILE__, 4900000000ULL);
    CHECK(size == 0, "source file < 4.9GB should fail validation");
    PASS();
}

/* ── Weight Check (Full) Tests ─────────────────────────────── */

static void test_weight_check(void) {
    TEST("weight check against real model files");
    weight_check_t result;
    int rc = weight_check(&result);

    /* The weights may or may not be present depending on the environment */
    /* We just verify the check runs without crash and reports consistently */
    if (rc == 0) {
        /* All present and valid */
        CHECK(result.all_present == 1, "all_present should be 1");
        CHECK(result.all_valid == 1, "all_valid should be 1");
        CHECK(result.total_size > 0, "total_size should be > 0");
        for (int i = 0; i < 4; i++) {
            CHECK(result.present[i] == 1, "each shard should be present");
        }
        printf("  (moondream3 weights found: %llu GB total) ",
               (unsigned long long)(result.total_size / (1024*1024*1024)));
    } else {
        /* Weights not present or not valid */
        CHECK(result.all_valid == 0, "all_valid should be 0 on failure");
        printf("  (moondream3 weights NOT fully present) ");
    }
    PASS();
}

static void test_weight_check_null(void) {
    TEST("weight check NULL returns error");
    CHECK(weight_check(NULL) == -1, "NULL should return -1");
    PASS();
}

/* ── Directory Path Test ───────────────────────────────────── */

static void test_dir_path(void) {
    TEST("directory path set in result");
    weight_check_t result;
    weight_check(&result);
    CHECK(strlen(result.dir) > 0, "dir should be set");
    CHECK(strstr(result.dir, "moondream3") != NULL, "dir should contain moondream3");
    PASS();
}

/* ── Main ──────────────────────────────────────────────────── */

int main(void) {
    printf("╔══════════════════════════════════════════════════╗\n");
    printf("║  WuBuOS Vision Weight Check Test Suite             ║\n");
    printf("╚══════════════════════════════════════════════════╝\n\n");

    test_shard_path();
    test_shard_path_invalid();
    test_validate_existing_file();
    test_validate_nonexistent();
    test_validate_too_small();
    test_weight_check();
    test_weight_check_null();
    test_dir_path();

    printf("\n══════════════════════════════════════════════════\n");
    printf("  Results: %d/%d passed, %d failed\n", g_pass, g_total, g_fail);
    printf("══════════════════════════════════════════════════\n");

    return g_fail > 0 ? 1 : 0;
}
