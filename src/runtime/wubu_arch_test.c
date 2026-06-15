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
#include "../apps/wubu_freedoom.h"

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

/* -- FreeDoom Tests ----------------------------------------------- */

static void test_doom_init(void) {
    TEST("doom_init creates launcher");
    WubuDoom *doom = wubu_doom_init(NULL);
    CHECK(doom != NULL, "doom should not be NULL");
    CHECK(doom->state == WUBU_DOOM_IDLE, "state should be IDLE");
    CHECK(doom->width == 1280, "default width should be 1280");
    CHECK(doom->height == 720, "default height should be 720");
    CHECK(doom->skill == 3, "default skill should be 3");
    CHECK(doom->sound == true, "sound should be enabled");
    CHECK(doom->music == true, "music should be enabled");
    wubu_doom_destroy(doom);
    PASS();
}

static void test_doom_init_with_root(void) {
    TEST("doom_init with custom arch root");
    WubuDoom *doom = wubu_doom_init("/var/wubu/roots/custom");
    CHECK(doom != NULL, "doom should not be NULL");
    CHECK(strcmp(doom->arch_root, "/var/wubu/roots/custom") == 0,
          "arch_root should match");
    wubu_doom_destroy(doom);
    PASS();
}

static void test_doom_config_resolution(void) {
    TEST("doom_set_resolution changes width/height");
    WubuDoom *doom = wubu_doom_init(NULL);
    wubu_doom_set_resolution(doom, 1920, 1080);
    CHECK(doom->width == 1920, "width should be 1920");
    CHECK(doom->height == 1080, "height should be 1080");
    wubu_doom_set_resolution(doom, 0, 0);
    CHECK(doom->width == 640, "zero width should default to 640");
    CHECK(doom->height == 480, "zero height should default to 480");
    wubu_doom_destroy(doom);
    PASS();
}

static void test_doom_config_fullscreen(void) {
    TEST("doom_set_fullscreen toggles mode");
    WubuDoom *doom = wubu_doom_init(NULL);
    CHECK(doom->fullscreen == false, "default is windowed");
    wubu_doom_set_fullscreen(doom, true);
    CHECK(doom->fullscreen == true, "should be fullscreen");
    wubu_doom_destroy(doom);
    PASS();
}

static void test_doom_config_wad(void) {
    TEST("doom_set_wad changes WAD selection");
    WubuDoom *doom = wubu_doom_init(NULL);
    CHECK(doom->wad == WUBU_DOOM_PHASE1, "default is Phase 1");
    wubu_doom_set_wad(doom, WUBU_DOOM_PHASE2);
    CHECK(doom->wad == WUBU_DOOM_PHASE2, "should be Phase 2");
    wubu_doom_set_wad(doom, WUBU_DOOM_DM);
    CHECK(doom->wad == WUBU_DOOM_DM, "should be FreeDM");
    wubu_doom_destroy(doom);
    PASS();
}

static void test_doom_config_skill(void) {
    TEST("doom_set_skill changes difficulty");
    WubuDoom *doom = wubu_doom_init(NULL);
    wubu_doom_set_skill(doom, 5);
    CHECK(doom->skill == 5, "skill should be NIGHTMARE");
    wubu_doom_set_skill(doom, 0);  /* Out of range  --  ignored */
    CHECK(doom->skill == 5, "out-of-range should be ignored");
    wubu_doom_set_skill(doom, 6);  /* Out of range  --  ignored */
    CHECK(doom->skill == 5, "out-of-range should be ignored");
    wubu_doom_destroy(doom);
    PASS();
}

static void test_doom_config_audio(void) {
    TEST("doom_set_audio toggles sound/music");
    WubuDoom *doom = wubu_doom_init(NULL);
    wubu_doom_set_audio(doom, false, false);
    CHECK(doom->sound == false, "sound should be off");
    CHECK(doom->music == false, "music should be off");
    wubu_doom_set_audio(doom, true, false);
    CHECK(doom->sound == true, "sound should be on");
    CHECK(doom->music == false, "music should still be off");
    wubu_doom_destroy(doom);
    PASS();
}

static void test_doom_state_idle(void) {
    TEST("doom_state returns IDLE after init");
    WubuDoom *doom = wubu_doom_init(NULL);
    WubuDoomState state = wubu_doom_state(doom);
    CHECK(state == WUBU_DOOM_IDLE, "state should be IDLE");
    wubu_doom_destroy(doom);
    PASS();
}

static void test_doom_not_installed(void) {
    TEST("doom_installed returns false for empty root");
    WubuDoom *doom = wubu_doom_init("/tmp/wubu-test-noarch-390");
    bool installed = wubu_doom_installed(doom);
    CHECK(!installed, "should not be installed");
    wubu_doom_destroy(doom);
    PASS();
}

static void test_doom_destroy_null(void) {
    TEST("doom_destroy handles NULL gracefully");
    wubu_doom_destroy(NULL);  /* Should not crash */
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

    printf("\n-- FreeDoom Launcher (Cell 391) --\n\n");

    test_doom_init();
    test_doom_init_with_root();
    test_doom_config_resolution();
    test_doom_config_fullscreen();
    test_doom_config_wad();
    test_doom_config_skill();
    test_doom_config_audio();
    test_doom_state_idle();
    test_doom_not_installed();
    test_doom_destroy_null();

    printf("\n==================================================\n");
    printf("  Results: %d/%d passed, %d failed\n",
           pass, pass + fail, fail);
    printf("==================================================\n");

    return fail > 0 ? 1 : 0;
}
