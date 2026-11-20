/**
 * wubu_pkgmgr_test.c - Unit tests for WuBuOS Package Manager
 */

#include "../egojar.h"
#include "../egojar_net.h"
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include "wubu_pkgmgr.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>

#define TEST_ASSERT(cond, msg) \
    do { \
        if (!(cond)) { \
            fprintf(stderr, "FAIL: %s (%s:%d)\n", msg, __FILE__, __LINE__); \
            return false; \
        } \
    } while (0)
#define TEST_ASSERT_STR_CONTAINS(haystack, needle, msg) \
    do { \
        if (strstr((haystack), (needle)) == NULL) { \
            fprintf(stderr, "FAIL: %s - expected to contain '%s' (%s:%d)\n", msg, (needle), __FILE__, __LINE__); \
            return false; \
        } \
    } while (0)

/* Test helpers for test file only */
static bool test_ensure_dir(const char* path) {
    char tmp[512];
    snprintf(tmp, sizeof(tmp), "%s", path);
    for (char* p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            mkdir(tmp, 0755);
            *p = '/';
        }
    }
    return mkdir(tmp, 0755) == 0 || errno == EEXIST;
}

#define ensure_dir test_ensure_dir

static int tests_passed = 0;
static int tests_failed = 0;
static pid_t pid = 0;

