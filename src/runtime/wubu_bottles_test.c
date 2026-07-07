/*
 * wubu_bottles_test.c  --  Tests for Bottles/Lutris Integration
 *
 * Cell 480: Bottles and Lutris compatibility via .wubu containers.
 */

#include "wubu_bottles.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>

static int pass = 0, fail = 0;
#define TEST(name) printf("  TEST Cell480: %-55s", name)
#define PASS() do { pass++; printf("✅\n"); } while(0)
#define FAIL(msg) do { fail++; printf("❌ %s\n", msg); } while(0)
#define CHECK(cond, msg) do { if (!(cond)) { FAIL(msg); return; } } while(0)

/* -- Bottle Creation Tests ---------------------------------------- */

static void test_bottle_create_wine(void) {
    TEST("bottle create wine type");
    WubuBottle *b = wubu_bottle_create("TestWine", BOTTLE_TYPE_WINE);
    CHECK(b != NULL, "bottle created");
    CHECK(strcmp(b->name, "TestWine") == 0, "name matches");
    CHECK(b->type == BOTTLE_TYPE_WINE, "type is wine");
    CHECK(b->runner == RUNNER_WINE_GE, "default runner GE");
    CHECK(strcmp(b->arch, "win64") == 0, "arch is win64");
    CHECK(b->dxvk.enabled == true, "dxvk enabled by default");
    CHECK(b->dxvk.async == true, "dxvk async by default");
    CHECK(b->gpu_passthrough == true, "gpu passthrough default");
    wubu_bottle_destroy(b);
    PASS();
}

static void test_bottle_create_proton(void) {
    TEST("bottle create proton type");
    WubuBottle *b = wubu_bottle_create("TestProton", BOTTLE_TYPE_PROTON);
    CHECK(b != NULL, "bottle created");
    CHECK(b->type == BOTTLE_TYPE_PROTON, "type is proton");
    CHECK(b->runner == RUNNER_PROTON_GE, "default runner proton GE");
    CHECK(strstr(b->runner_version, "GE-Proton") != NULL, "proton GE version");
    wubu_bottle_destroy(b);
    PASS();
}

static void test_bottle_create_lutris(void) {
    TEST("bottle create lutris type");
    WubuBottle *b = wubu_bottle_create("TestLutris", BOTTLE_TYPE_LUTRIS);
    CHECK(b != NULL, "bottle created");
    CHECK(b->type == BOTTLE_TYPE_LUTRIS, "type is lutris");
    CHECK(b->runner == RUNNER_LUTRIS_WINE, "runner lutris wine");
    wubu_bottle_destroy(b);
    PASS();
}

static void test_bottle_create_bottles(void) {
    TEST("bottle create bottles type");
    WubuBottle *b = wubu_bottle_create("TestBottles", BOTTLE_TYPE_BOTTLES);
    CHECK(b != NULL, "bottle created");
    CHECK(b->type == BOTTLE_TYPE_BOTTLES, "type is bottles");
    CHECK(b->runner == RUNNER_WINE_GE, "runner wine GE");
    wubu_bottle_destroy(b);
    PASS();
}

static void test_bottle_destroy_null(void) {
    TEST("bottle destroy null");
    wubu_bottle_destroy(NULL);  /* Should not crash */
    PASS();
}

/* -- Dependency Management Tests ---------------------------------- */

static void test_bottle_add_dep_dxvk(void) {
    TEST("bottle add dxvk dependency");
    WubuBottle *b = wubu_bottle_create("DepTest", BOTTLE_TYPE_WINE);
    int rc = wubu_bottle_add_dep(b, DEP_DXVK, NULL);
    CHECK(rc == 0, "add dep succeeds");
    CHECK(b->dep_count == 1, "one dependency");
    CHECK(b->deps[0].type == DEP_DXVK, "type is dxvk");
    CHECK(strcmp(b->deps[0].name, "dxvk") == 0, "name is dxvk");
    CHECK(b->deps[0].installed == false, "not installed yet");
    wubu_bottle_destroy(b);
    PASS();
}

static void test_bottle_add_dep_vcrun(void) {
    TEST("bottle add vcrun dependency with version");
    WubuBottle *b = wubu_bottle_create("DepTest2", BOTTLE_TYPE_WINE);
    int rc = wubu_bottle_add_dep(b, DEP_VCRUN, "2022");
    CHECK(rc == 0, "add dep succeeds");
    CHECK(b->dep_count == 1, "one dependency");
    CHECK(strcmp(b->deps[0].name, "vcrun2022") == 0, "name includes version");
    CHECK(strcmp(b->deps[0].version, "2022") == 0, "version stored");
    wubu_bottle_destroy(b);
    PASS();
}

