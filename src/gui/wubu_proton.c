#define _XOPEN_SOURCE 500
#include "wubu_proton.h"
#include "wubu_settings.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <glob.h>
#include <libgen.h>
#include <limits.h>
#include <pwd.h>
#include <ftw.h>

/* -- Recursive directory removal (replaces system("rm -rf")) -------- */



/* -- Safe String Macros (WUBU_SAFE_STRING) -------------------------- */

#define WUBU_STRCPY(dst, src, dst_size) \
    do { \
        if (dst_size > 0) { \
            strncpy((dst), (src), (dst_size) - 1); \
            (dst)[(dst_size) - 1] = '\0'; \
        } \
    } while (0)

#define WUBU_SNPRINTF(dst, dst_size, fmt, ...) \
    do { \
        if (dst_size > 0) { \
            snprintf((dst), (dst_size), (fmt), __VA_ARGS__); \
            (dst)[(dst_size) - 1] = '\0'; \
        } \
    } while (0)

#define WUBU_STRLCAT(dst, src, dst_size) \
    do { \
        size_t _dst_len = strlen(dst); \
        size_t _src_len = strlen(src); \
        if (_dst_len + _src_len + 1 <= dst_size) { \
            memcpy((dst) + _dst_len, (src), _src_len + 1); \
        } else if (_dst_len < dst_size) { \
            size_t _avail = (dst_size) - _dst_len - 1; \
            memcpy((dst) + _dst_len, (src), _avail); \
            (dst)[(dst_size) - 1] = '\0'; \
        } \
    } while (0)

/* ============================================================
 * Internal State
 * ============================================================ */
ProtonState g_proton = {0};

/* ============================================================
 * Helpers
 * ============================================================ */






/* ============================================================
 * Proton API
 * ============================================================ */

ProtonState *wubu_proton_state(void) {
    return &g_proton;
}

int wubu_proton_init(void) {
    memset(&g_proton, 0, sizeof(g_proton));
    
    /* Set up base paths */
    const char *home = getenv("HOME");
    if (!home) home = ".";
    
    const char *xdg_data = getenv("XDG_DATA_HOME");
    if (xdg_data) {
        snprintf(g_proton.proton_base_path, sizeof(g_proton.proton_base_path), "%s/wubu/proton", xdg_data);
    } else {
        snprintf(g_proton.proton_base_path, sizeof(g_proton.proton_base_path), "%s/.local/share/wubu/proton", home);
    }
    
    snprintf(g_proton.prefixes_path, sizeof(g_proton.prefixes_path), "%s/prefixes", g_proton.proton_base_path);
    ensure_dir(g_proton.proton_base_path);
    ensure_dir(g_proton.prefixes_path);
    
    /* Detect Steam */
    wubu_proton_detect_steam();
    
    /* Set defaults */
    g_proton.default_proton_version = PROTON_VERSION_GE_LATEST;
    g_proton.default_dxvk_mode = DXVK_MODE_AUTO;
    g_proton.default_vkd3d_mode = VKD3D_MODE_AUTO;
    
    /* Load config */
    wubu_proton_load_config();
    
    return 0;
}

void wubu_proton_shutdown(void) {
    wubu_proton_save_config();
    g_proton.prefix_count = 0;
    g_proton.game_count = 0;
}

int wubu_proton_detect_steam(void) {
    char *steam_path = find_steam_path();
    if (steam_path) {
        strncpy(g_proton.steam_path, steam_path, sizeof(g_proton.steam_path) - 1);
        snprintf(g_proton.steamapps_path, sizeof(g_proton.steamapps_path), "%s/steamapps", steam_path);
        free(steam_path);
        return 0;
    }
    return -1;
}

int wubu_proton_scan_steam_library(void) {
    if (!g_proton.steamapps_path[0]) return -1;
    
    /* Read libraryfolders.vdf */
    char libfile[PROTON_PATH_MAX];
    snprintf(libfile, sizeof(libfile), "%s/libraryfolders.vdf", g_proton.steamapps_path);
    
    /* For now, just scan steamapps/common */
    char common[PROTON_PATH_MAX];
    snprintf(common, sizeof(common), "%s/common", g_proton.steamapps_path);
    
    DIR *d = opendir(common);
    if (!d) return -1;
    
    struct dirent *ent;
    while ((ent = readdir(d))) {
        if (ent->d_name[0] == '.') continue;
        
        /* Check if it's a game folder with appmanifest */
        char manifest_path[PROTON_PATH_MAX];
        snprintf(manifest_path, sizeof(manifest_path), "%s/appmanifest_%s.acf", g_proton.steamapps_path, ent->d_name);
        /* Actually, manifests are named by appid, not folder name */
    }
    closedir(d);
    
    /* Scan all appmanifest files */
    char pattern[PROTON_PATH_MAX];
    snprintf(pattern, sizeof(pattern), "%s/appmanifest_*.acf", g_proton.steamapps_path);
    
    glob_t glob_result;
    if (glob(pattern, 0, NULL, &glob_result) == 0) {
        for (size_t i = 0; i < glob_result.gl_pathc; i++) {
            char appid[32] = {0}, name[256] = {0}, installdir[256] = {0};
            if (parse_appmanifest(glob_result.gl_pathv[i], appid, sizeof(appid), name, sizeof(name), installdir, sizeof(installdir)) == 0) {
                if (g_proton.game_count < PROTON_MAX_GAMES) {
                    ProtonGame *game = &g_proton.games[g_proton.game_count++];
                    snprintf(game->id, sizeof(game->id), "steam_%s", appid);
                    strncpy(game->name, name, sizeof(game->name) - 1);
                    strncpy(game->steam_app_id, appid, sizeof(game->steam_app_id) - 1);
                    snprintf(game->install_path, sizeof(game->install_path), "%s/common/%s", g_proton.steamapps_path, installdir);
                    game->is_non_steam = false;
                    strncpy(game->prefix_id, "default", sizeof(game->prefix_id) - 1);
                }
            }
        }
        globfree(&glob_result);
    }
    
    return g_proton.game_count;
}

