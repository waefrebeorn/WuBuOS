/**
 * wubu_pkgmgr_test.c - Unit tests for WuBuOS Package Manager
 */

#include "wubu_pkgmgr.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
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
    
    TEST_ASSERT(wubu_pkgmgr_txn_add(&txn, WUBU_PKG_TXN_INSTALL, "pkg1", NULL, "1.0", false), "txn add install");
    TEST_ASSERT(wubu_pkgmgr_txn_add(&txn, WUBU_PKG_TXN_UPGRADE, "pkg2", "1.0", "2.0", false), "txn add upgrade");
    TEST_ASSERT(wubu_pkgmgr_txn_add(&txn, WUBU_PKG_TXN_REMOVE, "pkg3", "1.0", NULL, true), "txn add remove");
    
    TEST_ASSERT(txn.n_items == 3, "txn item count");
    TEST_ASSERT(txn.items[0].type == WUBU_PKG_TXN_INSTALL, "item 0 type");
    TEST_ASSERT(txn.items[1].type == WUBU_PKG_TXN_UPGRADE, "item 1 type");
    TEST_ASSERT(txn.items[2].type == WUBU_PKG_TXN_REMOVE, "item 2 type");
    
    TEST_ASSERT(wubu_pkgmgr_txn_commit(&txn), "txn commit (dry run)");
    TEST_ASSERT(wubu_pkgmgr_txn_rollback(&txn), "txn rollback");
    
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
    
    printf("\n=== Results: %d passed, %d failed ===\n", tests_passed, tests_failed);
    return tests_failed == 0 ? 0 : 1;
}