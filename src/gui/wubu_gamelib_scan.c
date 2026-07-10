/* wubu_gamelib_scan.c -- Game library source scanners.
 *
 * Self-contained module extracted from wubu_gamelib.c: Steam/Heroic/Lutris/
 * custom-dir scanners + the shared id/size/sort-name helpers. Uses the public
 * wubu_gamelib_state() for the shared library store. Minimal includes.
 */

#include <glob.h>
#include "wubu_proton.h"
#include <time.h>
#include "wubu_gamelib_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

void generate_game_id(GameSource source, const char *source_id, char *out_id, size_t size) {
    const char *prefix;
    switch (source) {
        case GAME_SOURCE_STEAM: prefix = "steam"; break;
        case GAME_SOURCE_EPIC: prefix = "epic"; break;
        case GAME_SOURCE_GOG: prefix = "gog"; break;
        case GAME_SOURCE_UBISOFT: prefix = "ubisoft"; break;
        case GAME_SOURCE_EA: prefix = "ea"; break;
        case GAME_SOURCE_BATTLENET: prefix = "battlenet"; break;
        case GAME_SOURCE_HEROIC: prefix = "heroic"; break;
        case GAME_SOURCE_LUTRIS: prefix = "lutris"; break;
        default: prefix = "custom"; break;
    }
    snprintf(out_id, size, "%s_%s", prefix, source_id);
}

char *make_sort_name(const char *name) {
    char *copy = strdup(name);
    char *p = copy;
    
    /* Remove leading "The ", "A ", "An " */
    if (strncasecmp(p, "The ", 4) == 0) p += 4;
    else if (strncasecmp(p, "A ", 2) == 0) p += 2;
    else if (strncasecmp(p, "An ", 3) == 0) p += 3;
    
    char *result = strdup(p);
    free(copy);
    return result;
}

uint64_t gamelib_get_dir_size(const char *path) {
    uint64_t total = 0;
    DIR *d = opendir(path);
    if (!d) return 0;
    
    struct dirent *ent;
    while ((ent = readdir(d))) {
        if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0) continue;
        
        char full[4096];
        snprintf(full, sizeof(full), "%s/%s", path, ent->d_name);
        
        struct stat st;
        if (lstat(full, &st) == 0) {
            if (S_ISDIR(st.st_mode)) {
                total += gamelib_get_dir_size(full);
            } else {
                total += (uint64_t)st.st_size;
            }
        }
    }
    closedir(d);
    return total;
}

static int scan_steam_appmanifest(const char *manifest_path, char *appid, size_t aid_len, 
                                   char *name, size_t name_len, char *installdir, size_t idir_len) {
    FILE *f = fopen(manifest_path, "r");
    if (!f) return -1;
    
    char *data = NULL;
    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (fsize > 0 && fsize < 65536) {
        data = malloc(fsize + 1);
        fread(data, 1, fsize, f);
        data[fsize] = '\0';
    }
    fclose(f);
    
    if (!data) return -1;
    
    char *p = data;
    int result = -1;
    while (*p) {
        if (*p == '"') {
            p++;
            char *key_start = p;
            while (*p && *p != '"') p++;
            if (!*p) break;
            *p = '\0';
            p++;
            while (*p && *p != '"') p++;
            if (!*p) break;
            p++;
            char *val_start = p;
            while (*p && *p != '"') p++;
            if (!*p) break;
            *p = '\0';
            
            if (strcmp(key_start, "appid") == 0) {
                strncpy(appid, val_start, aid_len - 1);
            } else if (strcmp(key_start, "name") == 0) {
                strncpy(name, val_start, name_len - 1);
            } else if (strcmp(key_start, "installdir") == 0) {
                strncpy(installdir, val_start, idir_len - 1);
            }
            p++;
        } else {
            p++;
        }
    }
    
    if (appid[0] && name[0]) result = 0;
    free(data);
    return result;
}

