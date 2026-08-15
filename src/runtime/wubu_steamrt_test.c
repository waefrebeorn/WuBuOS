/*
 * wubu_steamrt_test.c -- the Steam Runtime (sniper) integration test.
 *
 * Asserts (against the real Proton launch contract):
 *   1. init + the compat path is <root>/<appid>
 *   2. build_env produces the STEAM_COMPAT_DATA_PATH + WINEPREFIX
 *      + LD_LIBRARY_PATH layout Proton's script expects
 *   3. the manifest covers the core Vulkan/GL/SDL libs
 *   4. verify() finds the missing libs (and none when covered)
 */
#include "wubu_steamrt.h"
#include "wubu_test.h"
#include <stdio.h>
#include <string.h>

/* FAIL: use wubu_test.h */

static const char *env_get(wubu_steamrt_env_t *env, int n, const char *key)
{
    for (int i = 0; i < n; i++)
        if (strcmp(env[i].key, key) == 0) return env[i].val;
    return NULL;
}

int main(void)
{
    printf("=== wubu_steamrt_test (the sniper runtime integration) ===\n");

    wubu_steamrt_init("/games/compatdata", "/usr/lib/steamrt/sniper");

    /* 1. the compat path */
    char path[600];
    if (wubu_steamrt_compat_path(1245620, path, sizeof(path)) != 0)
        FAIL("compat_path");
    if (strcmp(path, "/games/compatdata/1245620") != 0)
        FAIL("compat path = '%s'", path);
    printf("  PASS: the compat data path is <root>/<appid>\n");

    /* 2. the launch env */
    wubu_steamrt_env_t env[8];
    int n = wubu_steamrt_build_env(1245620, "/games/common/EldenRing",
                                   "/games/common/Proton-9", env, 8);
    if (n < 5) FAIL("only %d env entries", n);
    const char *v;
    v = env_get(env, n, "STEAM_COMPAT_DATA_PATH");
    if (!v || strcmp(v, "/games/compatdata/1245620") != 0)
        FAIL("STEAM_COMPAT_DATA_PATH = '%s'", v ? v : "(null)");
    v = env_get(env, n, "WINEPREFIX");
    if (!v || strcmp(v, "/games/compatdata/1245620/pfx") != 0)
        FAIL("WINEPREFIX = '%s'", v ? v : "(null)");
    v = env_get(env, n, "STEAM_COMPAT_TOOL_PATHS");
    if (!v || strcmp(v, "/games/common/Proton-9") != 0)
        FAIL("STEAM_COMPAT_TOOL_PATHS = '%s'", v ? v : "(null)");
    v = env_get(env, n, "STEAM_COMPAT_INSTALLED");
    if (!v || strcmp(v, "1") != 0) FAIL("STEAM_COMPAT_INSTALLED");
    v = env_get(env, n, "LD_LIBRARY_PATH");
    if (!v || strstr(v, "/games/common/EldenRing/lib") == NULL)
        FAIL("LD_LIBRARY_PATH lacks the game lib: '%s'", v ? v : "(null)");
    if (!v || strstr(v, "sniper") == NULL)
        FAIL("LD_LIBRARY_PATH lacks the runtime: '%s'", v ? v : "(null)");
    printf("  PASS: the Proton launch env (COMPAT_* + WINEPREFIX + LDPATH)\n");

    /* 3. the manifest covers the core stack */
    if (!wubu_steamrt_in_manifest("libvulkan1"))
        FAIL("libvulkan1 missing from the manifest");
    if (!wubu_steamrt_in_manifest("mesa-vulkan-drivers"))
        FAIL("mesa-vulkan-drivers missing");
    if (!wubu_steamrt_in_manifest("libsdl2-2.0-0"))
        FAIL("libsdl2 missing");
    if (!wubu_steamrt_in_manifest("libpipewire-0.3-0"))
        FAIL("pipewire missing");
    printf("  PASS: the manifest covers the Vulkan/GL/SDL stack\n");

    /* 4. verify: the covered libs are fine, the fake one is missing */
    const char *covered[] = { "libvulkan1", "libdrm2", "libfontconfig1", NULL };
    if (wubu_steamrt_verify(covered, 3, 0) != 0)
        FAIL("covered libs reported missing");
    const char *with_gap[] = { "libvulkan1", "libdoesnotexist9", NULL };
    int missing = wubu_steamrt_verify(with_gap, 2, 0);
    if (missing != 1)
        FAIL("missing count = %d, want 1", missing);
    printf("  PASS: verify() finds the gaps (manifest-first)\n");

    /* 5. the view */
    wubu_steamrt_view_t v2;
    wubu_steamrt_get(&v2);
    if (!v2.initialized || v2.n_manifest < 30)
        FAIL("view: init=%d manifest=%d", v2.initialized, v2.n_manifest);
    printf("  PASS: the manifest carries %d critical libs\n", v2.n_manifest);

    printf("=== ALL STEAMRT TESTS PASSED (the sniper runtime integration) ===\n");
    return 0;
}