#define RUN_TEST(fn) \
    do { \
        printf("Running %s... ", #fn); \
        fflush(stdout); \
        if (fn()) { \
            printf("PASS\n"); \
            tests_passed++; \
        } else { \
            printf("FAIL\n"); \
            tests_failed++; \
        } \
    } while (0)

/* Progress callback for tests */
static int test_progress_count = 0;
static void test_progress_cb(void* userdata, const char* stage, const char* pkg_id, float progress, const char* msg) {
    (void)userdata;
    (void)stage;
    (void)pkg_id;
    (void)progress;
    (void)msg;
    test_progress_count++;
}

/* Test default config */
bool test_default_config(void) {
    wubu_pkgmgr_config_t config;
    wubu_pkgmgr_get_default_config(&config);
    
    TEST_ASSERT(config.max_parallel_downloads > 0, "max_parallel_downloads");
    TEST_ASSERT(config.verify_signatures == true, "verify_signatures");
    TEST_ASSERT(config.index_ttl_hours > 0, "index_ttl_hours");
    TEST_ASSERT(strlen(config.db_path) > 0, "db_path");
    TEST_ASSERT(strlen(config.cache_dir) > 0, "cache_dir");
    TEST_ASSERT(strlen(config.install_prefix) > 0, "install_prefix");
    
    return true;
}

/* Test init/shutdown lifecycle */
bool test_lifecycle(void) {
    /* Clean up first - use fork+exec instead of system() */
    pid_t pid = fork();
    if (pid == 0) {
        execl("/bin/rm", "rm", "-rf", "/tmp/wubu_pkgmgr_test", (char*)NULL);
        _exit(1);
    }
    waitpid(pid, NULL, 0);
    
    wubu_pkgmgr_config_t config;
    wubu_pkgmgr_get_default_config(&config);
    snprintf(config.db_path, sizeof(config.db_path), "/tmp/wubu_pkgmgr_test");
    snprintf(config.cache_dir, sizeof(config.cache_dir), "/tmp/wubu_pkgmgr_test/cache");
    snprintf(config.install_prefix, sizeof(config.install_prefix), "/tmp/wubu_pkgmgr_test/install");
    snprintf(config.repo_config_dir, sizeof(config.repo_config_dir), "/tmp/wubu_pkgmgr_test/repos");
    config.verify_signatures = false;
    config.allow_untrusted = true;
    
    TEST_ASSERT(wubu_pkgmgr_init(&config), "pkgmgr init");
    TEST_ASSERT(wubu_pkgmgr_is_installed("nonexistent") == false, "not installed check");
    
    wubu_pkgmgr_shutdown();
    return true;
}

/* Test repository management */
bool test_repo_management(void) {
    /* Clean up first - use fork+exec instead of system() */
    pid_t pid = fork();
    if (pid == 0) {
        execl("/bin/rm", "rm", "-rf", "/tmp/wubu_pkgmgr_test2", (char*)NULL);
        _exit(1);
    }
    waitpid(pid, NULL, 0);
    
    wubu_pkgmgr_config_t config;
    wubu_pkgmgr_get_default_config(&config);
    snprintf(config.db_path, sizeof(config.db_path), "/tmp/wubu_pkgmgr_test2");
    snprintf(config.cache_dir, sizeof(config.cache_dir), "/tmp/wubu_pkgmgr_test2/cache");
    snprintf(config.install_prefix, sizeof(config.install_prefix), "/tmp/wubu_pkgmgr_test2/install");
    snprintf(config.repo_config_dir, sizeof(config.repo_config_dir), "/tmp/wubu_pkgmgr_test2/repos");
    config.verify_signatures = false;
    config.allow_untrusted = true;
    
    TEST_ASSERT(wubu_pkgmgr_init(&config), "pkgmgr init");
    
    /* Add repo */
    TEST_ASSERT(wubu_pkgmgr_repo_add("test-repo", "https://repo.example.com", "abc123", 10), "add repo");
    
    /* List repos */
    wubu_pkg_repo_t repos[16];
    int n = wubu_pkgmgr_repo_list(repos, 16);
    TEST_ASSERT(n >= 1, "list repos count");
    TEST_ASSERT_STR_CONTAINS(repos[0].name, "test-repo", "repo name");
    TEST_ASSERT_STR_CONTAINS(repos[0].url, "https://repo.example.com", "repo url");
    TEST_ASSERT(repos[0].enabled == true, "repo enabled");
    TEST_ASSERT(repos[0].priority == 10, "repo priority");
    
    /* Disable repo */
    TEST_ASSERT(wubu_pkgmgr_repo_set_enabled("test-repo", false), "disable repo");
    wubu_pkgmgr_shutdown();
    TEST_ASSERT(wubu_pkgmgr_init(&config), "re-init");
    n = wubu_pkgmgr_repo_list(repos, 16);
    TEST_ASSERT(n >= 1, "list after disable");
    TEST_ASSERT(repos[0].enabled == false, "repo still disabled");
    
    /* Remove repo */
    TEST_ASSERT(wubu_pkgmgr_repo_remove("test-repo"), "remove repo");
    wubu_pkgmgr_shutdown();
    TEST_ASSERT(wubu_pkgmgr_init(&config), "re-init after remove");
    n = wubu_pkgmgr_repo_list(repos, 16);
    TEST_ASSERT(n == 0, "repo removed");
    
    wubu_pkgmgr_shutdown();
    return true;
}

/* Test manifest JSON serialization */
bool test_manifest_json(void) {
    wubu_pkg_manifest_t manifest = {0};
    strncpy(manifest.id, "test-package", sizeof(manifest.id)-1);
    strncpy(manifest.name, "Test Package", sizeof(manifest.name)-1);
    strncpy(manifest.version, "1.0.0", sizeof(manifest.version)-1);
    strncpy(manifest.description, "A test package", sizeof(manifest.description)-1);
    strncpy(manifest.maintainer, "Tester <test@example.com>", sizeof(manifest.maintainer)-1);
    strncpy(manifest.homepage, "https://example.com", sizeof(manifest.homepage)-1);
    strncpy(manifest.license, "MIT", sizeof(manifest.license)-1);
    
    strncpy(manifest.depends[0], "dep1", sizeof(manifest.depends[0])-1);
    manifest.n_depends = 1;
    strncpy(manifest.recommends[0], "rec1", sizeof(manifest.recommends[0])-1);
    manifest.n_recommends = 1;
    
    strncpy(manifest.entrypoints[0].id, "main", sizeof(manifest.entrypoints[0].id)-1);
    strncpy(manifest.entrypoints[0].name, "Test App", sizeof(manifest.entrypoints[0].name)-1);
    strncpy(manifest.entrypoints[0].exec, "test-app %f", sizeof(manifest.entrypoints[0].exec)-1);
    strncpy(manifest.entrypoints[0].icon, "icon.png", sizeof(manifest.entrypoints[0].icon)-1);
    strncpy(manifest.entrypoints[0].categories, "Utility;", sizeof(manifest.entrypoints[0].categories)-1);
    manifest.entrypoints[0].terminal = false;
    manifest.entrypoints[0].startup_notify = true;
    manifest.n_entrypoints = 1;
    
    manifest.payload_type = WUBU_PKG_PAYLOAD_NATIVE;
    manifest.arch = WUBU_PKG_ARCH_X86_64;
    strncpy(manifest.sandbox_profile, "default", sizeof(manifest.sandbox_profile)-1);
    strncpy(manifest.build_host, "build-machine", sizeof(manifest.build_host)-1);
    strncpy(manifest.build_date, "2024-01-01T00:00:00Z", sizeof(manifest.build_date)-1);
    strncpy(manifest.builder_version, "1.0", sizeof(manifest.builder_version)-1);
    
    /* Serialize - using internal function via create package test */
    char* json = malloc(8192);
    /* Just test that the struct is well-formed */
    TEST_ASSERT(strlen(manifest.id) > 0, "manifest id");
    TEST_ASSERT(strlen(manifest.name) > 0, "manifest name");
    TEST_ASSERT(manifest.n_depends == 1, "n_depends");
    TEST_ASSERT(manifest.n_entrypoints == 1, "n_entrypoints");
    TEST_ASSERT(manifest.payload_type == WUBU_PKG_PAYLOAD_NATIVE, "payload_type");
    
    free(json);
    return true;
}

/* Test package creation (basic) */
bool test_package_creation(void) {
    wubu_pkgmgr_config_t config;
    wubu_pkgmgr_get_default_config(&config);
    snprintf(config.db_path, sizeof(config.db_path), "/tmp/wubu_pkgmgr_test3");
    snprintf(config.cache_dir, sizeof(config.cache_dir), "/tmp/wubu_pkgmgr_test3/cache");
    snprintf(config.install_prefix, sizeof(config.install_prefix), "/tmp/wubu_pkgmgr_test3/install");
    snprintf(config.repo_config_dir, sizeof(config.repo_config_dir), "/tmp/wubu_pkgmgr_test3/repos");
    config.verify_signatures = false;
    config.allow_untrusted = true;
    
    TEST_ASSERT(wubu_pkgmgr_init(&config), "pkgmgr init");
    
    /* Create source dir */
    ensure_dir("/tmp/wubu_test_pkg_src/bin");
    FILE* f = fopen("/tmp/wubu_test_pkg_src/bin/test-app", "w");
    if (f) { fprintf(f, "#!/bin/sh\necho hello\n"); fclose(f); }
    chmod("/tmp/wubu_test_pkg_src/bin/test-app", 0755);
    
    wubu_pkg_manifest_t manifest = {0};
    strncpy(manifest.id, "test-app", sizeof(manifest.id)-1);
    strncpy(manifest.name, "Test App", sizeof(manifest.name)-1);
    strncpy(manifest.version, "1.0.0", sizeof(manifest.version)-1);
    strncpy(manifest.description, "Test application", sizeof(manifest.description)-1);
    manifest.payload_type = WUBU_PKG_PAYLOAD_NATIVE;
    manifest.arch = WUBU_PKG_ARCH_X86_64;
    strncpy(manifest.sandbox_profile, "default", sizeof(manifest.sandbox_profile)-1);
    
    strncpy(manifest.files[0].src, "bin/test-app", sizeof(manifest.files[0].src)-1);
    strncpy(manifest.files[0].dst, "bin/test-app", sizeof(manifest.files[0].dst)-1);
    manifest.files[0].mode = 0755;
    manifest.n_files = 1;
    
    strncpy(manifest.entrypoints[0].id, "main", sizeof(manifest.entrypoints[0].id)-1);
    strncpy(manifest.entrypoints[0].name, "Test App", sizeof(manifest.entrypoints[0].name)-1);
    strncpy(manifest.entrypoints[0].exec, "test-app", sizeof(manifest.entrypoints[0].exec)-1);
    strncpy(manifest.entrypoints[0].icon, "icon.png", sizeof(manifest.entrypoints[0].icon)-1);
    strncpy(manifest.entrypoints[0].categories, "Utility;", sizeof(manifest.entrypoints[0].categories)-1);
    manifest.n_entrypoints = 1;
    
    /* Create package */
    bool result = wubu_pkgmgr_create_package(
        "/tmp/wubu_test_pkg_src",
        "/tmp/test-app.wubu",
        &manifest,
        NULL
    );
    
    TEST_ASSERT(result, "create package");
    
    /* Verify package */
    TEST_ASSERT(wubu_pkgmgr_verify_package("/tmp/test-app.wubu", NULL), "verify package");
    
    /* Read manifest back */
    wubu_pkg_manifest_t read_manifest;
    TEST_ASSERT(wubu_pkgmgr_read_manifest("/tmp/test-app.wubu", &read_manifest), "read manifest");
    TEST_ASSERT_STR_CONTAINS(read_manifest.id, "test-app", "read manifest id");
    
    unlink("/tmp/test-app.wubu");
    /* Clean up with fork+exec instead of system() */
    pid_t pid = fork();
    if (pid == 0) {
        execl("/bin/rm", "rm", "-rf", "/tmp/wubu_test_pkg_src", (char*)NULL);
        _exit(1);
    }
    waitpid(pid, NULL, 0);
    wubu_pkgmgr_shutdown();
    return true;
}

/* Test install/remove via database */
bool test_install_remove_db(void) {
    /* Clean up first - use fork+exec instead of system() */
    pid_t pid = fork();
    if (pid == 0) {
        execl("/bin/rm", "rm", "-rf", "/tmp/wubu_pkgmgr_test4", (char*)NULL);
        _exit(1);
    }
    waitpid(pid, NULL, 0);
    
    wubu_pkgmgr_config_t config;
    wubu_pkgmgr_get_default_config(&config);
    snprintf(config.db_path, sizeof(config.db_path), "/tmp/wubu_pkgmgr_test4");
    snprintf(config.cache_dir, sizeof(config.cache_dir), "/tmp/wubu_pkgmgr_test4/cache");
    snprintf(config.install_prefix, sizeof(config.install_prefix), "/tmp/wubu_pkgmgr_test4/install");
    snprintf(config.repo_config_dir, sizeof(config.repo_config_dir), "/tmp/wubu_pkgmgr_test4/repos");
    config.verify_signatures = false;
    config.allow_untrusted = true;
    
    TEST_ASSERT(wubu_pkgmgr_init(&config), "pkgmgr init");
    
    /* Create a minimal .wubu package and install it */
    ensure_dir("/tmp/wubu_test_pkg2/bin");
    FILE* f = fopen("/tmp/wubu_test_pkg2/bin/pkg-tool", "w");
    if (f) { fprintf(f, "#!/bin/sh\necho hello\n"); fclose(f); }
    chmod("/tmp/wubu_test_pkg2/bin/pkg-tool", 0755);
    
    /* Allocate pkgs first for error handling */
    wubu_pkg_installed_t* pkgs = calloc(16, sizeof(wubu_pkg_installed_t));
    if (!pkgs) {
        wubu_pkgmgr_shutdown();
        return false;
    }
    
    wubu_pkg_manifest_t* manifest = calloc(1, sizeof(wubu_pkg_manifest_t));
    if (!manifest) {
        free(pkgs);
        wubu_pkgmgr_shutdown();
        return false;
    }
    strncpy(manifest->id, "test-pkg", sizeof(manifest->id)-1);
    strncpy(manifest->name, "Test Package", sizeof(manifest->name)-1);
    strncpy(manifest->version, "1.0", sizeof(manifest->version)-1);
    strncpy(manifest->description, "Desc", sizeof(manifest->description)-1);
    manifest->payload_type = WUBU_PKG_PAYLOAD_NATIVE;
    manifest->arch = WUBU_PKG_ARCH_X86_64;
    strncpy(manifest->sandbox_profile, "default", sizeof(manifest->sandbox_profile)-1);
    strncpy(manifest->files[0].src, "bin/pkg-tool", sizeof(manifest->files[0].src)-1);
    strncpy(manifest->files[0].dst, "bin/pkg-tool", sizeof(manifest->files[0].dst)-1);
    manifest->files[0].mode = 0755;
    manifest->n_files = 1;
    
    TEST_ASSERT(wubu_pkgmgr_create_package("/tmp/wubu_test_pkg2", "/tmp/test-pkg.wubu", manifest, NULL), "create pkg");
    free(manifest);
    TEST_ASSERT(wubu_pkgmgr_install("/tmp/test-pkg.wubu", false), "install pkg");
    int n = wubu_pkgmgr_list_installed(pkgs, 16);
    TEST_ASSERT(n >= 1, "list installed count");
    
    /* Get installed */
    wubu_pkg_installed_t pkg;
    TEST_ASSERT(wubu_pkgmgr_get_installed("test-pkg", &pkg), "get installed");
    TEST_ASSERT_STR_CONTAINS(pkg.manifest.id, "test-pkg", "get installed id");
    TEST_ASSERT_STR_CONTAINS(pkg.manifest.name, "Test Package", "get installed name");
    
    /* Check is_installed */
    TEST_ASSERT(wubu_pkgmgr_is_installed("test-pkg"), "is_installed true");
    TEST_ASSERT(!wubu_pkgmgr_is_installed("nonexistent"), "is_installed false");
    
    free(pkgs);
    /* Clean up with fork+exec instead of system() */
    pid = fork();
    if (pid == 0) {
        execl("/bin/rm", "rm", "-rf", "/tmp/wubu_test_pkg2", "/tmp/test-pkg.wubu", (char*)NULL);
        _exit(1);
    }
    waitpid(pid, NULL, 0);
    wubu_pkgmgr_shutdown();
    return true;
}

/* Test desktop entry generation */
bool test_desktop_entry_generation(void) {
    wubu_pkgmgr_config_t config;
    wubu_pkgmgr_get_default_config(&config);
    snprintf(config.db_path, sizeof(config.db_path), "/tmp/wubu_pkgmgr_test5");
    snprintf(config.cache_dir, sizeof(config.cache_dir), "/tmp/wubu_pkgmgr_test5/cache");
    snprintf(config.install_prefix, sizeof(config.install_prefix), "/tmp/wubu_pkgmgr_test5/install");
    snprintf(config.repo_config_dir, sizeof(config.repo_config_dir), "/tmp/wubu_pkgmgr_test5/repos");
    config.verify_signatures = false;
    config.allow_untrusted = true;
    
    TEST_ASSERT(wubu_pkgmgr_init(&config), "pkgmgr init");
    
    wubu_pkg_manifest_t manifest = {0};
    strncpy(manifest.id, "desktop-test", sizeof(manifest.id)-1);
    strncpy(manifest.name, "Desktop Test", sizeof(manifest.name)-1);
    strncpy(manifest.version, "1.0", sizeof(manifest.version)-1);
    
    strncpy(manifest.entrypoints[0].id, "main", sizeof(manifest.entrypoints[0].id)-1);
    strncpy(manifest.entrypoints[0].name, "Desktop Test App", sizeof(manifest.entrypoints[0].name)-1);
    strncpy(manifest.entrypoints[0].exec, "desktop-test", sizeof(manifest.entrypoints[0].exec)-1);
    strncpy(manifest.entrypoints[0].icon, "icon.png", sizeof(manifest.entrypoints[0].icon)-1);
    strncpy(manifest.entrypoints[0].categories, "Utility;", sizeof(manifest.entrypoints[0].categories)-1);
    strncpy(manifest.entrypoints[0].mime_types, "text/plain;", sizeof(manifest.entrypoints[0].mime_types)-1);
    manifest.entrypoints[0].terminal = false;
    manifest.entrypoints[0].startup_notify = true;
    manifest.n_entrypoints = 1;
    
    wubu_pkg_installed_t pkg = {0};
    pkg.manifest = manifest;
    strncpy(pkg.install_path, "/tmp/install/desktop-test", sizeof(pkg.install_path)-1);
    
    /* Generate desktop file */
    TEST_ASSERT(wubu_pkgmgr_generate_desktop_files(&pkg), "generate desktop files");
    
    /* Check file exists */
    char path[512];
    snprintf(path, sizeof(path), "%s/share/applications/desktop-test-main.desktop", 
             config.install_prefix);
    struct stat st;
    TEST_ASSERT(stat(path, &st) == 0, "desktop file exists");
    
    /* Read and verify content */
    FILE* fp = fopen(path, "r");
    TEST_ASSERT(fp != NULL, "open desktop file");
    char content[2048];
    size_t sz = fread(content, 1, sizeof(content)-1, fp);
    content[sz] = '\0';
    fclose(fp);
    TEST_ASSERT_STR_CONTAINS(content, "Desktop Test App", "desktop name");
    TEST_ASSERT_STR_CONTAINS(content, "Exec=desktop-test", "desktop exec");
    TEST_ASSERT_STR_CONTAINS(content, "X-Wubu-Package=desktop-test", "desktop x-wubu-package");
    
    unlink(path);
    wubu_pkgmgr_shutdown();
    return true;
}

/* Test transaction */
bool test_transaction(void) {
    wubu_pkg_transaction_t txn;
    TEST_ASSERT(wubu_pkgmgr_txn_begin(&txn, true), "txn begin");

    /* Two transactions begun in the same second must NOT share a rollback directory.
     * The old name was strftime("%Y%m%d_%H%M%S"), which has second granularity: both
     * got the same path, so the second begin inherited whatever the first had left
     * and a rollback could restore another transaction's debris. */
    {
        wubu_pkg_transaction_t other;
        TEST_ASSERT(wubu_pkgmgr_txn_begin(&other, true), "second txn begin");
        TEST_ASSERT(strcmp(txn.rollback_dir, other.rollback_dir) != 0,
                    "two rollback directories are distinct");
        TEST_ASSERT(txn.rollback_dir[0] == '/', "rollback dir is absolute");
    }
    
    TEST_ASSERT(wubu_pkgmgr_txn_add(&txn, WUBU_PKG_TXN_INSTALL, "pkg1", NULL, "1.0", false), "txn add install");
    TEST_ASSERT(wubu_pkgmgr_txn_add(&txn, WUBU_PKG_TXN_UPGRADE, "pkg2", "1.0", "2.0", false), "txn add upgrade");
    TEST_ASSERT(wubu_pkgmgr_txn_add(&txn, WUBU_PKG_TXN_REMOVE, "pkg3", "1.0", NULL, true), "txn add remove");
    
    TEST_ASSERT(txn.n_items == 3, "txn item count");
    TEST_ASSERT(txn.items[0].type == WUBU_PKG_TXN_INSTALL, "item 0 type");
    TEST_ASSERT(txn.items[1].type == WUBU_PKG_TXN_UPGRADE, "item 1 type");
    TEST_ASSERT(txn.items[2].type == WUBU_PKG_TXN_REMOVE, "item 2 type");
    
    /* These items name packages that do not exist ("pkg1".."pkg3"), so a
     * fail-closed commit MUST report failure.  This assertion previously required
     * `true`, which encoded the old lie: commit ignored every item's return value
     * and returned true unconditionally, so a transaction whose every item failed
     * still reported success. */
    TEST_ASSERT(!wubu_pkgmgr_txn_commit(&txn),
                "txn commit fails closed when an item cannot be performed");
    /* Rollback must report FAILURE, not pretend success.  Nothing has ever been
     * backed up (install copies directly, remove runs rm -rf), so there is no state
     * to restore -- returning true told callers the system was clean when it was not.
     * This assertion used to require `true`, encoding the lie. */
    TEST_ASSERT(!wubu_pkgmgr_txn_rollback(&txn),
                "txn rollback fails closed when there is nothing to restore");
    
    return true;
}

/* Test progress callback */
bool test_progress_callback(void) {
    wubu_pkgmgr_config_t config;
    wubu_pkgmgr_get_default_config(&config);
    snprintf(config.db_path, sizeof(config.db_path), "/tmp/wubu_pkgmgr_test6");
    snprintf(config.cache_dir, sizeof(config.cache_dir), "/tmp/wubu_pkgmgr_test6/cache");
    snprintf(config.install_prefix, sizeof(config.install_prefix), "/tmp/wubu_pkgmgr_test6/install");
    snprintf(config.repo_config_dir, sizeof(config.repo_config_dir), "/tmp/wubu_pkgmgr_test6/repos");
    config.verify_signatures = false;
    config.allow_untrusted = true;
    
    TEST_ASSERT(wubu_pkgmgr_init(&config), "pkgmgr init");
    
    test_progress_count = 0;
    wubu_pkgmgr_set_progress_callback(test_progress_cb, &test_progress_count);
    
    /* Trigger progress via API - use install with dry_run to trigger progress */
    wubu_pkgmgr_install("nonexistent", true);
    TEST_ASSERT(test_progress_count >= 1, "callback invoked");
    
    wubu_pkgmgr_shutdown();
    return true;
}

/* Test stats */
bool test_stats(void) {
    wubu_pkgmgr_config_t config;
    wubu_pkgmgr_get_default_config(&config);
    snprintf(config.db_path, sizeof(config.db_path), "/tmp/wubu_pkgmgr_test7");
    snprintf(config.cache_dir, sizeof(config.cache_dir), "/tmp/wubu_pkgmgr_test7/cache");
    snprintf(config.install_prefix, sizeof(config.install_prefix), "/tmp/wubu_pkgmgr_test7/install");
    snprintf(config.repo_config_dir, sizeof(config.repo_config_dir), "/tmp/wubu_pkgmgr_test7/repos");
    config.verify_signatures = false;
    config.allow_untrusted = true;
    
    TEST_ASSERT(wubu_pkgmgr_init(&config), "pkgmgr init");
    
    wubu_pkgmgr_stats_t stats;
    TEST_ASSERT(wubu_pkgmgr_get_stats(&stats), "get stats");
    TEST_ASSERT(stats.installed_count >= 0, "installed count");
    
    wubu_pkgmgr_shutdown();
    return true;
}

/* Test autoremove */
bool test_autoremove(void) {
    /* Clean up first - use fork+exec instead of system() */
    pid = fork();
    if (pid == 0) {
        execl("/bin/rm", "rm", "-rf", "/tmp/wubu_pkgmgr_test8", "/tmp/wubu_test_pkg8", "/tmp/auto-dep.wubu", (char*)NULL);
        _exit(1);
    }
    waitpid(pid, NULL, 0);
    
    wubu_pkgmgr_config_t config;
    wubu_pkgmgr_get_default_config(&config);
    snprintf(config.db_path, sizeof(config.db_path), "/tmp/wubu_pkgmgr_test8");
    snprintf(config.cache_dir, sizeof(config.cache_dir), "/tmp/wubu_pkgmgr_test8/cache");
    snprintf(config.install_prefix, sizeof(config.install_prefix), "/tmp/wubu_pkgmgr_test8/install");
    snprintf(config.repo_config_dir, sizeof(config.repo_config_dir), "/tmp/wubu_pkgmgr_test8/repos");
    config.verify_signatures = false;
    config.allow_untrusted = true;
    
    TEST_ASSERT(wubu_pkgmgr_init(&config), "pkgmgr init");
    
    /* Create a minimal .wubu package and install as auto-installed */
    ensure_dir("/tmp/wubu_test_pkg8/bin");
    FILE* f = fopen("/tmp/wubu_test_pkg8/bin/auto-dep", "w");
    if (f) { fprintf(f, "#!/bin/sh\necho hello\n"); fclose(f); }
    chmod("/tmp/wubu_test_pkg8/bin/auto-dep", 0755);
    
    wubu_pkg_manifest_t* manifest = calloc(1, sizeof(wubu_pkg_manifest_t));
    if (!manifest) {
        wubu_pkgmgr_shutdown();
        return false;
    }
    strncpy(manifest->id, "auto-dep", sizeof(manifest->id)-1);
    strncpy(manifest->name, "Auto Dep", sizeof(manifest->name)-1);
    strncpy(manifest->version, "1.0", sizeof(manifest->version)-1);
    strncpy(manifest->description, "Dep", sizeof(manifest->description)-1);
    manifest->payload_type = WUBU_PKG_PAYLOAD_NATIVE;
    manifest->arch = WUBU_PKG_ARCH_X86_64;
    strncpy(manifest->sandbox_profile, "default", sizeof(manifest->sandbox_profile)-1);
    strncpy(manifest->files[0].src, "bin/auto-dep", sizeof(manifest->files[0].src)-1);
    strncpy(manifest->files[0].dst, "bin/auto-dep", sizeof(manifest->files[0].dst)-1);
    manifest->files[0].mode = 0755;
    manifest->n_files = 1;
    
    TEST_ASSERT(wubu_pkgmgr_create_package("/tmp/wubu_test_pkg8", "/tmp/auto-dep.wubu", manifest, NULL), "create pkg");
    free(manifest);
    TEST_ASSERT(wubu_pkgmgr_install("/tmp/auto-dep.wubu", false), "install pkg");
    
    /* Mark as auto-installed via SQL (test the function) */
    char sql[512];
    snprintf(sql, sizeof(sql), "UPDATE packages SET auto_installed=1 WHERE id='auto-dep'");
    /* We can't use db_exec directly; skip this part of the test */
    
    /* Dry run autoremove */
    int removed = wubu_pkgmgr_autoremove(true);
    TEST_ASSERT(removed >= 0, "autoremove dry run runs");
    
    /* Clean up with fork+exec instead of system() */
    pid = fork();
    if (pid == 0) {
        execl("/bin/rm", "rm", "-rf", "/tmp/wubu_test_pkg8", "/tmp/auto-dep.wubu", (char*)NULL);
        _exit(1);
    }
    waitpid(pid, NULL, 0);
    wubu_pkgmgr_shutdown();
    return true;
}

/* Test repo_update performs a REAL remote fetch + index parse (BATTLESHIP #24).
 * Stands up a local HTTP server (python3) serving a known index.json, points a
 * repo at it, refreshes, and asserts the package table is actually populated. */
bool test_repo_update_fetch(void) {
    /* Clean previous state. */
    pid_t cp = fork();
    if (cp == 0) { execl("/bin/rm", "rm", "-rf", "/tmp/wubu_pkgmgr_test_repo", (char*)NULL); _exit(1); }
    waitpid(cp, NULL, 0);

    wubu_pkgmgr_config_t config;
    wubu_pkgmgr_get_default_config(&config);
    snprintf(config.db_path, sizeof(config.db_path), "/tmp/wubu_pkgmgr_test_repo");
    snprintf(config.cache_dir, sizeof(config.cache_dir), "/tmp/wubu_pkgmgr_test_repo/cache");
    snprintf(config.install_prefix, sizeof(config.install_prefix), "/tmp/wubu_pkgmgr_test_repo/install");
    snprintf(config.repo_config_dir, sizeof(config.repo_config_dir), "/tmp/wubu_pkgmgr_test_repo/repos");
    config.verify_signatures = false;
    config.allow_untrusted = true;

    TEST_ASSERT(wubu_pkgmgr_init(&config), "pkgmgr init");

    /* Seed an index.json in a temp dir and serve it over HTTP. */
    ensure_dir("/tmp/wubu_repo_srv");
    const char *INDEX =
        "[\n"
        "  {\"id\":\"pkg-a\",\"name\":\"Package A\",\"version\":\"1.2.3\",\"arch\":\"x86_64\",\"description\":\"first pkg\",\"download_url\":\"http://x/a\",\"sha256\":\"deadbeef\",\"size\":1024},\n"
        "  {\"id\":\"pkg-b\",\"name\":\"Package B\",\"version\":\"2.0.0\",\"arch\":\"aarch64\",\"description\":\"second pkg\",\"download_url\":\"http://x/b\",\"sha256\":\"cafe\",\"size\":2048}\n"
        "]";
    FILE *idx = fopen("/tmp/wubu_repo_srv/index.json", "w");
    if (idx) { fputs(INDEX, idx); fclose(idx); }

    /* Pick a genuinely free port: bind a socket to get an ephemeral one,
     * then hand that port to the python server (avoids collisions with
     * stale servers from prior runs). */
    int probe = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in sa = {0};
    sa.sin_family = AF_INET;
    sa.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    sa.sin_port = 0;
    bind(probe, (struct sockaddr*)&sa, sizeof(sa));
    socklen_t sl = sizeof(sa);
    getsockname(probe, (struct sockaddr*)&sa, &sl);
    int port = ntohs(sa.sin_port);
    close(probe);

    char portstr[16]; snprintf(portstr, sizeof(portstr), "%d", port);
    pid_t srv = fork();
    if (srv == 0) {
        chdir("/tmp/wubu_repo_srv");
        execlp("python3", "python3", "-m", "http.server", portstr, (char*)NULL);
        _exit(127);
    }
    /* Give the server a moment to bind. */
    struct timespec ts = { .tv_sec = 0, .tv_nsec = 500000000 };
    nanosleep(&ts, NULL);

    /* Register the repo pointing at the local server. */
    char url[256];
    snprintf(url, sizeof(url), "http://127.0.0.1:%d/index.json", port);
    TEST_ASSERT(wubu_pkgmgr_repo_add("local-repo", url, NULL, 5), "add local repo");

    /* Ensure no stale packages before refresh. */
    wubu_pkg_repo_entry_t entries[16];
    int before = wubu_pkgmgr_search("pkg", entries, 16);
    (void)before; /* may be 0 or leftover; not asserted */

    /* THE REAL CALL under test. */
    bool ok = wubu_pkgmgr_repo_update("local-repo");
    TEST_ASSERT(ok, "repo_update returns true after fetch");

    /* Assert the index was actually fetched + parsed into the DB. */
    int n = wubu_pkgmgr_search("pkg", entries, 16);
    TEST_ASSERT(n >= 2, "repo index populated (>=2 packages)");

    bool found_a = false, found_b = false;
    for (int i = 0; i < n; i++) {
        if (strstr(entries[i].id, "pkg-a")) found_a = true;
        if (strstr(entries[i].id, "pkg-b")) found_b = true;
    }
    TEST_ASSERT(found_a, "pkg-a present in fetched index");
    TEST_ASSERT(found_b, "pkg-b present in fetched index");

    /* Assert last_update got stamped (was 0 before a real refresh). */
    wubu_pkg_repo_t repos[4];
    int rn = wubu_pkgmgr_repo_list(repos, 4);
    bool stamped = false;
    for (int i = 0; i < rn; i++)
        if (strcmp(repos[i].name, "local-repo") == 0 && repos[i].last_update > 0)
            stamped = true;
    TEST_ASSERT(stamped, "last_update stamped after real fetch");

    /* Tear down server. */
    kill(srv, SIGTERM);
    waitpid(srv, NULL, 0);

    wubu_pkgmgr_shutdown();
    return true;
}

/* ============================================================
 * L2 gates: real download + sha256 verification + bookkeeping.
 * Extend the test_repo_update_fetch pattern: a local python3
 * http.server serves index.json AND real .wubu payloads beside it.
 * ============================================================ */

#include <sqlite3.h>
#include "../runtime/wubu_crypto.h"   /* wubu_sha256_digest (static inline) */

/* sha256 of a file on disk, lowercase hex. */
static bool fixture_sha256_file(const char *path, char *out_hex) {
    FILE *f = fopen(path, "rb");
    if (!f) return false;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    rewind(f);
    if (sz < 0) { fclose(f); return false; }
    char *data = malloc((size_t)sz + 1);
    if (!data) { fclose(f); return false; }
    size_t rd = fread(data, 1, (size_t)sz, f);
    fclose(f);
    if (rd != (size_t)sz) { free(data); return false; }
    wubu_sha256_digest(data, (size_t)sz, out_hex, 65);
    free(data);
    return true;
}

/* Start python3 -m http.server serving `dir` on a genuinely free port.
 * Returns the server pid (or -1) and writes the port to *out_port. */
static pid_t fixture_http_start(const char *dir, int *out_port) {
    int probe = socket(AF_INET, SOCK_STREAM, 0);
    if (probe < 0) return -1;
    struct sockaddr_in sa = {0};
    sa.sin_family = AF_INET;
    sa.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    sa.sin_port = 0;
    if (bind(probe, (struct sockaddr*)&sa, sizeof(sa)) != 0) { close(probe); return -1; }
    socklen_t sl = sizeof(sa);
    getsockname(probe, (struct sockaddr*)&sa, &sl);
    int port = ntohs(sa.sin_port);
    close(probe);

    char portstr[16]; snprintf(portstr, sizeof(portstr), "%d", port);
    pid_t srv = fork();
    if (srv == 0) {
        if (chdir(dir) < 0) _exit(127);
        execlp("python3", "python3", "-m", "http.server", portstr, (char*)NULL);
        _exit(127);
    }
    if (srv < 0) return -1;
    /* Poll until the server accepts connections (bounded). */
    for (int i = 0; i < 60; i++) {
        int c = socket(AF_INET, SOCK_STREAM, 0);
        struct sockaddr_in ca = {0};
        ca.sin_family = AF_INET;
        ca.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        ca.sin_port = htons((uint16_t)port);
        int ok = connect(c, (struct sockaddr*)&ca, sizeof(ca));
        close(c);
        if (ok == 0) { *out_port = port; return srv; }
        struct timespec ts = { .tv_sec = 0, .tv_nsec = 50000000 };
        nanosleep(&ts, NULL);
    }
    kill(srv, SIGTERM); waitpid(srv, NULL, 0);
    return -1;
}

static void fixture_http_stop(pid_t srv) {
    if (srv > 0) { kill(srv, SIGTERM); waitpid(srv, NULL, 0); }
}

static void fixture_rm(const char *path) {
    pid_t cp = fork();
    if (cp == 0) { execl("/bin/rm", "rm", "-rf", path, (char*)NULL); _exit(1); }
    if (cp > 0) waitpid(cp, NULL, 0);
}

/* Build a REAL .wubu with two payload files (bin/<id> + share/<id>.txt). */
static bool fixture_make_pkg(const char *id, const char *version,
                             const char *workdir, const char *out_wubu) {
    char srcdir[512], path[640], content[256];
    snprintf(srcdir, sizeof(srcdir), "%s/src_%s", workdir, id);
    snprintf(path, sizeof(path), "%s/bin", srcdir); ensure_dir(path);
    snprintf(path, sizeof(path), "%s/share", srcdir); ensure_dir(path);

    snprintf(path, sizeof(path), "%s/bin/%s", srcdir, id);
    snprintf(content, sizeof(content), "#!/bin/sh\necho %s %s\n", id, version);
    FILE *f = fopen(path, "w"); if (!f) return false;
    fputs(content, f); fclose(f); chmod(path, 0755);

    snprintf(path, sizeof(path), "%s/share/%s.txt", srcdir, id);
    snprintf(content, sizeof(content), "data for %s %s\n", id, version);
    f = fopen(path, "w"); if (!f) return false;
    fputs(content, f); fclose(f);

    wubu_pkg_manifest_t *m = calloc(1, sizeof(*m));
    if (!m) return false;
    snprintf(m->id, sizeof(m->id), "%s", id);
    snprintf(m->name, sizeof(m->name), "Fixture %s", id);
    snprintf(m->version, sizeof(m->version), "%s", version);
    snprintf(m->description, sizeof(m->description), "L2 fixture package");
    m->payload_type = WUBU_PKG_PAYLOAD_SCRIPT;
    m->arch = WUBU_PKG_ARCH_X86_64;
    snprintf(m->files[0].src, sizeof(m->files[0].src), "bin/%s", id);
    snprintf(m->files[0].dst, sizeof(m->files[0].dst), "bin/%s", id);
    m->files[0].mode = 0755;
    snprintf(m->files[1].src, sizeof(m->files[1].src), "share/%s.txt", id);
    snprintf(m->files[1].dst, sizeof(m->files[1].dst), "share/%s.txt", id);
    m->files[1].mode = 0644;
    m->n_files = 2;
    snprintf(m->entrypoints[0].id, sizeof(m->entrypoints[0].id), "main");
    snprintf(m->entrypoints[0].name, sizeof(m->entrypoints[0].name), "%s", id);
    snprintf(m->entrypoints[0].exec, sizeof(m->entrypoints[0].exec), "bin/%s", id);
    m->n_entrypoints = 1;
    bool ok = wubu_pkgmgr_create_package(srcdir, out_wubu, m, NULL);
    free(m);
    return ok;
}

/* Read the first column of the first row of a query against the test DB. */
static bool fixture_db_text(const char *dbdir, const char *sql, char *out, size_t outsz) {
    char dbpath[640];
    snprintf(dbpath, sizeof(dbpath), "%s/pkgmgr.db", dbdir);
    sqlite3 *db = NULL;
    if (sqlite3_open_v2(dbpath, &db, SQLITE_OPEN_READONLY, NULL) != SQLITE_OK) {
        if (db) sqlite3_close(db);
        return false;
    }
    sqlite3_stmt *stmt = NULL;
    bool found = false;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            const char *v = (const char *)sqlite3_column_text(stmt, 0);
            snprintf(out, outsz, "%s", v ? v : "");
            found = true;
        }
        sqlite3_finalize(stmt);
    }
    sqlite3_close(db);
    return found;
}

