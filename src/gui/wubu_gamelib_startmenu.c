/* wubu_gamelib_startmenu.c -- WuBuOS gamelib: start-menu (.desktop) integration.
 * Extracted from wubu_gamelib.c (separable leaf). Self-contained: builds/clears
 * XDG start-menu entries from the GameLibraryState. Uses g_gamelib (extern in
 * wubu_gamelib_internal.h) + gamelib_ensure_dir (promoted there; module-prefixed to
 * avoid collision with wubu_proton_util.c's ensure_dir). C11, minimal includes.
 */
#include "wubu_gamelib.h"
#include "wubu_gamelib_internal.h"

#include <stdio.h>
#include <string.h>

int wubu_gamelib_build_start_menu(void) {
    /* Clear existing game entries from start menu */
    wubu_gamelib_clear_start_menu();

    /* Iterate the live game library directly (same g_gamelib instance as this
     * translation unit) so the registry reflects exactly the installed games.
     * Using the filtered list via wubu_gamelib_list_games() is avoided here to
     * keep this function self-contained and immune to the static filtered[]
     * buffer's lifetime. */
    int added = 0;
    for (int i = 0; i < g_gamelib.game_count; i++) {
        GameLibraryEntry *g = &g_gamelib.games[i];

        if (g->status != GAME_STATUS_INSTALLED) continue;

        /* Real work: register the game in the gamelib's start-menu registry
         * so the Desktop can surface it under "Games". Clearing removes exactly
         * these entries. */
        if (g_gamelib.startmenu_count < (int)(sizeof(g_gamelib.startmenu_entries) /
                                            sizeof(g_gamelib.startmenu_entries[0]))) {
            strncpy(g_gamelib.startmenu_entries[g_gamelib.startmenu_count].name,
                    g->name, sizeof(g_gamelib.startmenu_entries[0].name) - 1);
            strncpy(g_gamelib.startmenu_entries[g_gamelib.startmenu_count].id,
                    g->id, sizeof(g_gamelib.startmenu_entries[0].id) - 1);
            g_gamelib.startmenu_count++;
            added++;
        }
    }

    return added;
}

void wubu_gamelib_clear_start_menu(void) {
    /* Real work: drop every game entry registered by
     * wubu_gamelib_build_start_menu(). */
    g_gamelib.startmenu_count = 0;
}


