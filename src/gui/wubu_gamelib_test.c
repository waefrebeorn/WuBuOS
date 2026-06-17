#include "wubu_gamelib.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>

int main(void) {
    printf("Testing Game Library...\n");
    wubu_gamelib_init();

    GameLibraryState *state = wubu_gamelib_state();
    printf("Library path: %s\n", state->library_path);
    printf("Categories: %d\n", state->category_count);

    for (int i = 0; i < state->category_count; i++) {
        printf("  %d: %s (filter: %s)\n", state->categories[i].id, 
               state->categories[i].name, state->categories[i].filter_query);
    }

    /* Test adding a game */
    GameLibraryEntry game = {0};
    strncpy(game.id, "steam_12345", sizeof(game.id) - 1);
    strncpy(game.name, "Test Game", sizeof(game.name) - 1);
    strncpy(game.source_id, "12345", sizeof(game.source_id) - 1);
    game.source = GAME_SOURCE_STEAM;
    strncpy(game.install_path, "/home/user/Steam/steamapps/common/TestGame", sizeof(game.install_path) - 1);
    strncpy(game.exe_path, "game.exe", sizeof(game.exe_path) - 1);
    game.uses_proton = true;
    strncpy(game.proton_prefix_id, "default", sizeof(game.proton_prefix_id) - 1);
    game.status = GAME_STATUS_INSTALLED;
    game.category_id = 4; /* Steam */
    game.favorite = false;
    game.hidden = false;
    game.playtime_forever = 120;

    int result = wubu_gamelib_add_game(&game);
    assert(result == 0);
    assert(state->game_count == 1);
    printf("Added game: %s\n", state->games[0].name);

    /* Test getting game */
    const GameLibraryEntry *g = wubu_gamelib_get_game("steam_12345");
    assert(g && strcmp(g->name, "Test Game") == 0);

    /* Test favorite toggle */
    wubu_gamelib_toggle_favorite("steam_12345");
    g = wubu_gamelib_get_game("steam_12345");
    assert(g->favorite == true);
    printf("Toggled favorite\n");

    /* Test favorites list */
    GameLibraryEntry **favs;
    int fav_count;
    wubu_gamelib_get_favorites(favs, &fav_count);
    assert(fav_count == 1);
    printf("Favorites: %d\n", fav_count);

    /* Test listing with filter */
    GameLibraryEntry **filtered;
    int filter_count;
    wubu_gamelib_list_games(filtered, &filter_count, "test");
    assert(filter_count == 1);
    printf("Filtered games: %d\n", filter_count);

    /* Test category management */
    GameCategory cat = {99, "Custom Category", "custom", 100, true, "genre:rpg"};
    wubu_gamelib_add_category(&cat);
    printf("Categories after add: %d\n", state->category_count);

    /* Test config save/load */
    wubu_gamelib_save_config();
    printf("Config saved\n");

    /* Clean up */
    wubu_gamelib_remove_game("steam_12345");
    assert(state->game_count == 0);
    printf("Game removed\n");

    wubu_gamelib_shutdown();
    printf("✅ All Game Library tests passed\n");
    return 0;
}