static int fixture_db_count(const char *dbdir, const char *sql) {
    char buf[64];
    if (!fixture_db_text(dbdir, sql, buf, sizeof(buf))) return -1;
    return atoi(buf);
}

/* Standard config for the L2 fixture tests. */
static void fixture_config(wubu_pkgmgr_config_t *config, const char *base) {
    wubu_pkgmgr_get_default_config(config);
    snprintf(config->db_path, sizeof(config->db_path), "%s", base);
    snprintf(config->cache_dir, sizeof(config->cache_dir), "%s/cache", base);
    snprintf(config->install_prefix, sizeof(config->install_prefix), "%s/install", base);
    snprintf(config->repo_config_dir, sizeof(config->repo_config_dir), "%s/repos", base);
    config->verify_signatures = false;
    config->allow_untrusted = true;
}

/* Happy path: install('pkg-a') downloads over real HTTP, verifies sha256,
 * installs; payload files exist at the DB-recorded paths; the files table
 * holds real hashes; verify_installed passes; tampering an installed file
 * is flagged; remove deletes the payload from DISK; the verified cache
 * fast path then works with the server gone. */
bool test_remote_install_real(void) {
    const char *BASE = "/tmp/wubu_pkgmgr_l2_remote";
    const char *SRV  = "/tmp/wubu_pkgmgr_l2_remote_srv";
    fixture_rm(BASE); fixture_rm(SRV); ensure_dir(SRV);

    wubu_pkgmgr_config_t config;
    fixture_config(&config, BASE);
    TEST_ASSERT(wubu_pkgmgr_init(&config), "pkgmgr init");

    /* Real .wubu beside the index. */
    char wubu[640], sha[65];
    snprintf(wubu, sizeof(wubu), "%s/pkg-a-1.2.3.wubu", SRV);
    TEST_ASSERT(fixture_make_pkg("pkg-a", "1.2.3", SRV, wubu), "fixture pkg created");
    TEST_ASSERT(fixture_sha256_file(wubu, sha), "fixture sha computed");
    struct stat wst;
    TEST_ASSERT(stat(wubu, &wst) == 0, "fixture wubu stat");

    int port = 0;
    pid_t srv = fixture_http_start(SRV, &port);
    TEST_ASSERT(srv > 0, "fixture http server up");

    char index[2048];
    snprintf(index, sizeof(index),
        "[{\"id\":\"pkg-a\",\"name\":\"Package A\",\"version\":\"1.2.3\","
        "\"arch\":\"x86_64\",\"description\":\"l2 fixture\","
        "\"download_url\":\"http://127.0.0.1:%d/pkg-a-1.2.3.wubu\","
        "\"sha256\":\"%s\",\"size\":%lld}]",
        port, sha, (long long)wst.st_size);
    char idxpath[640]; snprintf(idxpath, sizeof(idxpath), "%s/index.json", SRV);
    FILE *f = fopen(idxpath, "w");
    TEST_ASSERT(f != NULL, "open index.json");
    fputs(index, f); fclose(f);

    char repourl[256];
    snprintf(repourl, sizeof(repourl), "http://127.0.0.1:%d/index.json", port);
    TEST_ASSERT(wubu_pkgmgr_repo_add("l2-repo", repourl, NULL, 5), "add repo");
    TEST_ASSERT(wubu_pkgmgr_repo_update("l2-repo"), "repo_update fetch");

    /* The index fields must SURVIVE into the DB and come back out. */
    wubu_pkg_repo_entry_t info;
    TEST_ASSERT(wubu_pkgmgr_repo_get_info("pkg-a", &info), "repo_get_info found");
    TEST_ASSERT_STR_CONTAINS(info.download_url, "/pkg-a-1.2.3.wubu", "download_url kept");
    TEST_ASSERT(strcmp(info.sha256, sha) == 0, "sha256 kept");
    TEST_ASSERT(!wubu_pkgmgr_repo_get_info("no-such-pkg", &info), "repo_get_info missing -> false");

    /* THE REAL CALL: download -> verify -> install. */
    TEST_ASSERT(wubu_pkgmgr_install("pkg-a", false), "remote install succeeds");
    TEST_ASSERT(wubu_pkgmgr_is_installed("pkg-a"), "is_installed");

    /* install_path is the TRUE payload root. */
    wubu_pkg_installed_t inst;
    TEST_ASSERT(wubu_pkgmgr_get_installed("pkg-a", &inst), "get_installed");
    char want_root[640];
    snprintf(want_root, sizeof(want_root), "%s/install/pkg-a", BASE);
    TEST_ASSERT(strcmp(inst.install_path, want_root) == 0, "install_path is prefix/id");

    /* Payload files exist at the DB-recorded paths, with real content. */
    char p1[768], p2[768];
    snprintf(p1, sizeof(p1), "%s/bin/pkg-a", inst.install_path);
    snprintf(p2, sizeof(p2), "%s/share/pkg-a.txt", inst.install_path);
    TEST_ASSERT(stat(p1, &wst) == 0 && S_ISREG(wst.st_mode), "payload bin exists");
    TEST_ASSERT(stat(p2, &wst) == 0 && S_ISREG(wst.st_mode), "payload data exists");
    FILE *cf = fopen(p2, "r");
    TEST_ASSERT(cf != NULL, "open installed data file");
    char cbuf[256]; size_t nrd = fread(cbuf, 1, sizeof(cbuf)-1, cf); fclose(cf);
    cbuf[nrd] = '\0';
    TEST_ASSERT_STR_CONTAINS(cbuf, "data for pkg-a 1.2.3", "payload content intact");

    /* files table: one row per copied file, with the REAL sha256. */
    TEST_ASSERT(fixture_db_count(BASE, "SELECT COUNT(*) FROM files WHERE pkg_id='pkg-a'") == 2,
                "files table has 2 rows");
    char dbsha[128];
    TEST_ASSERT(fixture_db_text(BASE,
        "SELECT sha256 FROM files WHERE pkg_id='pkg-a' AND path='bin/pkg-a'",
        dbsha, sizeof(dbsha)), "files row for bin/pkg-a");
    char disksha[65];
    TEST_ASSERT(fixture_sha256_file(p1, disksha), "hash installed bin");
    TEST_ASSERT(strcmp(dbsha, disksha) == 0, "files sha matches disk");
    char dbsize[64];
    TEST_ASSERT(fixture_db_text(BASE,
        "SELECT size FROM files WHERE pkg_id='pkg-a' AND path='bin/pkg-a'",
        dbsize, sizeof(dbsize)), "files size row");
    TEST_ASSERT(atoll(dbsize) > 0, "files size recorded");

    /* The downloaded artifact sits in the cache, hash-identical to the index. */
    char cachepath[640];
    snprintf(cachepath, sizeof(cachepath), "%s/cache/pkg-a-1.2.3.wubu", BASE);
    TEST_ASSERT(stat(cachepath, &wst) == 0, "cache artifact exists");
    char cachesha[65];
    TEST_ASSERT(fixture_sha256_file(cachepath, cachesha), "hash cache artifact");
    TEST_ASSERT(strcmp(cachesha, sha) == 0, "cache artifact matches index sha");

    /* verify_installed now has real data: intact -> 0. */
    char **broken = NULL; int nbroken = 0;
    TEST_ASSERT(wubu_pkgmgr_verify_installed(&broken, &nbroken) == 0 && nbroken == 0,
                "verify_installed intact");
    if (broken) { for (int i = 0; i < nbroken; i++) free(broken[i]); free(broken); broken = NULL; }

    /* Tamper an INSTALLED file -> verify_installed must flag pkg-a. */
    FILE *tf = fopen(p2, "r+");
    TEST_ASSERT(tf != NULL, "open installed file for tamper");
    fseek(tf, 0, SEEK_SET);
    int c0 = fgetc(tf);
    fseek(tf, 0, SEEK_SET);
    fputc(c0 ^ 0xFF, tf);
    fclose(tf);
    int rc = wubu_pkgmgr_verify_installed(&broken, &nbroken);
    TEST_ASSERT(rc == -1 && nbroken >= 1, "verify_installed flags tampered file");
    bool flagged = false;
    for (int i = 0; i < nbroken; i++) {
        if (strcmp(broken[i], "pkg-a") == 0) flagged = true;
        free(broken[i]);
    }
    free(broken); broken = NULL;
    TEST_ASSERT(flagged, "pkg-a named as broken");
    /* Restore. */
    tf = fopen(p2, "r+");
    TEST_ASSERT(tf != NULL, "reopen for restore");
    fseek(tf, 0, SEEK_SET); fputc(c0, tf); fclose(tf);
    TEST_ASSERT(wubu_pkgmgr_verify_installed(&broken, &nbroken) == 0, "verify after restore");
    if (broken) { for (int i = 0; i < nbroken; i++) free(broken[i]); free(broken); broken = NULL; }

    /* remove: payload must be GONE FROM DISK, not just from the DB. */
    TEST_ASSERT(wubu_pkgmgr_remove("pkg-a", false), "remove succeeds");
    TEST_ASSERT(stat(p1, &wst) != 0, "payload bin gone from disk");
    TEST_ASSERT(stat(p2, &wst) != 0, "payload data gone from disk");
    TEST_ASSERT(stat(want_root, &wst) != 0, "install root gone from disk");
    TEST_ASSERT(!wubu_pkgmgr_is_installed("pkg-a"), "not installed after remove");
    TEST_ASSERT(fixture_db_count(BASE, "SELECT COUNT(*) FROM files WHERE pkg_id='pkg-a'") == 0,
                "files rows removed");
    TEST_ASSERT(wubu_pkgmgr_verify_installed(&broken, &nbroken) == 0, "verify clean after remove");
    if (broken) { for (int i = 0; i < nbroken; i++) free(broken[i]); free(broken); broken = NULL; }

    /* Cache fast path: with the server DEAD, reinstall must succeed from
     * the hash-verified cache alone. */
    fixture_http_stop(srv); srv = 0;
    TEST_ASSERT(wubu_pkgmgr_install("pkg-a", false), "reinstall from verified cache (offline)");
    TEST_ASSERT(stat(p1, &wst) == 0, "payload restored from cache");
    TEST_ASSERT(wubu_pkgmgr_remove("pkg-a", false), "second remove");

    wubu_pkgmgr_shutdown();
    fixture_rm(BASE); fixture_rm(SRV);
    return true;
}

