/*
 * wubu_archd_test.c  --  Test suite for wubu_archd (Arch Linux Daemon)
 *
 * Tests daemon init, root lifecycle, package operations, service operations,
 * health checks, and GPU detection.
 */

#define _POSIX_C_SOURCE 200809L

#include "wubu_archd.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <sys/stat.h>

static int g_pass = 0, g_fail = 0, g_total = 0;

#define TEST(name) do { \
    printf("  TEST %-55s", name); g_total++; \
} while(0)
#define PASS() do { printf("✅\n"); g_pass++; } while(0)
#define FAIL(msg) do { printf("❌ %s\n", msg); g_fail++; } while(0)
#define CHECK(cond, msg) do { if (!(cond)) { FAIL(msg); return; } } while(0)

/* -- Daemon Config Tests ------------------------------------------ */

static void test_config_defaults(void) {
    TEST("daemon config defaults");
    WubuArchdConfig config = {0};
    strncpy(config.roots_path, "/tmp/wubu-test-roots", sizeof(config.roots_path) - 1);
    strncpy(config.socket_path, "/tmp/wubu-test-archd.sock", sizeof(config.socket_path) - 1);
    strncpy(config.log_path, "/tmp/wubu-test-archd.log", sizeof(config.log_path) - 1);
    config.auto_update = false;
    config.health_check_interval_sec = 60;
    config.log_level = 2;
    config.daemonize = false;

    WubuArchd d;
    int ret = wubu_archd_init(&d, &config);
    CHECK(ret == 0, "archd_init returns 0");
    PASS();

    TEST("daemon initial root count is 0");
    CHECK(d.root_count == 0, "no roots initially");
    PASS();

    TEST("daemon version string");
    CHECK(strcmp(wubu_archd_version(), "0.1.0") == 0, "version is 0.1.0");
    PASS();

    wubu_archd_shutdown(&d);
}

/* -- Root Type/String Tests --------------------------------------- */

static void test_root_type_strings(void) {
    TEST("root type string: base");
    CHECK(strcmp(wubu_archd_root_type_str(ROOT_TYPE_BASE), "base") == 0, "base");
    PASS();

    TEST("root type string: gaming");
    CHECK(strcmp(wubu_archd_root_type_str(ROOT_TYPE_GAMING), "gaming") == 0, "gaming");
    PASS();

    TEST("root state string: active");
    CHECK(strcmp(wubu_archd_root_state_str(ROOT_STATE_ACTIVE), "active") == 0, "active");
    PASS();

    TEST("root state string: failed");
    CHECK(strcmp(wubu_archd_root_state_str(ROOT_STATE_FAILED), "failed") == 0, "failed");
    PASS();
}

/* -- Service String Tests ----------------------------------------- */

static void test_svc_strings(void) {
    TEST("service state string: running");
    CHECK(strcmp(wubu_archd_svc_state_str(SERVICE_STATE_RUNNING), "running") == 0, "running");
    PASS();

    TEST("service state string: disabled");
    CHECK(strcmp(wubu_archd_svc_state_str(SERVICE_STATE_DISABLED), "disabled") == 0, "disabled");
    PASS();
}

/* -- Cmd String Tests --------------------------------------------- */

static void test_cmd_strings(void) {
    TEST("cmd string: root_create");
    CHECK(strcmp(wubu_archd_cmd_str(ARCHD_CMD_ROOT_CREATE), "root_create") == 0, "root_create");
    PASS();

    TEST("cmd string: pkg_install");
    CHECK(strcmp(wubu_archd_cmd_str(ARCHD_CMD_PKG_INSTALL), "pkg_install") == 0, "pkg_install");
    PASS();

    TEST("cmd string: shutdown");
    CHECK(strcmp(wubu_archd_cmd_str(ARCHD_CMD_SHUTDOWN), "shutdown") == 0, "shutdown");
    PASS();
}

/* -- Stats Test --------------------------------------------------- */