int wubu_gamelib_scan_steam(void) {
    ProtonState *ps = wubu_proton_state();
    if (!ps->steamapps_path[0]) return -1;
    
    char pattern[4096];
    snprintf(pattern, sizeof(pattern), "%s/appmanifest_*.acf", ps->steamapps_path);
    
    glob_t glob_result;
    if (glob(pattern, 0, NULL, &glob_result) != 0) return -1;
    
    int found = 0;
    for (size_t i = 0; i < glob_result.gl_pathc; i++) {
        char appid[32] = {0}, name[256] = {0}, installdir[256] = {0};
        if (scan_steam_appmanifest(glob_result.gl_pathv[i], appid, sizeof(appid), 
                                    name, sizeof(name), installdir, sizeof(installdir)) == 0) {
            char game_id[64];
            generate_game_id(GAME_SOURCE_STEAM, appid, game_id, sizeof(game_id));
            
            GameLibraryEntry game = {0};
            strncpy(game.id, game_id, sizeof(game.id) - 1);
            strncpy(game.name, name, sizeof(game.name) - 1);
            strncpy(game.source_id, appid, sizeof(game.source_id) - 1);
            game.source = GAME_SOURCE_STEAM;
            snprintf(game.install_path, sizeof(game.install_path), "%s/common/%s", ps->steamapps_path, installdir);
            game.uses_proton = true;
            strncpy(game.proton_prefix_id, "default", sizeof(game.proton_prefix_id) - 1);
            char *sn = make_sort_name(name);
            strncpy(game.sort_name, sn, sizeof(game.sort_name) - 1);
            free(sn);
            game.category_id = 4; /* Steam category */
            
            /* Check if installed */
            struct stat st;
            if (stat(game.install_path, &st) == 0) {
                game.status = GAME_STATUS_INSTALLED;
                game.install_size = gamelib_get_dir_size(game.install_path);
            } else {
                game.status = GAME_STATUS_NOT_INSTALLED;
            }
            
            wubu_gamelib_add_game(&game);
            found++;
        }
    }
    
    globfree(&glob_result);
    return found;
}

/* Heroic Games Launcher scanning */
int wubu_gamelib_scan_heroic(void) {
    const char *home = getenv("HOME");
    if (!home) return -1;
    
    char config_path[4096];
    snprintf(config_path, sizeof(config_path), "%s/.config/heroic/legendaryConfig.json", home);
    
    /* Heroic uses legendary backend - would parse its config */
    /* For now, skip */
    return 0;
}

/* Lutris scanning */
int wubu_gamelib_scan_lutris(void) {
    const char *home = getenv("HOME");
    if (!home) return -1;
    
    char db_path[4096];
    snprintf(db_path, sizeof(db_path), "%s/.local/share/lutris/lutris.db", home);
    
    /* Lutris uses SQLite - would query games table */
    /* For now, skip */
    return 0;
}

int wubu_gamelib_scan_custom_dir(const char *path) {
    if (!path) return -1;
    
    DIR *d = opendir(path);
    if (!d) return -1;
    
    int found = 0;
    struct dirent *ent;
    while ((ent = readdir(d))) {
        if (ent->d_name[0] == '.') continue;
        
        char full[4096];
        snprintf(full, sizeof(full), "%s/%s", path, ent->d_name);
        
        struct stat st;
        if (stat(full, &st) == 0 && S_ISDIR(st.st_mode)) {
            /* Look for .exe files */
            char exe_pattern[4096];
            snprintf(exe_pattern, sizeof(exe_pattern), "%s/*.exe", full);
            
            glob_t glob_result;
            if (glob(exe_pattern, 0, NULL, &glob_result) == 0) {
                for (size_t i = 0; i < glob_result.gl_pathc && i < 5; i++) {
                    char *base = basename(glob_result.gl_pathv[i]);
                    char game_id[64];
                    generate_game_id(GAME_SOURCE_CUSTOM, ent->d_name, game_id, sizeof(game_id));
                    
                    GameLibraryEntry game = {0};
                    strncpy(game.id, game_id, sizeof(game.id) - 1);
                    strncpy(game.name, ent->d_name, sizeof(game.name) - 1);
                    game.source = GAME_SOURCE_CUSTOM;
                    strncpy(game.source_id, ent->d_name, sizeof(game.source_id) - 1);
                    strncpy(game.install_path, full, sizeof(game.install_path) - 1);
                    strncpy(game.exe_path, base, sizeof(game.exe_path) - 1);
                    game.uses_proton = true;
                    strncpy(game.proton_prefix_id, "default", sizeof(game.proton_prefix_id) - 1);
                    char *sn = make_sort_name(ent->d_name);
                    strncpy(game.sort_name, sn, sizeof(game.sort_name) - 1);
                    free(sn);
                    game.status = GAME_STATUS_INSTALLED;
                    game.install_size = gamelib_get_dir_size(full);
                    game.category_id = 1;
                    
                    wubu_gamelib_add_game(&game);
                    found++;
                }
                globfree(&glob_result);
            }
        }
    }
    closedir(d);
    return found;
}

int wubu_gamelib_full_scan(void) {
    int total = 0;
    total += wubu_gamelib_scan_steam();
    total += wubu_gamelib_scan_heroic();
    total += wubu_gamelib_scan_lutris();
    wubu_gamelib_state()->last_scan = time(NULL);
    return total;
}