int wubu_proton_scan_steam_games(void) {
    return wubu_proton_scan_steam_library();
}

int wubu_proton_create_prefix(const char *id, const char *game_name, ProtonVersion version) {
    if (!id || g_proton.prefix_count >= PROTON_MAX_PREFIXES) return -1;
    
    /* Check if already exists */
    for (int i = 0; i < g_proton.prefix_count; i++) {
        if (strcmp(g_proton.prefixes[i].id, id) == 0) return 0;
    }
    
    ProtonPrefix *p = &g_proton.prefixes[g_proton.prefix_count++];
    memset(p, 0, sizeof(ProtonPrefix));
    
    strncpy(p->id, id, sizeof(p->id) - 1);
    strncpy(p->game_name, game_name ? game_name : id, sizeof(p->game_name) - 1);
    p->proton_version = version;
    p->runtime = PROTON_RUNTIME_STEAM;
    p->dxvk_mode = g_proton.default_dxvk_mode;
    p->vkd3d_mode = g_proton.default_vkd3d_mode;
    strncpy(p->windows_version, "win10", sizeof(p->windows_version) - 1);
    p->esync = true;
    p->fsync = true;
    p->dxvk_async = true;
    p->active = true;
    p->created = time(NULL);
    p->last_used = p->created;
    
    /* Create prefix directory */
    snprintf(p->path, sizeof(p->path), "%s/%s", g_proton.prefixes_path, id);
    ensure_dir(p->path);
    
    /* Initialize wine prefix */
    const char *argv[] = {"wineboot", "-u", NULL};
    wubu_proton_winecmd(id, argv);
    
    return 0;
}

const ProtonPrefix *wubu_proton_get_prefix(const char *id) {
    for (int i = 0; i < g_proton.prefix_count; i++) {
        if (strcmp(g_proton.prefixes[i].id, id) == 0) {
            return &g_proton.prefixes[i];
        }
    }
    return NULL;
}

int wubu_proton_remove_prefix(const char *id) {
    for (int i = 0; i < g_proton.prefix_count; i++) {
        if (strcmp(g_proton.prefixes[i].id, id) == 0) {
            /* Remove directory */
            rm_rf(g_proton.prefixes[i].path);
            
            /* Remove from array */
            for (int j = i; j < g_proton.prefix_count - 1; j++) {
                g_proton.prefixes[j] = g_proton.prefixes[j + 1];
            }
            g_proton.prefix_count--;
            return 0;
        }
    }
    return -1;
}

int wubu_proton_set_default_prefix(const char *id) {
    const ProtonPrefix *p = wubu_proton_get_prefix(id);
    if (!p) return -1;
    
    /* Create/update default symlink */
    char default_path[PROTON_PATH_MAX];
    snprintf(default_path, sizeof(default_path), "%s/default", g_proton.prefixes_path);
    unlink(default_path);
    symlink(p->path, default_path);
    return 0;
}

const ProtonPrefix *wubu_proton_get_default_prefix(void) {
    char default_path[PROTON_PATH_MAX];
    snprintf(default_path, sizeof(default_path), "%s/default", g_proton.prefixes_path);
    
    struct stat st;
    if (lstat(default_path, &st) == 0) {
        char target[PROTON_PATH_MAX];
        readlink(default_path, target, sizeof(target) - 1);
        char *base = basename(target);
        return wubu_proton_get_prefix(base);
    }
    
    /* Return first prefix if no default */
    if (g_proton.prefix_count > 0) return &g_proton.prefixes[0];
    return NULL;
}

int wubu_proton_add_game(const ProtonGame *game) {
    if (!game || g_proton.game_count >= PROTON_MAX_GAMES) return -1;
    
    /* Check for duplicate */
    for (int i = 0; i < g_proton.game_count; i++) {
        if (strcmp(g_proton.games[i].id, game->id) == 0) {
            g_proton.games[i] = *game;
            return 0;
        }
    }
    
    g_proton.games[g_proton.game_count++] = *game;
    return 0;
}

const ProtonGame *wubu_proton_get_game(const char *id) {
    for (int i = 0; i < g_proton.game_count; i++) {
        if (strcmp(g_proton.games[i].id, id) == 0) {
            return &g_proton.games[i];
        }
    }
    return NULL;
}

int wubu_proton_remove_game(const char *id) {
    for (int i = 0; i < g_proton.game_count; i++) {
        if (strcmp(g_proton.games[i].id, id) == 0) {
            for (int j = i; j < g_proton.game_count - 1; j++) {
                g_proton.games[j] = g_proton.games[j + 1];
            }
            g_proton.game_count--;
            return 0;
        }
    }
    return -1;
}

int wubu_proton_list_games(ProtonGame **out_games, int *out_count) {
    if (out_games) *out_games = g_proton.games;
    if (out_count) *out_count = g_proton.game_count;
    return g_proton.game_count;
}

/* Build wine command with proper environment */






/* Proton-GE handling */



/* DXVK/VKD3D */



/* ============================================================
 * DXVK Config UI
 * ============================================================ */



/* ============================================================
 * Config persistence
 * ============================================================ */



