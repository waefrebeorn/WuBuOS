/*
 * wubu_pressure_vessel_test.c -- the pressure-vessel preset test.
 *
 * Asserts the SteamOS runtime-container semantics WITHOUT a display:
 *   1. the runtime lib bind + the game lib bind are registered (ro)
 *   2. the curated LD_LIBRARY_PATH puts the GAME libs FIRST, the
 *      runtime after (the game wins, the runtime fills the gaps)
 *   3. the game command is set + the surfaces (gpu, net) are on
 *   4. the describe preview is non-empty
 *   5. the teardown is clean (no crash)
 */
#include "wubu_pressure_vessel.h"
#include "wubu_host_exec.h"
#include <stdio.h>
#include <string.h>

#define FAIL(...) do { printf("  FAIL: " __VA_ARGS__); printf("\n"); return 1; } while (0)

int main(void)
{
    printf("=== wubu_pressure_vessel_test (the SteamOS runtime container) ===\n");

    const char *runtime = "/usr/lib/steamrt/sniper/lib/x86_64-linux-gnu";
    const char *game_lib = "/home/wubu/.local/share/Steam/steamapps/common/MyGame/lib";
    const char *game_exe = "/home/wubu/.local/share/Steam/steamapps/common/MyGame/MyGame";

    WubuPv *pv = wubu_pv_create("pv-test", runtime, game_lib, game_exe);
    if (!pv) FAIL("create");

    WubuCt *ct = wubu_pv_ct(pv);

    /* 1. the binds: the runtime + the game lib, both read-only */
    if (ct->n_binds != 2) FAIL("binds = %d, want 2", ct->n_binds);
    int found_runtime = 0, found_game = 0;
    for (int i = 0; i < ct->n_binds; i++) {
        if (strcmp(ct->binds[i].host, runtime) == 0) found_runtime = 1;
        if (strcmp(ct->binds[i].host, game_lib) == 0) found_game = 1;
        if (!ct->binds[i].readonly) FAIL("bind %d not read-only", i);
    }
    if (!found_runtime) FAIL("the runtime bind missing");
    if (!found_game) FAIL("the game lib bind missing");
    printf("  PASS: the runtime + game lib binds are ro (2 binds)\n");

    /* 2. the curated LD_LIBRARY_PATH: game libs FIRST */
    char desc[2048];
    wubu_pv_describe(pv, desc, sizeof(desc));
    printf("  describe: %s\n", desc);
    if (desc[0] == '\0') FAIL("empty describe");
    const char *ldp = strstr(desc, "LD_LIBRARY_PATH=");
    if (!ldp) FAIL("no LD_LIBRARY_PATH in the describe");
    ldp += strlen("LD_LIBRARY_PATH=");
    if (strncmp(ldp, game_lib, strlen(game_lib)) != 0)
        FAIL("the game lib is NOT first in the LD_LIBRARY_PATH");
    if (!strstr(ldp, runtime))
        FAIL("the runtime lib missing from the LD_LIBRARY_PATH");
    printf("  PASS: the curated LD_LIBRARY_PATH is game-first, runtime after\n");

    /* 3. the game command + the surfaces */
    if (!ct->argv[0] || strcmp(ct->argv[0], game_exe) != 0)
        FAIL("the game command not set");
    if (!ct->gpu_passthrough || !ct->net_enabled)
        FAIL("the GPU/net surfaces off");
    printf("  PASS: the game cmd + the gpu/net surfaces are on\n");

    /* 4. the set_game override */
    if (wubu_pv_set_game(pv, "/tmp/other-game") != 0) FAIL("set_game");
    if (strcmp(ct->argv[0], "/tmp/other-game") != 0)
        FAIL("set_game did not update the argv");
    printf("  PASS: wubu_pv_set_game updates the command\n");

    /* 5. the teardown is clean */
    wubu_pv_destroy(pv);
    printf("  PASS: the teardown is clean\n");

    printf("=== ALL PRESSURE-VESSEL TESTS PASSED (the SteamOS runtime container) ===\n");
    return 0;
}