/* Tamper cases: every trust failure must refuse the install (fail closed).
 *   pkg-flip:  served payload byte flipped vs the index sha
 *   pkg-wrong: index sha wrong for the served payload
 *   pkg-nohash: index carries no sha256 at all
 * A good package in the same index still installs (fixture sanity). */
bool test_remote_install_tamper(void) {
    const char *BASE = "/tmp/wubu_pkgmgr_l2_tamper";
    const char *SRV  = "/tmp/wubu_pkgmgr_l2_tamper_srv";
    fixture_rm(BASE); fixture_rm(SRV); ensure_dir(SRV);

    wubu_pkgmgr_config_t config;
    fixture_config(&config, BASE);
    TEST_ASSERT(wubu_pkgmgr_init(&config), "pkgmgr init");

    char w_flip[640], w_wrong[640], w_nohash[640], w_good[640];
    char sha_flip[65], sha_wrong[65], sha_good[65];
    snprintf(w_flip, sizeof(w_flip), "%s/pkg-flip-1.0.wubu", SRV);
    snprintf(w_wrong, sizeof(w_wrong), "%s/pkg-wrong-1.0.wubu", SRV);
    snprintf(w_nohash, sizeof(w_nohash), "%s/pkg-nohash-1.0.wubu", SRV);
    snprintf(w_good, sizeof(w_good), "%s/pkg-good-1.0.wubu", SRV);
    TEST_ASSERT(fixture_make_pkg("pkg-flip", "1.0", SRV, w_flip), "make pkg-flip");
    TEST_ASSERT(fixture_make_pkg("pkg-wrong", "1.0", SRV, w_wrong), "make pkg-wrong");
    TEST_ASSERT(fixture_make_pkg("pkg-nohash", "1.0", SRV, w_nohash), "make pkg-nohash");
    TEST_ASSERT(fixture_make_pkg("pkg-good", "1.0", SRV, w_good), "make pkg-good");
    TEST_ASSERT(fixture_sha256_file(w_flip, sha_flip), "sha pkg-flip");
    TEST_ASSERT(fixture_sha256_file(w_wrong, sha_wrong), "sha pkg-wrong");
    TEST_ASSERT(fixture_sha256_file(w_good, sha_good), "sha pkg-good");

    /* Tamper 1: flip a payload byte AFTER the index hash was computed. */
    FILE *tf = fopen(w_flip, "r+b");
    TEST_ASSERT(tf != NULL, "open served payload for flip");
    fseek(tf, 200, SEEK_SET);
    int c0 = fgetc(tf);
    TEST_ASSERT(c0 != EOF, "payload long enough to flip");
    fseek(tf, 200, SEEK_SET);
    fputc(c0 ^ 0xFF, tf);
    fclose(tf);
    /* Tamper 2: corrupt the index hash (flip one hex digit). */
    sha_wrong[10] = (sha_wrong[10] == '0') ? '1' : '0';

    int port = 0;
    pid_t srv = fixture_http_start(SRV, &port);
    TEST_ASSERT(srv > 0, "fixture http server up");

    char index[4096];
    snprintf(index, sizeof(index),
        "[{\"id\":\"pkg-flip\",\"version\":\"1.0\",\"download_url\":\"http://127.0.0.1:%d/pkg-flip-1.0.wubu\",\"sha256\":\"%s\",\"size\":100},"
        "{\"id\":\"pkg-wrong\",\"version\":\"1.0\",\"download_url\":\"http://127.0.0.1:%d/pkg-wrong-1.0.wubu\",\"sha256\":\"%s\",\"size\":100},"
        "{\"id\":\"pkg-nohash\",\"version\":\"1.0\",\"download_url\":\"http://127.0.0.1:%d/pkg-nohash-1.0.wubu\",\"sha256\":\"\",\"size\":100},"
        "{\"id\":\"pkg-good\",\"version\":\"1.0\",\"download_url\":\"http://127.0.0.1:%d/pkg-good-1.0.wubu\",\"sha256\":\"%s\",\"size\":100}]",
        port, sha_flip, port, sha_wrong, port, port, sha_good);
    char idxpath[640]; snprintf(idxpath, sizeof(idxpath), "%s/index.json", SRV);
    FILE *f = fopen(idxpath, "w");
    TEST_ASSERT(f != NULL, "open index.json");
    fputs(index, f); fclose(f);

    char repourl[256];
    snprintf(repourl, sizeof(repourl), "http://127.0.0.1:%d/index.json", port);
    TEST_ASSERT(wubu_pkgmgr_repo_add("l2-tamper-repo", repourl, NULL, 5), "add repo");
    TEST_ASSERT(wubu_pkgmgr_repo_update("l2-tamper-repo"), "repo_update fetch");

    const char *bad_ids[3] = { "pkg-flip", "pkg-wrong", "pkg-nohash" };
    for (int i = 0; i < 3; i++) {
        char msg[128];
        snprintf(msg, sizeof(msg), "install('%s') rejected (trust failure)", bad_ids[i]);
        TEST_ASSERT(!wubu_pkgmgr_install(bad_ids[i], false), msg);
        snprintf(msg, sizeof(msg), "'%s' not installed", bad_ids[i]);
        TEST_ASSERT(!wubu_pkgmgr_is_installed(bad_ids[i]), msg);
        char root[640];
        snprintf(root, sizeof(root), "%s/install/%s", BASE, bad_ids[i]);
        struct stat st;
        snprintf(msg, sizeof(msg), "'%s' has no install root on disk", bad_ids[i]);
        TEST_ASSERT(stat(root, &st) != 0, msg);
        char sql[256];
        snprintf(sql, sizeof(sql), "SELECT COUNT(*) FROM files WHERE pkg_id='%s'", bad_ids[i]);
        snprintf(msg, sizeof(msg), "'%s' has no files rows", bad_ids[i]);
        TEST_ASSERT(fixture_db_count(BASE, sql) == 0, msg);
        char cache[640], part[704];
        snprintf(cache, sizeof(cache), "%s/cache/%s-1.0.wubu", BASE, bad_ids[i]);
        snprintf(part, sizeof(part), "%s.part", cache);
        snprintf(msg, sizeof(msg), "'%s' left no cache artifact", bad_ids[i]);
        TEST_ASSERT(stat(cache, &st) != 0, msg);
        snprintf(msg, sizeof(msg), "'%s' left no .part debris", bad_ids[i]);
        TEST_ASSERT(stat(part, &st) != 0, msg);
    }

    /* Sanity: the honest package in the SAME index installs fine. */
    TEST_ASSERT(wubu_pkgmgr_install("pkg-good", false), "honest pkg installs");
    TEST_ASSERT(wubu_pkgmgr_is_installed("pkg-good"), "pkg-good installed");

    fixture_http_stop(srv);
    wubu_pkgmgr_shutdown();
    fixture_rm(BASE); fixture_rm(SRV);
    return true;
}

