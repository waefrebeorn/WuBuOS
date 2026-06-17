#include "wubu_proton.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>

int main(void) {
    printf("Testing Proton subsystem...\n");
    wubu_proton_init();

    ProtonState *state = wubu_proton_state();
    printf("Proton base: %s\n", state->proton_base_path);
    printf("Prefixes: %s\n", state->prefixes_path);

    /* Test Steam detection */
    int result = wubu_proton_detect_steam();
    printf("Steam detected: %s\n", result == 0 ? "yes" : "no");
    if (result == 0) {
        printf("Steam path: %s\n", state->steam_path);
        printf("Steamapps: %s\n", state->steamapps_path);
    }

    /* Test prefix creation */
    result = wubu_proton_create_prefix("test_prefix", "Test Game", PROTON_VERSION_GE_LATEST);
    assert(result == 0);
    assert(state->prefix_count == 1);
    
    const ProtonPrefix *prefix = wubu_proton_get_prefix("test_prefix");
    assert(prefix);
    printf("Created prefix: %s at %s\n", prefix->id, prefix->path);

    /* Test default prefix */
    wubu_proton_set_default_prefix("test_prefix");
    const ProtonPrefix *def = wubu_proton_get_default_prefix();
    assert(def && strcmp(def->id, "test_prefix") == 0);
    printf("Default prefix set\n");

    /* Test game addition */
    ProtonGame game = {0};
    strncpy(game.id, "test_game", sizeof(game.id) - 1);
    strncpy(game.name, "Test Game", sizeof(game.name) - 1);
    strncpy(game.steam_app_id, "12345", sizeof(game.steam_app_id) - 1);
    strncpy(game.exe_path, "game.exe", sizeof(game.exe_path) - 1);
    strncpy(game.prefix_id, "test_prefix", sizeof(game.prefix_id) - 1);
    game.is_non_steam = false;
    
    result = wubu_proton_add_game(&game);
    assert(result == 0);
    assert(state->game_count == 1);
    printf("Added game: %s\n", state->games[0].name);

    /* Test Proton-GE */
    result = wubu_proton_ge_install_latest();
    if (result == 0) {
        char ver[64];
        wubu_proton_ge_get_version(ver, sizeof(ver));
        printf("Proton-GE: %s at %s\n", ver, wubu_proton_ge_get_path());
    } else {
        printf("Proton-GE not installed\n");
    }

    /* Test config save/load */
    wubu_proton_save_config();
    printf("Config saved\n");

    /* Clean up */
    wubu_proton_remove_prefix("test_prefix");
    assert(state->prefix_count == 0);
    printf("Prefix removed\n");

    wubu_proton_shutdown();
    printf("✅ All Proton tests passed\n");
    return 0;
}