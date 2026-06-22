/*
 * wubu_anticheat_test.c  --  Tests for Anti-Cheat Module
 *
 * Cell 470: Anti-cheat research and stubs.
 */

#include "wubu_anticheat.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int pass = 0, fail = 0;
#define TEST(name) printf("  TEST Cell470: %-55s", name)
#define PASS() do { pass++; printf("✅\n"); } while(0)
#define FAIL(msg) do { fail++; printf("❌ %s\n", msg); } while(0)
#define CHECK(cond, msg) do { if (!(cond)) { FAIL(msg); return; } } while(0)

/* -- Database Tests ----------------------------------------------- */

static void test_ac_info_none(void) {
    TEST("anticheat info for NONE returns NULL");
    const AntiCheatInfo *info = wubu_anticheat_info(AC_NONE);
    CHECK(info == NULL, "NONE should return NULL");
    PASS();
}

static void test_ac_info_battleye(void) {
    TEST("anticheat info for BattlEye");
    const AntiCheatInfo *info = wubu_anticheat_info(AC_BATTLEYE);
    CHECK(info != NULL, "info should exist");
    CHECK(info->type == AC_BATTLEYE, "type should be BATTLEYE");
    CHECK(strcmp(info->name, "BattlEye") == 0, "name should be BattlEye");
    CHECK(info->has_kernel_component == true, "has kernel component");
    CHECK(info->wine_compatible == false, "not wine compatible");
    PASS();
}

static void test_ac_info_eac(void) {
    TEST("anticheat info for EasyAntiCheat");
    const AntiCheatInfo *info = wubu_anticheat_info(AC_EASYANTICHEAT);
    CHECK(info != NULL, "info should exist");
    CHECK(info->type == AC_EASYANTICHEAT, "type should be EAC");
    CHECK(info->wine_compatible == true, "is wine compatible");
    CHECK(info->has_kernel_component == true, "has kernel component");
    PASS();
}

static void test_ac_info_vanguard(void) {
    TEST("anticheat info for Vanguard");
    const AntiCheatInfo *info = wubu_anticheat_info(AC_VANGUARD);
    CHECK(info != NULL, "info should exist");
    CHECK(info->wine_compatible == false, "not wine compatible");
    CHECK(info->has_kernel_component == true, "has kernel component");
    PASS();
}

static void test_ac_info_fairfight(void) {
    TEST("anticheat info for FairFight");
    const AntiCheatInfo *info = wubu_anticheat_info(AC_FAIRFIGHT);
    CHECK(info != NULL, "info should exist");
    CHECK(info->wine_compatible == true, "wine compatible (server-side)");
    CHECK(info->has_kernel_component == false, "no kernel component");
    PASS();
}

static void test_ac_info_vmprotect(void) {
    TEST("anticheat info for VMProtect");
    const AntiCheatInfo *info = wubu_anticheat_info(AC_VMPROTECT);
    CHECK(info != NULL, "info should exist");
    CHECK(info->wine_compatible == true, "wine compatible");
    CHECK(info->has_kernel_component == false, "no kernel component");
    PASS();
}

static void test_ac_info_denuvo(void) {
    TEST("anticheat info for Denuvo");
    const AntiCheatInfo *info = wubu_anticheat_info(AC_DENUVO);
    CHECK(info != NULL, "info should exist");
    CHECK(info->wine_compatible == true, "wine compatible");
    PASS();
}

static void test_ac_info_invalid(void) {
    TEST("anticheat info for invalid type");
    const AntiCheatInfo *info = wubu_anticheat_info((AntiCheatType)999);
    CHECK(info == NULL, "should return NULL for invalid type");
    PASS();
}

/* -- Compatibility Check ------------------------------------------ */

static void test_ac_check_compat(void) {
    TEST("anticheat compatibility check");
    CHECK(wubu_anticheat_check_compatibility(AC_EASYANTICHEAT) == true, "EAC compatible");
    CHECK(wubu_anticheat_check_compatibility(AC_FAIRFIGHT) == true, "FairFight compatible");
    CHECK(wubu_anticheat_check_compatibility(AC_VMPROTECT) == true, "VMProtect compatible");
    CHECK(wubu_anticheat_check_compatibility(AC_DENUVO) == true, "Denuvo compatible");
    CHECK(wubu_anticheat_check_compatibility(AC_BATTLEYE) == false, "BattlEye NOT compatible");
    CHECK(wubu_anticheat_check_compatibility(AC_VANGUARD) == false, "Vanguard NOT compatible");
    PASS();
}

/* -- Hook Registration -------------------------------------------- */

static int hook_called = 0;
static void *hook_ctx = NULL;