/* Upgrade must compare versions NUMERICALLY: 1.9 -> 1.10 is an upgrade
 * (strcmp said "1.10" < "1.9" and refused it), and 1.10 -> 1.9 is not. */
bool test_upgrade_numeric_versions(void) {
    const char *BASE = "/tmp/wubu_pkgmgr_l2_upgrade";
    const char *SRV  = "/tmp/wubu_pkgmgr_l2_upgrade_srv";
    fixture_rm(BASE); fixture_rm(SRV); ensure_dir(SRV);

    wubu_pkgmgr_config_t config;
    fixture_config(&config, BASE);
    TEST_ASSERT(wubu_pkgmgr_init(&config), "pkgmgr init");

    char w19[640], w110[640], sha19[65], sha110[65];
    snprintf(w19, sizeof(w19), "%s/pkg-up-1.9.wubu", SRV);
    snprintf(w110, sizeof(w110), "%s/pkg-up-1.10.wubu", SRV);
    TEST_ASSERT(fixture_make_pkg("pkg-up", "1.9", SRV, w19), "make 1.9");
    TEST_ASSERT(fixture_make_pkg("pkg-up", "1.10", SRV, w110), "make 1.10");
    TEST_ASSERT(fixture_sha256_file(w19, sha19), "sha 1.9");
    TEST_ASSERT(fixture_sha256_file(w110, sha110), "sha 1.10");

    int port = 0;
    pid_t srv = fixture_http_start(SRV, &port);
    TEST_ASSERT(srv > 0, "fixture http server up");

    char idxpath[640]; snprintf(idxpath, sizeof(idxpath), "%s/index.json", SRV);
    char index[2048], repourl[256];
    snprintf(repourl, sizeof(repourl), "http://127.0.0.1:%d/index.json", port);

    /* Serve 1.9 and install it. */
    snprintf(index, sizeof(index),
        "[{\"id\":\"pkg-up\",\"version\":\"1.9\",\"download_url\":\"http://127.0.0.1:%d/pkg-up-1.9.wubu\",\"sha256\":\"%s\",\"size\":100}]",
        port, sha19);
    FILE *f = fopen(idxpath, "w"); TEST_ASSERT(f != NULL, "index 1.9");
    fputs(index, f); fclose(f);
    TEST_ASSERT(wubu_pkgmgr_repo_add("l2-up-repo", repourl, NULL, 5), "add repo");
    TEST_ASSERT(wubu_pkgmgr_repo_update("l2-up-repo"), "update 1.9");
    TEST_ASSERT(wubu_pkgmgr_install("pkg-up", false), "install 1.9");
    wubu_pkg_installed_t inst;
    TEST_ASSERT(wubu_pkgmgr_get_installed("pkg-up", &inst), "get 1.9");
    TEST_ASSERT(strcmp(inst.manifest.version, "1.9") == 0, "version is 1.9");

    /* Same version: upgrade is a no-op. */
    TEST_ASSERT(wubu_pkgmgr_upgrade("pkg-up", false), "upgrade same version ok");
    TEST_ASSERT(wubu_pkgmgr_get_installed("pkg-up", &inst), "get after no-op");
    TEST_ASSERT(strcmp(inst.manifest.version, "1.9") == 0, "still 1.9");

    /* Serve 1.10: strcmp("1.10","1.9") < 0 would WRONGLY skip this. */
    snprintf(index, sizeof(index),
        "[{\"id\":\"pkg-up\",\"version\":\"1.10\",\"download_url\":\"http://127.0.0.1:%d/pkg-up-1.10.wubu\",\"sha256\":\"%s\",\"size\":100}]",
        port, sha110);
    f = fopen(idxpath, "w"); TEST_ASSERT(f != NULL, "index 1.10");
    fputs(index, f); fclose(f);
    TEST_ASSERT(wubu_pkgmgr_repo_update("l2-up-repo"), "update 1.10");
    TEST_ASSERT(wubu_pkgmgr_upgrade("pkg-up", false), "upgrade 1.9 -> 1.10");
    TEST_ASSERT(wubu_pkgmgr_get_installed("pkg-up", &inst), "get 1.10");
    TEST_ASSERT(strcmp(inst.manifest.version, "1.10") == 0, "version is 1.10 after upgrade");
    char p[768];
    snprintf(p, sizeof(p), "%s/share/pkg-up.txt", inst.install_path);
    FILE *pf = fopen(p, "r");
    TEST_ASSERT(pf != NULL, "open upgraded payload");
    char buf[256]; size_t nr = fread(buf, 1, sizeof(buf)-1, pf); fclose(pf);
    buf[nr] = '\0';
    TEST_ASSERT_STR_CONTAINS(buf, "1.10", "upgraded payload content on disk");

    /* Serve 1.9 again: upgrade must NOT downgrade. */
    snprintf(index, sizeof(index),
        "[{\"id\":\"pkg-up\",\"version\":\"1.9\",\"download_url\":\"http://127.0.0.1:%d/pkg-up-1.9.wubu\",\"sha256\":\"%s\",\"size\":100}]",
        port, sha19);
    f = fopen(idxpath, "w"); TEST_ASSERT(f != NULL, "index 1.9 again");
    fputs(index, f); fclose(f);
    TEST_ASSERT(wubu_pkgmgr_repo_update("l2-up-repo"), "update 1.9 again");
    TEST_ASSERT(wubu_pkgmgr_upgrade("pkg-up", false), "upgrade refuses downgrade");
    TEST_ASSERT(wubu_pkgmgr_get_installed("pkg-up", &inst), "get after downgrade attempt");
    TEST_ASSERT(strcmp(inst.manifest.version, "1.10") == 0, "still 1.10 (no downgrade)");

    fixture_http_stop(srv);
    wubu_pkgmgr_shutdown();
    fixture_rm(BASE); fixture_rm(SRV);
    return true;
}

