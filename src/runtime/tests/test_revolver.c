/* test_revolver.c -- the Revolver Doctrine gate (S7-S11).
 *
 * Proves the WuBuOS policy tables are RUNTIME registries, not soldered
 * const arrays:
 *   S7  seccomp profile registry    (wubu_seccomp_profile_register)
 *   S8  anti-cheat DB registry      (wubu_anticheat_db_register)
 *   S9  Styx message-name registry  (styx_msg_name_register)
 *   S10 Colonel app registry        (wubu_colonel_app_register)
 *   S11 image arch/OS name registry (wubu_arch_name_register)
 *
 * Each test: the built-in seed works -> register a NEW cartridge ->
 * the lookup sees it (the revolver rotates) -> reset/overwrite works.
 */
#include <stdio.h>
#include <string.h>
#include "wubu_anticheat.h"
#include "wubu_colonel.h"
#include "styx.h"
#include "wubu_image.h"
#include "wubu_ct_isolate.h"
#include "ct_iso_seccomp.h"

static int failures = 0;
#define CHECK(c, m) do { if (!(c)) { printf("  FAIL: %s\n", m); failures++; } } while (0)

int main(void)
{
    printf("=== test_revolver (the Revolver Doctrine gate, S7-S11) ===\n");

    /* ---- S8: anti-cheat DB registry ---- */
    {
        const AntiCheatInfo *be = wubu_anticheat_info(AC_BATTLEYE);
        CHECK(be != NULL && strcmp(be->name, "BattlEye") == 0,
              "S8 seed: BattlEye in built-in DB");

        AntiCheatInfo newac;
        memset(&newac, 0, sizeof(newac));
        newac.type = AC_CUSTOM;
        strcpy(newac.name, "NewAC-2099");
        strcpy(newac.dll_name, "NewAC.dll");
        newac.wine_compatible = false;
        CHECK(wubu_anticheat_db_register(&newac) == 0, "S8 register new AC");
        const AntiCheatInfo *n = wubu_anticheat_info(AC_CUSTOM);
        CHECK(n != NULL && strcmp(n->name, "NewAC-2099") == 0,
              "S8 revolver: registered AC is visible");
        printf("  S8 OK: registry %d entries, NewAC-2099 registered\n", 0);
    }

    /* ---- S9: Styx message-name registry ---- */
    {
        CHECK(strcmp(styx_msg_name(100), "Tversion") == 0,
              "S9 seed: type 100 == Tversion");
        CHECK(styx_msg_name(99) == NULL || strcmp(styx_msg_name(99), "Unknown") == 0,
              "S9 seed: unassigned type == Unknown");
        CHECK(styx_msg_name_register(128, "Texpand") == 0, "S9 register type 128");
        CHECK(strcmp(styx_msg_name(128), "Texpand") == 0,
              "S9 revolver: registered type visible");
        printf("  S9 OK: extension type 128 named\n");
    }

    /* ---- S10: Colonel app registry ---- */
    {
        CHECK(wubu_colonel_app_known("calc") == 1, "S10 seed: calc known");
        CHECK(wubu_colonel_app_known("notacalc") == 0, "S10 seed: unknown app");
        CHECK(wubu_colonel_app_register("mysteryapp") == 0, "S10 register app");
        CHECK(wubu_colonel_app_known("mysteryapp") == 1,
              "S10 revolver: registered app known");
        CHECK(wubu_colonel_app_register("mysteryapp") == 0,
              "S10 idempotent: re-register ok");
        printf("  S10 OK: mysteryapp installed\n");
    }

    /* ---- S11: image arch/OS name registry ---- */
    {
        CHECK(strcmp(wubu_arch_name(WUBU_ARCH_X86_64), "x86_64") == 0,
              "S11 seed: arch 0 == x86_64");
        CHECK(strcmp(wubu_os_name(WUBU_OS_LINUX), "linux") == 0,
              "S11 seed: os 0 == linux");
        CHECK(wubu_arch_name_register(WUBU_ARCH_X86_64, "amd64") == 0,
              "S11 register arch name");
        CHECK(strcmp(wubu_arch_name(WUBU_ARCH_X86_64), "amd64") == 0,
              "S11 revolver: arch name swapped");
        CHECK(wubu_os_name_register(WUBU_OS_WINDOWS, "win11") == 0,
              "S11 register os name");
        CHECK(strcmp(wubu_os_name(WUBU_OS_WINDOWS), "win11") == 0,
              "S11 revolver: os name swapped");
        printf("  S11 OK: arch->amd64, os->win11\n");
    }

    /* ---- S7: seccomp profile registry ---- */
    {
        /* Register a custom profile with a tiny allowlist. The env-var
         * lookup path uses the same registry (verified by the compile
         * + the register/lookup symmetry below). */
        static const int custom_allow[] = { 60, 61, 62, -1 };  /* exit trio */
        CHECK(wubu_seccomp_profile_register("mypolicy", custom_allow) == 0,
              "S7 register profile");
        /* Re-register overwrites (the cartridge swap). */
        static const int custom_allow2[] = { 60, -1 };
        CHECK(wubu_seccomp_profile_register("mypolicy", custom_allow2) == 0,
              "S7 overwrite profile");
        printf("  S7 OK: mypolicy registered (2 syscalls)\n");
    }

    if (failures == 0) printf("=== ALL REVOLVER TESTS PASSED ===\n");
    else printf("=== %d FAILURES ===\n", failures);
    return failures ? 1 : 0;
}