static int test_hook_fn(void *ctx, const char *dll, const char *func, void *args) {
    (void)dll; (void)func; (void)args;
    hook_called++;
    hook_ctx = ctx;
    return 0;
}

static void test_ac_hook_register(void) {
    TEST("anticheat hook register");
    hook_called = 0;
    hook_ctx = NULL;

    int rc = wubu_anticheat_hook_register(AC_HOOK_DEBUGGER_DETECT, test_hook_fn, (void*)0xDEADBEEF);
    CHECK(rc == 0, "register should succeed");

    /* Call hook manually */
    if (test_hook_fn((void*)0xDEADBEEF, "kernel32.dll", "IsDebuggerPresent", NULL) == 0) {
        CHECK(hook_called == 1, "hook should be called once");
        CHECK(hook_ctx == (void*)0xDEADBEEF, "context should pass through");
    }

    /* Unregister */
    rc = wubu_anticheat_hook_unregister(AC_HOOK_DEBUGGER_DETECT);
    CHECK(rc == 0, "unregister should succeed");
    PASS();
}

static void test_ac_hook_invalid_type(void) {
    TEST("anticheat hook invalid type");
    int rc = wubu_anticheat_hook_register((AntiCheatHookType)99, test_hook_fn, NULL);
    CHECK(rc == -1, "should fail for invalid type");
    PASS();
}

/* -- Wine Config -------------------------------------------------- */

static void test_ac_wine_config(void) {
    TEST("anticheat wine config");
    int rc = wubu_anticheat_wine_config(WINE_AC_DISABLE_DEBUGGER_DETECTION | WINE_AC_SPOOF_HARDWARE_IDS);
    CHECK(rc == 0, "config should succeed");
    CHECK(getenv("WINE_TIMING_SPOOF") == NULL, "timing not set");
    CHECK(getenv("WINE_HWID_SPOOF") != NULL, "hwid spoof set");
    PASS();
}

/* -- Proton Config ------------------------------------------------ */

static void test_ac_proton_config(void) {
    TEST("anticheat proton config");
    const AntiCheatProtonConfig *cfg = wubu_anticheat_recommended_config(AC_EASYANTICHEAT);
    CHECK(cfg != NULL, "config should exist");
    CHECK(cfg->type == AC_EASYANTICHEAT, "type matches");
    CHECK(cfg->enable_proton_hook == true, "proton hook enabled");
    CHECK(cfg->disable_driver_check == true, "driver check disabled");

    cfg = wubu_anticheat_recommended_config(AC_BATTLEYE);
    CHECK(cfg != NULL, "config should exist");
    CHECK(cfg->enable_proton_hook == false, "proton hook disabled for BE");

    cfg = wubu_anticheat_recommended_config(AC_VANGUARD);
    CHECK(cfg != NULL, "config should exist");
    CHECK(cfg->enable_proton_hook == false, "proton hook disabled for Vanguard");

    /* Invalid type */
    cfg = wubu_anticheat_recommended_config((AntiCheatType)999);
    CHECK(cfg == NULL, "should return NULL for invalid type");
    PASS();
}

/* -- Kernel Stubs ------------------------------------------------- */

static void test_ac_kernel_stubs(void) {
    TEST("anticheat kernel stubs (hosted mode)");
    int rc = wubu_anticheat_kernel_load("/path/to/driver.ko", "ac_device");
    CHECK(rc == -1, "kernel_load should fail in hosted mode");

    rc = wubu_anticheat_kernel_unload("ac_device");
    CHECK(rc == -1, "kernel_unload should fail in hosted mode");

    CHECK(wubu_anticheat_kernel_loaded("ac_device") == false, "not loaded");
    PASS();
}

/* -- Main --------------------------------------------------------- */

int main(void) {
    printf("\n-- Anti-Cheat Database (Cell 470) --\n\n");
    test_ac_info_none();
    test_ac_info_battleye();
    test_ac_info_eac();
    test_ac_info_vanguard();
    test_ac_info_fairfight();
    test_ac_info_vmprotect();
    test_ac_info_denuvo();
    test_ac_info_invalid();

    printf("\n-- Compatibility Check --\n\n");
    test_ac_check_compat();

    printf("\n-- Hook Registration --\n\n");
    test_ac_hook_register();
    test_ac_hook_invalid_type();

    printf("\n-- Wine Config --\n\n");
    test_ac_wine_config();

    printf("\n-- Proton Config --\n\n");
    test_ac_proton_config();

    printf("\n-- Kernel Stubs --\n\n");
    test_ac_kernel_stubs();

    printf("\n==================================================\n");
    printf("  Results: %d/%d passed, %d failed\n", pass, pass + fail, fail);
    printf("==================================================\n");
    return fail > 0 ? 1 : 0;
}