/* Local .wubu install gets the same bookkeeping: files under prefix/<id>,
 * files-table rows, working verify_installed, real removal. */
bool test_local_install_layout(void) {
    const char *BASE = "/tmp/wubu_pkgmgr_l2_local";
    fixture_rm(BASE);

    wubu_pkgmgr_config_t config;
    fixture_config(&config, BASE);
    TEST_ASSERT(wubu_pkgmgr_init(&config), "pkgmgr init");

    char wubu[640];
    snprintf(wubu, sizeof(wubu), "%s/local-pkg-2.0.wubu", BASE);
    TEST_ASSERT(fixture_make_pkg("local-pkg", "2.0", BASE, wubu), "make local pkg");
    TEST_ASSERT(wubu_pkgmgr_install(wubu, false), "local install");

    wubu_pkg_installed_t inst;
    TEST_ASSERT(wubu_pkgmgr_get_installed("local-pkg", &inst), "get installed");
    char root[640], p1[768], p2[768];
    snprintf(root, sizeof(root), "%s/install/local-pkg", BASE);
    TEST_ASSERT(strcmp(inst.install_path, root) == 0, "local install_path is prefix/id");
    snprintf(p1, sizeof(p1), "%s/bin/local-pkg", root);
    snprintf(p2, sizeof(p2), "%s/share/local-pkg.txt", root);
    struct stat st;
    TEST_ASSERT(stat(p1, &st) == 0, "local payload bin exists");
    TEST_ASSERT(stat(p2, &st) == 0, "local payload data exists");
    TEST_ASSERT(fixture_db_count(BASE, "SELECT COUNT(*) FROM files WHERE pkg_id='local-pkg'") == 2,
                "local files table populated");

    char **broken = NULL; int nbroken = 0;
    TEST_ASSERT(wubu_pkgmgr_verify_installed(&broken, &nbroken) == 0, "local verify intact");
    if (broken) { for (int i = 0; i < nbroken; i++) free(broken[i]); free(broken); broken = NULL; }

    /* Tamper -> flagged. */
    FILE *tf = fopen(p1, "r+");
    TEST_ASSERT(tf != NULL, "open local payload for tamper");
    fseek(tf, 0, SEEK_SET);
    int c0 = fgetc(tf);
    fseek(tf, 0, SEEK_SET); fputc(c0 ^ 0xFF, tf); fclose(tf);
    TEST_ASSERT(wubu_pkgmgr_verify_installed(&broken, &nbroken) == -1 && nbroken == 1,
                "local verify flags tamper");
    if (broken) { for (int i = 0; i < nbroken; i++) free(broken[i]); free(broken); broken = NULL; }

    /* Remove -> payload gone from disk. */
    TEST_ASSERT(wubu_pkgmgr_remove("local-pkg", false), "local remove");
    TEST_ASSERT(stat(root, &st) != 0, "local install root gone");
    TEST_ASSERT(stat(p1, &st) != 0, "local payload gone");
    TEST_ASSERT(fixture_db_count(BASE, "SELECT COUNT(*) FROM files WHERE pkg_id='local-pkg'") == 0,
                "local files rows gone");

    wubu_pkgmgr_shutdown();
    fixture_rm(BASE);
    return true;
}