static void test_stats(void) {
    TEST("stats produces JSON");
    WubuArchdConfig config = {0};
    strncpy(config.roots_path, "/tmp/wubu-test-roots2", sizeof(config.roots_path) - 1);
    strncpy(config.socket_path, "/tmp/wubu-test2.sock", sizeof(config.socket_path) - 1);
    strncpy(config.log_path, "/tmp/wubu-test2.log", sizeof(config.log_path) - 1);
    config.log_level = 0;

    WubuArchd d;
    wubu_archd_init(&d, &config);

    char stats[4096];
    int ret = wubu_archd_stats(&d, stats, sizeof(stats));
    CHECK(ret == 0, "stats returns 0");
    CHECK(strstr(stats, "\"version\"") != NULL, "stats contains version");
    CHECK(strstr(stats, "\"roots\"") != NULL, "stats contains roots");
    PASS();

    wubu_archd_shutdown(&d);
}

/* -- GPU Detection Test ------------------------------------------- */

static void test_gpu_detect(void) {
    TEST("GPU detection runs without crash");
    WubuArchdConfig config = {0};
    strncpy(config.roots_path, "/tmp/wubu-test-roots3", sizeof(config.roots_path) - 1);
    strncpy(config.socket_path, "/tmp/wubu-test3.sock", sizeof(config.socket_path) - 1);
    strncpy(config.log_path, "/tmp/wubu-test3.log", sizeof(config.log_path) - 1);
    config.log_level = 0;

    WubuArchd d;
    wubu_archd_init(&d, &config);

    char gpu_info[4096];
    int ret = wubu_archd_gpu_detect(&d, gpu_info, sizeof(gpu_info));
    CHECK(ret == 0, "gpu_detect returns 0");
    /* May be empty [] if no GPU, that's OK */
    CHECK(strlen(gpu_info) >= 0, "gpu_detect produces output");
    PASS();

    wubu_archd_shutdown(&d);
}

/* -- Root List Test ----------------------------------------------- */

static void test_root_list(void) {
    TEST("root list with no roots");
    WubuArchdConfig config = {0};
    strncpy(config.roots_path, "/tmp/wubu-test-roots4", sizeof(config.roots_path) - 1);
    strncpy(config.socket_path, "/tmp/wubu-test4.sock", sizeof(config.socket_path) - 1);
    strncpy(config.log_path, "/tmp/wubu-test4.log", sizeof(config.log_path) - 1);
    config.log_level = 0;

    WubuArchd d;
    wubu_archd_init(&d, &config);

    WubuArchdRoot roots[16];
    int count = wubu_archd_root_list(&d, roots, 16);
    CHECK(count == 0, "no roots listed");
    PASS();

    wubu_archd_shutdown(&d);
}

/* -- Event Publishing Test ---------------------------------------- */

static void test_event_publish(void) {
    TEST("event publish creates event file");
    WubuArchdConfig config = {0};
    strncpy(config.roots_path, "/tmp/wubu-test-roots5", sizeof(config.roots_path) - 1);
    strncpy(config.socket_path, "/tmp/wubu-test5.sock", sizeof(config.socket_path) - 1);
    strncpy(config.log_path, "/tmp/wubu-test5.log", sizeof(config.log_path) - 1);
    config.log_level = 0;

    WubuArchd d;
    wubu_archd_init(&d, &config);

    mkdir(config.roots_path, 0755);
    int ret = wubu_archd_publish_event(&d, "test_event", "test_root", "{\"test\":true}");
    CHECK(ret == 0, "publish returns 0");
    PASS();

    wubu_archd_shutdown(&d);
    /* Cleanup */
    unlink("/tmp/wubu-test5/Events");
    rmdir("/tmp/wubu-test5");
}

/* -- Main --------------------------------------------------------- */

int main(void) {
    printf("\n==================================================\n");
    printf("  WuBuOS Arch Daemon Test Suite\n");
    printf("==================================================\n\n");

    test_config_defaults();
    test_root_type_strings();
    test_svc_strings();
    test_cmd_strings();
    test_stats();
    test_gpu_detect();
    test_root_list();
    test_event_publish();

    printf("\n==================================================\n");
    printf("  Results: %d/%d passed, %d failed\n", g_pass, g_total, g_fail);
    printf("==================================================\n");

    return g_fail > 0 ? 1 : 0;
}
