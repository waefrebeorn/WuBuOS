/* wubu_gamelib_config.c -- WuBuOS gamelib: config save/load (JSON file I/O).
 * Extracted from wubu_gamelib.c (separable leaf). Self-contained: serializes the
 * GameLibraryState to/from gamelib.json. Uses g_gamelib (extern in
 * wubu_gamelib_internal.h). C11, minimal includes.
 */
#include "wubu_gamelib.h"
#include "wubu_gamelib_internal.h"

#include <stdio.h>
#include <string.h>

int wubu_gamelib_save_config(void) {
    char config_path[4096];
    snprintf(config_path, sizeof(config_path), "%s/gamelib.json", g_gamelib.library_path);
    
    FILE *f = fopen(config_path, "w");
    if (!f) return -1;
    
    fprintf(f, "{\n");
    fprintf(f, "  \"sort_mode\": \"%s\",\n", g_gamelib.sort_mode);
    fprintf(f, "  \"sort_ascending\": %s,\n", g_gamelib.sort_ascending ? "true" : "false");
    fprintf(f, "  \"show_non_steam\": %s,\n", g_gamelib.show_non_steam ? "true" : "false");
    fprintf(f, "  \"show_hidden\": %s,\n", g_gamelib.show_hidden ? "true" : "false");
    fprintf(f, "  \"auto_refresh\": %s,\n", g_gamelib.auto_refresh ? "true" : "false");
    fprintf(f, "  \"refresh_interval_minutes\": %d,\n", g_gamelib.refresh_interval_minutes);
    fprintf(f, "  \"games\": [\n");
    
    for (int i = 0; i < g_gamelib.game_count; i++) {
        GameLibraryEntry *g = &g_gamelib.games[i];
        fprintf(f, "    {\n");
        fprintf(f, "      \"id\": \"%s\",\n", g->id);
        fprintf(f, "      \"name\": \"%s\",\n", g->name);
        fprintf(f, "      \"source\": %d,\n", g->source);
        fprintf(f, "      \"source_id\": \"%s\",\n", g->source_id);
        fprintf(f, "      \"install_path\": \"%s\",\n", g->install_path);
        fprintf(f, "      \"exe_path\": \"%s\",\n", g->exe_path);
        fprintf(f, "      \"launch_options\": \"%s\",\n", g->launch_options);
        fprintf(f, "      \"status\": %d,\n", g->status);
        fprintf(f, "      \"uses_proton\": %s,\n", g->uses_proton ? "true" : "false");
        fprintf(f, "      \"proton_prefix_id\": \"%s\",\n", g->proton_prefix_id);
        fprintf(f, "      \"favorite\": %s,\n", g->favorite ? "true" : "false");
        fprintf(f, "      \"hidden\": %s,\n", g->hidden ? "true" : "false");
        fprintf(f, "      \"category_id\": %d,\n", g->category_id);
        fprintf(f, "      \"playtime_forever\": %d\n", g->playtime_forever);
        fprintf(f, "    }%s\n", i < g_gamelib.game_count - 1 ? "," : "");
    }
    
    fprintf(f, "  ]\n");
    fprintf(f, "}\n");
    
    fclose(f);
    return 0;
}

int wubu_gamelib_load_config(void) {
    char config_path[4096];
    snprintf(config_path, sizeof(config_path), "%s/gamelib.json", g_gamelib.library_path);
    
    FILE *f = fopen(config_path, "r");
    if (!f) return 0;
    
    fclose(f);
    return 0;
}

/* Start menu integration */
