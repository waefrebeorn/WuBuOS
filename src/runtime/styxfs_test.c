/*
 * styxfs_test.c  --  WuBuOS StyxFS Test Suite (Cell 106)
 *
 * Tests StyxFS mount/unmount, file namespace, .wubu container
 * detection, path normalization, and readonly mode.
 */

#include "styxfs.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int g_pass = 0, g_fail = 0, g_total = 0;
#define TEST(name) printf("  TEST %-50s", name); g_total++;
#define PASS() do { printf("✅\n"); g_pass++; } while(0)
#define FAIL(msg) do { printf("❌ %s\n", msg); g_fail++; } while(0)
#define CHECK(cond, msg) do { if (!(cond)) { FAIL(msg); return; } } while(0)

/* -- Tests ---------------------------------------------------- */

static void test_init(void) {
    TEST("styxfs_init zeroes server");
    styxfs_server_t srv;
    styxfs_init(&srv);
    CHECK(srv.mounts == NULL, "no mounts after init");
    CHECK(srv.readonly == 0, "not readonly by default");
    CHECK(srv.next_qid_path == 1, "qid_path starts at 1");
    PASS();
}

static void test_mount(void) {
    TEST("styxfs_mount adds mount point");
    styxfs_server_t srv;
    styxfs_init(&srv);
    int rc = styxfs_mount(&srv, "/wubu", "/var/wubu", 1);
    CHECK(rc == 0, "mount should succeed");
    CHECK(srv.mounts != NULL, "mounts should be non-NULL");
    CHECK(strcmp(srv.mounts->path, "/wubu") == 0, "path should be /wubu");
    CHECK(srv.mounts->is_container_repo == 1, "should be repo");
    PASS();
}

static void test_mount_multiple(void) {
    TEST("styxfs_mount multiple mount points");
    styxfs_server_t srv;
    styxfs_init(&srv);
    styxfs_mount(&srv, "/wubu", "/var/wubu", 1);
    styxfs_mount(&srv, "/apps", "/var/apps", 1);
    styxfs_mount(&srv, "/dev", "/dev", 0);
    int count = 0;
    for (styxfs_mount_t *m = srv.mounts; m; m = m->next) count++;
    CHECK(count == 3, "should have 3 mounts");
    PASS();
}

static void test_unmount(void) {
    TEST("styxfs_unmount removes mount point");
    styxfs_server_t srv;
    styxfs_init(&srv);
    styxfs_mount(&srv, "/wubu", "/var/wubu", 1);
    styxfs_mount(&srv, "/apps", "/var/apps", 1);
    int rc = styxfs_unmount(&srv, "/wubu");
    CHECK(rc == 0, "unmount should succeed");
    int count = 0;
    for (styxfs_mount_t *m = srv.mounts; m; m = m->next) count++;
    CHECK(count == 1, "should have 1 mount after unmount");
    PASS();
}

static void test_unmount_nonexistent(void) {
    TEST("styxfs_unmount nonexistent returns -1");
    styxfs_server_t srv;
    styxfs_init(&srv);
    int rc = styxfs_unmount(&srv, "/nope");
    CHECK(rc == -1, "should fail for nonexistent");
    PASS();
}

static void test_readonly(void) {
    TEST("styxfs_set_readonly");
    styxfs_server_t srv;
    styxfs_init(&srv);
    styxfs_set_readonly(&srv, 1);
    CHECK(srv.readonly == 1, "should be readonly");
    styxfs_set_readonly(&srv, 0);
    CHECK(srv.readonly == 0, "should be writable");
    PASS();
}

static void test_is_wubu_container(void) {
    TEST("styxfs_is_wubu_container detects .wubu extension");
    CHECK(styxfs_is_wubu_container("hello.wubu") == 1, "hello.wubu is container");
    CHECK(styxfs_is_wubu_container("/apps/editor.wubu") == 1, "path .wubu is container");
    CHECK(styxfs_is_wubu_container("readme.txt") == 0, ".txt is not container");
    CHECK(styxfs_is_wubu_container("wubu") == 0, "no extension is not container");
    CHECK(styxfs_is_wubu_container(NULL) == 0, "NULL is not container");
    PASS();
}

static void test_find_mount(void) {
    TEST("styxfs_find_mount resolves path to mount");
    styxfs_server_t srv;
    styxfs_init(&srv);
    styxfs_mount(&srv, "/wubu", "/var/wubu", 1);
    styxfs_mount(&srv, "/apps", "/var/apps", 1);
    char rel[STYXFS_MAX_PATH];
    styxfs_mount_t *m = styxfs_find_mount(&srv, "/wubu/hello.wubu", rel);
    CHECK(m != NULL, "should find /wubu mount");
    CHECK(strcmp(rel, "hello.wubu") == 0, "rel_path should be hello.wubu");
    PASS();
}

static void test_find_mount_miss(void) {
    TEST("styxfs_find_mount returns NULL for no match");
    styxfs_server_t srv;
    styxfs_init(&srv);
    styxfs_mount(&srv, "/wubu", "/var/wubu", 1);
    styxfs_mount_t *m = styxfs_find_mount(&srv, "/dev/cons", NULL);
    CHECK(m == NULL, "should not find mount for /dev");
    PASS();
}

static void test_scan_repo(void) {
    TEST("styxfs_scan_repo no crash");
    styxfs_server_t srv;
    styxfs_init(&srv);
    int rc = styxfs_scan_repo(&srv, "/wubu", "/var/wubu");
    CHECK(rc == 0, "scan_repo should succeed");
    PASS();
}

static void test_serve_null(void) {
    TEST("styxfs_serve with NULL returns -1");
    int rc = styxfs_serve(NULL, NULL, 0, NULL, NULL);
    CHECK(rc == -1, "should return -1 for NULL");
    PASS();
}

/* -- Main ----------------------------------------------------- */

int main(void) {
    printf("+========================================================+\n");
    printf("|  WuBuOS StyxFS Test Suite (Cell 106)                   |\n");
    printf("+========================================================+\n\n");

    test_init();
    test_mount();
    test_mount_multiple();
    test_unmount();
    test_unmount_nonexistent();
    test_readonly();
    test_is_wubu_container();
    test_find_mount();
    test_find_mount_miss();
    test_scan_repo();
    test_serve_null();

    printf("\n========================================================\n");
    printf("  Results: %d/%d passed, %d failed\n", g_pass, g_total, g_fail);
    printf("========================================================\n");

    return g_fail > 0 ? 1 : 0;
}