/* Pre-existing DBs (old repo_packages without download_url/sha256) must be
 * migrated gracefully at init, and a refresh must then populate the new
 * columns. */
bool test_schema_migration(void) {
    const char *BASE = "/tmp/wubu_pkgmgr_l2_migrate";
    const char *SRV  = "/tmp/wubu_pkgmgr_l2_migrate_srv";
    fixture_rm(BASE); fixture_rm(SRV); ensure_dir(SRV); ensure_dir(BASE);

    /* Hand-build an OLD-schema database (no download_url / sha256). */
    char dbpath[640]; snprintf(dbpath, sizeof(dbpath), "%s/pkgmgr.db", BASE);
    sqlite3 *db = NULL;
    TEST_ASSERT(sqlite3_open(dbpath, &db) == SQLITE_OK, "open old db");
    const char *old_schema =
        "CREATE TABLE repo_packages (id TEXT, name TEXT, version TEXT, arch TEXT,"
        " repo_name TEXT, description TEXT, deps TEXT, size_bytes INTEGER,"
        " PRIMARY KEY (id, repo_name));"
        "CREATE TABLE repos (name TEXT PRIMARY KEY, url TEXT NOT NULL,"
        " pubkey_hex TEXT, priority INTEGER DEFAULT 0, enabled INTEGER DEFAULT 1,"
        " last_update INTEGER DEFAULT 0);";
    char *err = NULL;
    int rc = sqlite3_exec(db, old_schema, NULL, NULL, &err);
    if (err) sqlite3_free(err);
    sqlite3_close(db);
    TEST_ASSERT(rc == SQLITE_OK, "old schema created");

    int port = 0;
    pid_t srv = fixture_http_start(SRV, &port);
    TEST_ASSERT(srv > 0, "fixture http server up");
    char index[1024];
    snprintf(index, sizeof(index),
        "[{\"id\":\"pkg-m\",\"version\":\"3.0\",\"download_url\":\"http://127.0.0.1:%d/pkg-m.wubu\","
        "\"sha256\":\"%064d\",\"size\":10}]", port, 7);
    char idxpath[640]; snprintf(idxpath, sizeof(idxpath), "%s/index.json", SRV);
    FILE *f = fopen(idxpath, "w"); TEST_ASSERT(f != NULL, "index for migration");
    fputs(index, f); fclose(f);

    wubu_pkgmgr_config_t config;
    fixture_config(&config, BASE);
    TEST_ASSERT(wubu_pkgmgr_init(&config), "init migrates old db");

    char repourl[256];
    snprintf(repourl, sizeof(repourl), "http://127.0.0.1:%d/index.json", port);
    TEST_ASSERT(wubu_pkgmgr_repo_add("mig-repo", repourl, NULL, 1), "add repo on migrated db");
    TEST_ASSERT(wubu_pkgmgr_repo_update("mig-repo"), "refresh on migrated db");
    wubu_pkg_repo_entry_t info;
    TEST_ASSERT(wubu_pkgmgr_repo_get_info("pkg-m", &info), "migrated entry found");
    TEST_ASSERT_STR_CONTAINS(info.download_url, "/pkg-m.wubu", "migrated download_url");
    TEST_ASSERT(strlen(info.sha256) == 64, "migrated sha256");

    fixture_http_stop(srv);
    wubu_pkgmgr_shutdown();
    fixture_rm(BASE); fixture_rm(SRV);
    return true;
}

/* -- Differential SQL-injection gate ------------------------------- *
 * Strings coming from a fetched repo config, a repo index JSON or a
 * package manifest must be treated as DATA, never as SQL.  The negative
 * control first proves the payload is genuinely offensive against the OLD
 * snprintf+sqlite3_exec pattern (it really drops a table); the rest of the
 * gate proves the hardened APIs store/query the same payload literally. */

/* NEGATIVE CONTROL: reproduce the legacy db_exec() path in a scratch DB.
 * Returns true iff the injected second statement really executed. */
static bool sql_inj_negative_control(const char *evil, char *detail, size_t n) {
    sqlite3 *db = NULL;
    if (sqlite3_open(":memory:", &db) != SQLITE_OK) return false;
    bool dropped = false;
    if (sqlite3_exec(db, "CREATE TABLE repos (name TEXT PRIMARY KEY, enabled INTEGER DEFAULT 1);",
                     NULL, NULL, NULL) == SQLITE_OK) {
        char sql[512];
        snprintf(sql, sizeof(sql), "DELETE FROM repos WHERE name='%s'", evil);
        sqlite3_exec(db, sql, NULL, NULL, NULL);
        sqlite3_stmt *st = NULL;
        dropped = sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM repos", -1, &st, NULL) != SQLITE_OK;
        if (st) sqlite3_finalize(st);
        snprintf(detail, n, "legacy snprintf+sqlite3_exec with '%s' -> DROP executed: %s",
                 evil, dropped ? "YES" : "NO");
    }
    sqlite3_close(db);
    return dropped;
}

/* Direct-row helpers so the gate can plant quote-bearing untrusted values
 * without using the very interpolation under test. */
