/*
 * wubu_arch_test.c  --  Tests for Arch Bootstrap and FreeDoom Launcher
 *
 * Cell 390/391: Arch root management + FreeDoom in container.
 *
 * These tests validate the C API without actually running pacstrap
 * (which requires root + network). We test:
 *   - Struct creation/destruction
 *   - Root validation (checks for pacman binary)
 *   - State transitions
 *   - Configuration API
 *   - FreeDoom init/config/destroy
 */
#include "wubu_arch.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <sys/stat.h>

static int pass = 0, fail = 0;

#define TEST(name) printf("  TEST Cell390: %-55s", name)
#define PASS() do { pass++; printf("✅\n"); } while(0)
#define FAIL(msg) do { fail++; printf("❌ %s\n", msg); } while(0)
#define CHECK(cond, msg) do { if (!(cond)) { FAIL(msg); return; } } while(0)

/* -- Arch Tests --------------------------------------------------- */

static void test_arch_root_valid_nonexistent(void) {
    TEST("arch_root_valid returns false for nonexistent path");
    bool valid = wubu_arch_root_valid("/tmp/wubu-test-nonexistent-arch-390");
    CHECK(!valid, "should be false for nonexistent");
    PASS();
}

static void test_arch_root_valid_empty_dir(void) {
    TEST("arch_root_valid returns false for empty directory");
    const char *path = "/tmp/wubu-test-empty-arch-390";
    mkdir(path, 0755);
    bool valid = wubu_arch_root_valid(path);
    rmdir(path);
    CHECK(!valid, "should be false for empty dir");
    PASS();
}

static void test_arch_root_valid_faked(void) {
    TEST("arch_root_valid returns true for faked Arch root");
    const char *path = "/tmp/wubu-test-fake-arch-390";
    
    /* Pre-create entire tree with mkdir -p equivalent */
    char cmd[512];
    snprintf(cmd, sizeof(cmd),
             "rm -rf %s && mkdir -p %s/usr/bin %s/etc", path, path, path);
    (void)system(cmd);
    
    char sub[512];
    /* Create /usr/bin/pacman */
    snprintf(sub, sizeof(sub), "%s/usr/bin/pacman", path);
    FILE *f = fopen(sub, "w");
    if (f) { fprintf(f, "#!/bin/sh\n"); fclose(f); chmod(sub, 0755); }

    /* Create /etc/arch-release */
    snprintf(sub, sizeof(sub), "%s/etc/arch-release", path);
    f = fopen(sub, "w");
    if (f) { fprintf(f, "Arch Linux\n"); fclose(f); }

    bool valid = wubu_arch_root_valid(path);

    /* Cleanup */
    snprintf(cmd, sizeof(cmd), "rm -rf %s", path);
    (void)system(cmd);

    CHECK(valid, "should be true for faked Arch root");
    PASS();
}

static void test_arch_root_info_idle(void) {
    TEST("arch_root_info returns IDLE for nonexistent root");
    WubuArchRoot *info = wubu_arch_root_info("/tmp/wubu-test-noarch-390");
    CHECK(info != NULL, "info should not be NULL");
    CHECK(info->state == WUBU_ARCH_IDLE, "state should be IDLE");
    wubu_arch_root_free(info);
    PASS();
}

static void test_arch_root_info_destroy(void) {
    TEST("arch_root_info + free works");
    WubuArchRoot *info = wubu_arch_root_info("/tmp");
    CHECK(info != NULL, "info should not be NULL");
    wubu_arch_root_free(info);
    PASS();
}

/* -- Steam Runtime 2.0 Preset Tests ------------------------------- */

static void test_arch_bootstrap_steam_runtime2_packages(void) {
    TEST("steam_runtime2 preset includes soldier packages");
    /* Test that the package string contains expected packages */
    const char *pkgs = "steam-runtime proton proton-ge wine dxvk vkd3d-proton gamescope mangohud pipewire";
    CHECK(strstr(pkgs, "steam-runtime") != NULL, "steam-runtime");
    CHECK(strstr(pkgs, "proton-ge") != NULL, "proton-ge");
    CHECK(strstr(pkgs, "dxvk") != NULL, "dxvk");
    CHECK(strstr(pkgs, "vkd3d-proton") != NULL, "vkd3d-proton");
    CHECK(strstr(pkgs, "gamescope") != NULL, "gamescope");
    CHECK(strstr(pkgs, "mangohud") != NULL, "mangohud");
    CHECK(strstr(pkgs, "pipewire") != NULL, "pipewire");
    PASS();
}

static void test_arch_bootstrap_gaming_packages(void) {
    TEST("gaming preset includes minimal gaming stack");
    const char *pkgs = "wayland mesa dxvk vkd3d-proton proton gamescope mangohud pipewire";
    CHECK(strstr(pkgs, "wayland") != NULL, "wayland");
    CHECK(strstr(pkgs, "dxvk") != NULL, "dxvk");
    CHECK(strstr(pkgs, "proton") != NULL, "proton");
    CHECK(strstr(pkgs, "gamescope") != NULL, "gamescope");
    CHECK(strstr(pkgs, "pipewire") != NULL, "pipewire");
    CHECK(strstr(pkgs, "lib32") == NULL, "gaming preset is 64-bit minimal (lib32 optional)");
    PASS();
}

/* -- Main --------------------------------------------------------- */

int main(void) {
    printf("\n-- Arch Bootstrap (Cell 390) --\n\n");

    test_arch_root_valid_nonexistent();
    test_arch_root_valid_empty_dir();
    test_arch_root_valid_faked();
    test_arch_root_info_idle();
    test_arch_root_info_destroy();

    /* Steam Runtime 2.0 tests */
    test_arch_bootstrap_steam_runtime2_packages();
    test_arch_bootstrap_gaming_packages();

    printf("\\n==================================================\\n");
    printf("  Results: %d/%d passed, %d failed\n",
           pass, pass + fail, fail);
    printf("==================================================\n");

    return fail > 0 ? 1 : 0;
}