static void test_bottle_add_multiple_deps(void) {
    TEST("bottle add multiple dependencies");
    WubuBottle *b = wubu_bottle_create("MultiDep", BOTTLE_TYPE_WINE);
    wubu_bottle_add_dep(b, DEP_DXVK, NULL);
    wubu_bottle_add_dep(b, DEP_VKD3D, NULL);
    wubu_bottle_add_dep(b, DEP_VCRUN, "2019");
    wubu_bottle_add_dep(b, DEP_CORE_FONTS, NULL);
    CHECK(b->dep_count == 4, "four dependencies");
    wubu_bottle_destroy(b);
    PASS();
}

static void test_bottle_remove_dep(void) {
    TEST("bottle remove dependency");
    WubuBottle *b = wubu_bottle_create("RemoveDep", BOTTLE_TYPE_WINE);
    wubu_bottle_add_dep(b, DEP_DXVK, NULL);
    wubu_bottle_add_dep(b, DEP_VKD3D, NULL);
    CHECK(b->dep_count == 2, "two deps added");

    int rc = wubu_bottle_remove_dep(b, DEP_DXVK);
    CHECK(rc == 0, "remove succeeds");
    CHECK(b->dep_count == 1, "one dep remaining");
    CHECK(b->deps[0].type == DEP_VKD3D, "remaining is vkd3d");

    rc = wubu_bottle_remove_dep(b, DEP_DXVK);  /* Already removed */
    CHECK(rc == -1, "remove non-existent fails");

    wubu_bottle_destroy(b);
    PASS();
}

static void test_bottle_install_deps(void) {
    TEST("bottle install dependencies (no false success)");
    WubuBottle *b = wubu_bottle_create("DepInstall", BOTTLE_TYPE_WINE);
    wubu_bottle_add_dep(b, DEP_DXVK, NULL);
    wubu_bottle_add_dep(b, DEP_VKD3D, NULL);
    CHECK(b->deps[0].installed == false, "deps start uninstalled");

    /* No prefix configured -> must fail (was a silent success) */
    CHECK(wubu_bottle_install_deps(b, NULL) == -1, "fails with no prefix");

    /* Prefix path that doesn't exist -> must fail */
    CHECK(wubu_bottle_install_deps(b, "/nonexistent/wubu/prefix") == -1,
          "fails with missing prefix dir");

    /* Real temp prefix dir -> success + deps marked installed */
    char tmpdir[512];
    snprintf(tmpdir, sizeof(tmpdir), "/tmp/wubu_bottle_deps_%d", (int)getpid());
    CHECK(mkdir(tmpdir, 0755) == 0, "create temp prefix dir");
    CHECK(wubu_bottle_install_deps(b, tmpdir) == 0, "install deps into real prefix");
    CHECK(b->deps[0].installed == true, "dxvk marked installed");
    CHECK(b->deps[1].installed == true, "vkd3d marked installed");
    CHECK(b->installed == true, "bottle marked installed");
    CHECK(strcmp(b->prefix_path, tmpdir) == 0, "prefix_path recorded on bottle");

    rmdir(tmpdir);
    wubu_bottle_destroy(b);
    PASS();
}

static void test_bottle_dep_available(void) {
    TEST("bottle dependency availability check");
    CHECK(wubu_bottle_dep_available(RUNNER_WINE_GE, DEP_DXVK) == true, "dxvk in GE");
    CHECK(wubu_bottle_dep_available(RUNNER_WINE_GE, DEP_VKD3D) == true, "vkd3d in GE");
    CHECK(wubu_bottle_dep_available(RUNNER_WINE_GE, DEP_VCRUN) == true, "vcrun in GE");
    CHECK(wubu_bottle_dep_available(RUNNER_WINE_SYSTEM, DEP_DXVK) == false, "dxvk not in system wine");
    CHECK(wubu_bottle_dep_available(RUNNER_PROTON_GE, DEP_DXVK) == true, "dxvk in proton GE");
    PASS();
}

/* -- Mount Management Tests --------------------------------------- */

static void test_bottle_add_mount(void) {
    TEST("bottle add mount");
    WubuBottle *b = wubu_bottle_create("MountTest", BOTTLE_TYPE_WINE);
    int rc = wubu_bottle_add_mount(b, "/home/user/Games", "/home/wubu/Games", true);
    CHECK(rc == 0, "add mount succeeds");
    CHECK(b->mount_count == 1, "one mount");
    CHECK(strcmp(b->mounts[0].host_path, "/home/user/Games") == 0, "host path");
    CHECK(strcmp(b->mounts[0].guest_path, "/home/wubu/Games") == 0, "guest path");
    CHECK(b->mounts[0].readonly == true, "readonly flag");
    wubu_bottle_destroy(b);
    PASS();
}