static bool sql_inj_insert_pkg(const char *base, const char *id, const char *path) {
    char dbpath[640];
    snprintf(dbpath, sizeof(dbpath), "%s/pkgmgr.db", base);
    sqlite3 *db = NULL;
    if (sqlite3_open(dbpath, &db) != SQLITE_OK) { if (db) sqlite3_close(db); return false; }
    sqlite3_stmt *st = NULL;
    bool ok = false;
    if (sqlite3_prepare_v2(db,
            "INSERT INTO packages (id, name, version, install_path, install_date) "
            "VALUES (?1, ?2, '1.0', ?3, '2026-01-01T00:00:00Z')", -1, &st, NULL) == SQLITE_OK) {
        sqlite3_bind_text(st, 1, id, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(st, 2, id, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(st, 3, path, -1, SQLITE_TRANSIENT);
        ok = sqlite3_step(st) == SQLITE_DONE;
        sqlite3_finalize(st);
    }
    sqlite3_close(db);
    return ok;
}

static bool sql_inj_insert_dep(const char *base, const char *pkg_id, const char *dep_id) {
    char dbpath[640];
    snprintf(dbpath, sizeof(dbpath), "%s/pkgmgr.db", base);
    sqlite3 *db = NULL;
    if (sqlite3_open(dbpath, &db) != SQLITE_OK) { if (db) sqlite3_close(db); return false; }
    sqlite3_stmt *st = NULL;
    bool ok = false;
    if (sqlite3_prepare_v2(db, "INSERT OR REPLACE INTO deps (pkg_id, dep_id, optional) "
                               "VALUES (?1, ?2, 0)", -1, &st, NULL) == SQLITE_OK) {
        sqlite3_bind_text(st, 1, pkg_id, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(st, 2, dep_id, -1, SQLITE_TRANSIENT);
        ok = sqlite3_step(st) == SQLITE_DONE;
        sqlite3_finalize(st);
    }
    sqlite3_close(db);
    return ok;
}

static bool sql_inj_insert_file(const char *base, const char *pkg_id, const char *rel) {
    char dbpath[640];
    snprintf(dbpath, sizeof(dbpath), "%s/pkgmgr.db", base);
    sqlite3 *db = NULL;
    if (sqlite3_open(dbpath, &db) != SQLITE_OK) { if (db) sqlite3_close(db); return false; }
    sqlite3_stmt *st = NULL;
    bool ok = false;
    if (sqlite3_prepare_v2(db, "INSERT OR REPLACE INTO files (pkg_id, path, size, sha256) "
                               "VALUES (?1, ?2, 1, '')", -1, &st, NULL) == SQLITE_OK) {
        sqlite3_bind_text(st, 1, pkg_id, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(st, 2, rel, -1, SQLITE_TRANSIENT);
        ok = sqlite3_step(st) == SQLITE_DONE;
        sqlite3_finalize(st);
    }
    sqlite3_close(db);
    return ok;
}

static bool contains_str(char **v, int n, const char *s) {
    for (int i = 0; i < n; i++) if (v && v[i] && strcmp(v[i], s) == 0) return true;
    return false;
}

bool test_untrusted_sql_literal(void) {
    const char *evil = "x'; DROP TABLE repos;--";
    char nc[256];
    TEST_ASSERT(sql_inj_negative_control(evil, nc, sizeof(nc)),
                "negative control: old interpolation really drops repos");
    printf("    [negative control] %s\n", nc);

    const char *base = "/tmp/wubu_pkgmgr_sqlinj";
    pid_t cp = fork();
    if (cp == 0) { execl("/bin/rm", "rm", "-rf", base, (char*)NULL); _exit(1); }
    waitpid(cp, NULL, 0);

    wubu_pkgmgr_config_t config;
    fixture_config(&config, base);
    TEST_ASSERT(wubu_pkgmgr_init(&config), "sql-injection gate init");

    /* (1) Repo add: quote-bearing name is stored literally, table survives. */
    TEST_ASSERT(wubu_pkgmgr_repo_add(evil, "http://example.invalid/index.json", NULL, 5),
                "repo_add accepts quote-bearing name");
    TEST_ASSERT(fixture_db_count(base, "SELECT COUNT(*) FROM repos") == 1,
                "repos table intact after repo_add");
    char namebuf[128];
    TEST_ASSERT(fixture_db_text(base, "SELECT name FROM repos", namebuf, sizeof(namebuf)),
                "repo name row readable");
    TEST_ASSERT(strcmp(namebuf, evil) == 0, "repo name stored literally");
    wubu_pkg_repo_t repos[8];
    int nr = wubu_pkgmgr_repo_list(repos, 8);
    TEST_ASSERT(nr == 1 && strcmp(repos[0].name, evil) == 0, "repo_list returns literal name");

    /* (2) Repo enable toggle: literal UPDATE, no stacked statement. */
    TEST_ASSERT(wubu_pkgmgr_repo_set_enabled(evil, false), "repo_set_enabled literal name");
    char flag[16];
    TEST_ASSERT(fixture_db_text(base, "SELECT enabled FROM repos", flag, sizeof(flag)) &&
                strcmp(flag, "0") == 0, "enabled=0 on exact row");
    TEST_ASSERT(wubu_pkgmgr_repo_set_enabled(evil, true), "repo_set_enabled re-enable");
    TEST_ASSERT(fixture_db_text(base, "SELECT enabled FROM repos", flag, sizeof(flag)) &&
                strcmp(flag, "1") == 0, "enabled=1 on exact row");
    TEST_ASSERT(fixture_db_count(base, "SELECT COUNT(*) FROM repos") == 1,
                "repos table intact after enable toggles");

    /* (3) Repo remove: DELETE is literal; repos+repo_packages both survive. */
    TEST_ASSERT(wubu_pkgmgr_repo_remove(evil), "repo_remove literal name");
    TEST_ASSERT(fixture_db_count(base, "SELECT COUNT(*) FROM repos") == 0,
                "repos table still exists, malicious row gone");
    TEST_ASSERT(fixture_db_count(base, "SELECT COUNT(*) FROM repo_packages") >= 0,
                "repo_packages table intact after repo_remove");

    /* (4) get_installed(): an injected OR predicate must not bypass WHERE. */
    const char *evil_or = "x' OR '1'='1";
    TEST_ASSERT(sql_inj_insert_pkg(base, "literal-pkg", "/nonexistent/a"), "plant benign package row");
    TEST_ASSERT(sql_inj_insert_pkg(base, evil_or, "/nonexistent/b"), "plant quote-bearing package id");
    wubu_pkg_installed_t inst;
    memset(&inst, 0, sizeof(inst));
    TEST_ASSERT(wubu_pkgmgr_get_installed(evil_or, &inst), "literal quote-bearing id found");
    TEST_ASSERT(strcmp(inst.manifest.id, evil_or) == 0, "get_installed returns exact literal id");
    TEST_ASSERT(!wubu_pkgmgr_get_installed("none' OR '1'='1", &inst),
                "injected OR predicate does not return all rows");
    TEST_ASSERT(fixture_db_count(base, "SELECT COUNT(*) FROM packages") == 2,
                "packages table intact");

    /* (5) verify_installed(): files lookup uses the literal pkg_id. */
    TEST_ASSERT(sql_inj_insert_file(base, evil_or, "missing.bin"), "plant file row for quote-bearing id");
    char **broken = NULL; int nbroken = -1;
    int vrc = wubu_pkgmgr_verify_installed(&broken, &nbroken);
    TEST_ASSERT(vrc == -1 && nbroken == 1, "verify flags only the literal broken package");
    TEST_ASSERT(contains_str(broken, nbroken, evil_or), "broken list holds literal quote-bearing id");
    for (int i = 0; i < nbroken; i++) free(broken[i]);
    free(broken);
    TEST_ASSERT(fixture_db_count(base, "SELECT COUNT(*) FROM files") == 1, "files table intact");

    /* (6) resolve_deps(): a dep_id from a manifest cannot widen the query. */
    const char *evil_dep = "dep' OR '1'='1";
    TEST_ASSERT(sql_inj_insert_dep(base, "root-pkg", evil_dep), "plant quote-bearing dep_id");
    TEST_ASSERT(sql_inj_insert_dep(base, "other-pkg", "legit-dep"), "plant decoy dep row");
    const char *root = "root-pkg";
    char **ordered = NULL; int n_ord = 0;
    TEST_ASSERT(wubu_pkgmgr_resolve_deps(&root, 1, &ordered, &n_ord) == 0, "resolve succeeds");
    TEST_ASSERT(n_ord == 2, "no synthetic dep from injected OR predicate");
    TEST_ASSERT(contains_str(ordered, n_ord, root) &&
                contains_str(ordered, n_ord, evil_dep), "dep_id returned literally");
    TEST_ASSERT(!contains_str(ordered, n_ord, "legit-dep"), "unrelated dep not pulled in by injection");
    for (int i = 0; i < n_ord; i++) free(ordered[i]);
    free(ordered);
    TEST_ASSERT(fixture_db_count(base, "SELECT COUNT(*) FROM deps") == 2, "deps table intact");

    wubu_pkgmgr_shutdown();
    pid_t rp = fork();
    if (rp == 0) { execl("/bin/rm", "rm", "-rf", base, (char*)NULL); _exit(1); }
    waitpid(rp, NULL, 0);
    return true;
}

/* Main */
int main(void) {
    printf("=== WuBuOS Package Manager Tests ===\n\n");
    
    RUN_TEST(test_default_config);
    RUN_TEST(test_lifecycle);
    RUN_TEST(test_repo_management);
    RUN_TEST(test_manifest_json);
    RUN_TEST(test_package_creation);
    RUN_TEST(test_install_remove_db);
    RUN_TEST(test_desktop_entry_generation);
    RUN_TEST(test_transaction);
    RUN_TEST(test_progress_callback);
    RUN_TEST(test_stats);
    RUN_TEST(test_autoremove);
    RUN_TEST(test_repo_update_fetch);
    RUN_TEST(test_remote_install_real);
    RUN_TEST(test_remote_install_tamper);
    RUN_TEST(test_upgrade_numeric_versions);
    RUN_TEST(test_local_install_layout);
    RUN_TEST(test_schema_migration);
    RUN_TEST(test_untrusted_sql_literal);
    
    printf("\n=== Results: %d passed, %d failed ===\n", tests_passed, tests_failed);
    return tests_failed == 0 ? 0 : 1;
}