/* wubu_gamelib_playtime.c -- WuBuOS gamelib: playtime tracking.
 * Extracted from wubu_gamelib.c (separable leaf). Self-contained: records/queries
 * per-game playtime. Uses wubu_gamelib_get_game (public API). C11, minimal includes.
 */
#include "wubu_gamelib.h"

#include <time.h>

void wubu_gamelib_record_playtime(const char *game_id, int minutes) {
    GameLibraryEntry *game = (GameLibraryEntry*)wubu_gamelib_get_game(game_id);
    if (!game) return;
    
    game->playtime_minutes += minutes;
    game->playtime_forever += minutes;
    game->last_played = time(NULL);
}

int wubu_gamelib_get_playtime(const char *game_id) {
    const GameLibraryEntry *game = wubu_gamelib_get_game(game_id);
    return game ? game->playtime_forever : 0;
}
