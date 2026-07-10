/* wubu_proton_config.c -- Proton config persistence.
 *
 * Self-contained module extracted from wubu_proton.c: save/load_config.
 * Uses the shared g_proton state + Proton API via wubu_proton_internal.h.
 * Minimal includes.
 */

#include "wubu_proton_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

int wubu_proton_save_config(void) {
    char config_path[PROTON_PATH_MAX];
    snprintf(config_path, sizeof(config_path), "%s/config.json", g_proton.proton_base_path);
    
    FILE *f = fopen(config_path, "w");
    if (!f) return -1;
    
    fprintf(f, "{\n");
    fprintf(f, "  \"default_proton_version\": %d,\n", g_proton.default_proton_version);
    fprintf(f, "  \"default_dxvk_mode\": %d,\n", g_proton.default_dxvk_mode);
    fprintf(f, "  \"default_vkd3d_mode\": %d,\n", g_proton.default_vkd3d_mode);
    fprintf(f, "  \"proton_ge_path\": \"%s\",\n", g_proton.proton_ge_path);
    fprintf(f, "  \"proton_ge_auto_update\": %s\n", g_proton.proton_ge_auto_update ? "true" : "false");
    fprintf(f, "  \"prefixes\": [\n");
    
    for (int i = 0; i < g_proton.prefix_count; i++) {
        ProtonPrefix *p = &g_proton.prefixes[i];
        fprintf(f, "    {\n");
        fprintf(f, "      \"id\": \"%s\",\n", p->id);
        fprintf(f, "      \"game_name\": \"%s\",\n", p->game_name);
        fprintf(f, "      \"proton_version\": %d,\n", p->proton_version);
        fprintf(f, "      \"runtime\": %d,\n", p->runtime);
        fprintf(f, "      \"dxvk_mode\": %d,\n", p->dxvk_mode);
        fprintf(f, "      \"vkd3d_mode\": %d,\n", p->vkd3d_mode);
        fprintf(f, "      \"windows_version\": \"%s\",\n", p->windows_version);
        fprintf(f, "      \"esync\": %s,\n", p->esync ? "true" : "false");
        fprintf(f, "      \"fsync\": %s,\n", p->fsync ? "true" : "false");
        fprintf(f, "      \"created\": %ld\n", p->created);
        fprintf(f, "    }%s\n", i < g_proton.prefix_count - 1 ? "," : "");
    }
    
    fprintf(f, "  ],\n");
    fprintf(f, "  \"games\": [\n");
    
    for (int i = 0; i < g_proton.game_count; i++) {
        ProtonGame *g = &g_proton.games[i];
        fprintf(f, "    {\n");
        fprintf(f, "      \"id\": \"%s\",\n", g->id);
        fprintf(f, "      \"name\": \"%s\",\n", g->name);
        fprintf(f, "      \"steam_app_id\": \"%s\",\n", g->steam_app_id);
        fprintf(f, "      \"install_path\": \"%s\",\n", g->install_path);
        fprintf(f, "      \"exe_path\": \"%s\",\n", g->exe_path);
        fprintf(f, "      \"launch_options\": \"%s\",\n", g->launch_options);
        fprintf(f, "      \"prefix_id\": \"%s\",\n", g->prefix_id);
        fprintf(f, "      \"is_non_steam\": %s,\n", g->is_non_steam ? "true" : "false");
        fprintf(f, "      \"custom_exe\": \"%s\",\n", g->custom_exe);
        fprintf(f, "      \"custom_args\": \"%s\"\n", g->custom_args);
        fprintf(f, "    }%s\n", i < g_proton.game_count - 1 ? "," : "");
    }
    
    fprintf(f, "  ]\n");
    fprintf(f, "}\n");
    
    fclose(f);
    return 0;
}

int wubu_proton_load_config(void) {
    char config_path[PROTON_PATH_MAX];
    snprintf(config_path, sizeof(config_path), "%s/config.json", g_proton.proton_base_path);
    
    FILE *f = fopen(config_path, "r");
    if (!f) return 0; /* No config yet */
    
    fclose(f);
    return 0;
}