static void test_bottle_remove_mount(void) {
    TEST("bottle remove mount");
    WubuBottle *b = wubu_bottle_create("RemoveMount", BOTTLE_TYPE_WINE);
    wubu_bottle_add_mount(b, "/A", "/B", false);
    wubu_bottle_add_mount(b, "/C", "/D", false);
    CHECK(b->mount_count == 2, "two mounts");

    int rc = wubu_bottle_remove_mount(b, "/B");
    CHECK(rc == 0, "remove succeeds");
    CHECK(b->mount_count == 1, "one mount remaining");
    CHECK(strcmp(b->mounts[0].guest_path, "/D") == 0, "remaining is /D");

    rc = wubu_bottle_remove_mount(b, "/X");  /* Not exist */
    CHECK(rc == -1, "remove non-existent fails");

    wubu_bottle_destroy(b);
    PASS();
}

/* -- Environment Variable Tests ----------------------------------- */

static void test_bottle_set_env(void) {
    TEST("bottle set environment variable");
    WubuBottle *b = wubu_bottle_create("EnvTest", BOTTLE_TYPE_WINE);
    int rc = wubu_bottle_set_env(b, "CUSTOM_VAR", "custom_value");
    CHECK(rc == 0, "set env succeeds");
    CHECK(b->env_count == 1, "one env var");
    CHECK(strcmp(b->env[0].key, "CUSTOM_VAR") == 0, "key matches");
    CHECK(strcmp(b->env[0].value, "custom_value") == 0, "value matches");

    const char *val = wubu_bottle_get_env(b, "CUSTOM_VAR");
    CHECK(val != NULL, "get env returns value");
    CHECK(strcmp(val, "custom_value") == 0, "value matches");

    const char *missing = wubu_bottle_get_env(b, "MISSING");
    CHECK(missing == NULL, "missing returns NULL");

    wubu_bottle_destroy(b);
    PASS();
}

/* -- DXVK Config Tests -------------------------------------------- */

static void test_bottle_dxvk_config_defaults(void) {
    TEST("bottle dxvk config defaults");
    WubuBottle *b = wubu_bottle_create("DXVKTest", BOTTLE_TYPE_WINE);
    CHECK(b->dxvk.enabled == true, "enabled by default");
    CHECK(b->dxvk.async == true, "async by default");
    CHECK(strcmp(b->dxvk.hud, "off") == 0, "hud off by default");
    CHECK(b->dxvk.frame_rate_limit == 0, "no frame limit");
    CHECK(b->dxvk.nvapi_hack == false, "nvapi hack off");
    CHECK(b->dxvk.present_mode_mailbox == false, "mailbox off");
    wubu_bottle_destroy(b);
    PASS();
}

/* -- GameScope Config Tests --------------------------------------- */

static void test_bottle_gamescope_config_defaults(void) {
    TEST("bottle gamescope config defaults");
    WubuBottle *b = wubu_bottle_create("GSTest", BOTTLE_TYPE_WINE);
    CHECK(b->gamescope.mode == 0, "mode off by default");
    CHECK(b->gamescope.fsr == false, "fsr off");
    CHECK(b->gamescope.width == 0, "width 0");
    CHECK(b->gamescope.height == 0, "height 0");
    CHECK(strcmp(b->gamescope.filter, "fsr") == 0, "filter defaults to fsr");
    wubu_bottle_destroy(b);
    PASS();
}

/* -- Flatpak Stub Tests ------------------------------------------- */

static void test_bottle_flatpak_runtime(void) {
    TEST("bottle flatpak runtime check");
    CHECK(wubu_bottle_flatpak_runtime_available("org.freedesktop.Platform") == false, "not available in stub");
    PASS();
}

/* -- Main --------------------------------------------------------- */

int main(void) {
    printf("\n-- Bottle Creation (Cell 480) --\n\n");
    test_bottle_create_wine();
    test_bottle_create_proton();
    test_bottle_create_lutris();
    test_bottle_create_bottles();
    test_bottle_destroy_null();

    printf("\n-- Dependency Management --\n\n");
    test_bottle_add_dep_dxvk();
    test_bottle_add_dep_vcrun();
    test_bottle_add_multiple_deps();
    test_bottle_remove_dep();
    test_bottle_install_deps();
    test_bottle_dep_available();

    printf("\n-- Mount Management --\n\n");
    test_bottle_add_mount();
    test_bottle_remove_mount();

    printf("\n-- Environment Variables --\n\n");
    test_bottle_set_env();

    printf("\n-- DXVK Config --\n\n");
    test_bottle_dxvk_config_defaults();

    printf("\n-- GameScope Config --\n\n");
    test_bottle_gamescope_config_defaults();

    printf("\n-- Flatpak --\n\n");
    test_bottle_flatpak_runtime();

    printf("\n==================================================\n");
    printf("  Results: %d/%d passed, %d failed\n", pass, pass + fail, fail);
    printf("==================================================\n");
    return fail > 0 ? 1 : 0;
}
