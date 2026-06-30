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

static int unlink_cb(const char *fpath, const struct stat *sb, int typeflag, struct FTW *ftwbuf) {
    (void)sb; (void)typeflag; (void)ftwbuf;
    return unlink(fpath) == 0 ? 0 : -1;
}

static int rm_rf(const char *path) {
    if (!path) return -1;
    return nftw(path, unlink_cb, 64, FTW_DEPTH | FTW_PHYS);
}

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
static ProtonState g_proton = {0};

/* ============================================================
 * Helpers
 * ============================================================ */
static bool ensure_dir(const char *path) {
    struct stat st;
    if (stat(path, &st) == 0) return S_ISDIR(st.st_mode);
    
    /* Create parent directories recursively */
    char *copy = strdup(path);
    char *p = copy + 1;
    while (*p) {
        if (*p == '/') {
            *p = '\0';
            mkdir(copy, 0755);
            *p = '/';
        }
        p++;
    }
    mkdir(copy, 0755);
    free(copy);
    return true;
}

static uint64_t get_dir_size(const char *path) {
    uint64_t total = 0;
    DIR *d = opendir(path);
    if (!d) return 0;
    
    struct dirent *ent;
    while ((ent = readdir(d))) {
        if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0) continue;
        
        char full[PROTON_PATH_MAX];
        snprintf(full, sizeof(full), "%s/%s", path, ent->d_name);
        
        struct stat st;
        if (lstat(full, &st) == 0) {
            if (S_ISDIR(st.st_mode)) {
                total += get_dir_size(full);
            } else {
                total += (uint64_t)st.st_size;
            }
        }
    }
    closedir(d);
    return total;
}

static bool file_exists(const char *path) {
    struct stat st;
    return stat(path, &st) == 0;
}

static char *find_steam_path(void) {
    /* Check common Steam locations */
    const char *locations[] = {
        "~/.steam/steam",
        "~/.local/share/Steam",
        "/usr/share/steam",
        "/var/lib/flatpak/app/com.valvesoftware.Steam/current/active/files/steam",
        NULL
    };
    
    const char *home = getenv("HOME");
    for (int i = 0; locations[i]; i++) {
        char path[PROTON_PATH_MAX];
        if (locations[i][0] == '~' && home) {
            snprintf(path, sizeof(path), "%s%s", home, locations[i] + 1);
        } else {
            strncpy(path, locations[i], sizeof(path) - 1);
        }
        
        if (file_exists(path)) {
            char *lib_path = malloc(PROTON_PATH_MAX);
            if (lib_path) {
                /* Check for steamapps */
                char steamapps[PROTON_PATH_MAX];
                snprintf(steamapps, sizeof(steamapps), "%s/steamapps", path);
                if (file_exists(steamapps)) {
                    strcpy(lib_path, path);
                    return lib_path;
                }
            }
        }
    }
    return NULL;
}

static void parse_vdf_pairs(const char *data, size_t len, char ***keys, char ***vals, int *count) {
    /* Simple VDF parser for appmanifest files */
    /* Format: "key" "value" with nested braces */
    /* This is a simplified version - real VDF is more complex */
    *count = 0;
    *keys = NULL;
    *vals = NULL;
}

static int parse_appmanifest(const char *path, char *appid, size_t aid_len, char *name, size_t name_len, char *installdir, size_t idir_len) {
    FILE *f = fopen(path, "r");
    if (!f) return -1;
    
    char *data = NULL;
    size_t size = 0;
    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (fsize > 0) {
        data = malloc(fsize + 1);
        fread(data, 1, fsize, f);
        data[fsize] = '\0';
    }
    fclose(f);
    
    if (!data) return -1;
    
    /* Simple parsing for "appid" "12345" and "name" "Game Name" and "installdir" "folder" */
    char *p = data;
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
    
    free(data);
    return (appid[0] && name[0]) ? 0 : -1;
}

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
static int run_wine_command(const char *prefix_id, const char *const argv[], bool wait) {
    const ProtonPrefix *prefix = wubu_proton_get_prefix(prefix_id);
    if (!prefix) return -1;
    
    /* Build bubblewrap command for wine */
    const char *bwrap_argv[64];
    int argc = 0;
    
    bwrap_argv[argc++] = "bwrap";
    bwrap_argv[argc++] = "--die-with-parent";
    bwrap_argv[argc++] = "--new-session";
    
    /* Bind mounts */
    bwrap_argv[argc++] = "--ro-bind";
    bwrap_argv[argc++] = "/usr";
    bwrap_argv[argc++] = "/usr";
    
    bwrap_argv[argc++] = "--ro-bind";
    bwrap_argv[argc++] = "/lib";
    bwrap_argv[argc++] = "/lib";
    
    bwrap_argv[argc++] = "--ro-bind";
    bwrap_argv[argc++] = "/lib64";
    bwrap_argv[argc++] = "/lib64";
    
    bwrap_argv[argc++] = "--ro-bind";
    bwrap_argv[argc++] = "/bin";
    bwrap_argv[argc++] = "/bin";
    
    bwrap_argv[argc++] = "--ro-bind";
    bwrap_argv[argc++] = "/sbin";
    bwrap_argv[argc++] = "/sbin";
    
    bwrap_argv[argc++] = "--ro-bind";
    bwrap_argv[argc++] = "/etc";
    bwrap_argv[argc++] = "/etc";
    
    /* Bind wine prefix */
    bwrap_argv[argc++] = "--bind";
    bwrap_argv[argc++] = prefix->path;
    bwrap_argv[argc++] = prefix->path;
    
    /* Bind game directory if in install_path */
    if (prefix->steam_app_id[0]) {
        char game_path[PROTON_PATH_MAX];
        snprintf(game_path, sizeof(game_path), "%s/steamapps/common", g_proton.steamapps_path);
        bwrap_argv[argc++] = "--ro-bind";
        bwrap_argv[argc++] = game_path;
        bwrap_argv[argc++] = game_path;
    }
    
    /* Bind home for configs */
    const char *home = getenv("HOME");
    if (home) {
        bwrap_argv[argc++] = "--bind";
        bwrap_argv[argc++] = home;
        bwrap_argv[argc++] = home;
    }
    
    /* GPU access */
    bwrap_argv[argc++] = "--dev-bind";
    bwrap_argv[argc++] = "/dev/dri";
    bwrap_argv[argc++] = "/dev/dri";
    
    /* Environment */
    bwrap_argv[argc++] = "--setenv";
    bwrap_argv[argc++] = "WINEPREFIX";
    bwrap_argv[argc++] = prefix->path;
    
    bwrap_argv[argc++] = "--setenv";
    bwrap_argv[argc++] = "WINEDLLOVERRIDES";
    bwrap_argv[argc++] = prefix->dll_overrides[0] != '\0' ? prefix->dll_overrides : "d3d11=n,b;dxgi=n;d3d10=n,b;d3d10_1=n,b;d3d10core=n,b";
    
    if (prefix->esync) {
        bwrap_argv[argc++] = "--setenv";
        bwrap_argv[argc++] = "WINEESYNC";
        bwrap_argv[argc++] = "1";
    }
    if (prefix->fsync) {
        bwrap_argv[argc++] = "--setenv";
        bwrap_argv[argc++] = "WINEFSYNC";
        bwrap_argv[argc++] = "1";
    }
    if (prefix->dxvk_async) {
        bwrap_argv[argc++] = "--setenv";
        bwrap_argv[argc++] = "DXVK_ASYNC";
        bwrap_argv[argc++] = "1";
    }
    
    /* Proton version path */
    const char *proton_path = NULL;
    switch (prefix->proton_version) {
        case PROTON_VERSION_GE_LATEST:
            proton_path = g_proton.proton_ge_path[0] ? g_proton.proton_ge_path : "/usr/share/proton-ge";
            break;
        case PROTON_VERSION_EXPERIMENTAL:
            proton_path = "/usr/share/proton-experimental";
            break;
        default:
            proton_path = "/usr/share/proton";
            break;
    }
    
    bwrap_argv[argc++] = "--setenv";
    bwrap_argv[argc++] = "STEAM_COMPAT_DATA_PATH";
    bwrap_argv[argc++] = prefix->path;
    
    bwrap_argv[argc++] = "--setenv";
    bwrap_argv[argc++] = "PROTON_PATH";
    bwrap_argv[argc++] = proton_path;
    
    /* Wine executable */
    bwrap_argv[argc++] = "--";
    bwrap_argv[argc++] = "wine64";
    
    /* User command */
    for (int i = 0; argv[i] && argc < 60; i++) {
        bwrap_argv[argc++] = argv[i];
    }
    bwrap_argv[argc++] = NULL;

        pid_t pid = fork();
        if (pid == 0) {
            execvp("bwrap", (char *const *)bwrap_argv);
            _exit(127);
        } else if (pid > 0) {
        if (wait) {
            int status;
            waitpid(pid, &status, 0);
            return WEXITSTATUS(status);
        }
        return 0;
    }
    return -1;
}

int wubu_proton_winecmd(const char *prefix_id, const char *const argv[]) {
    return run_wine_command(prefix_id, argv, true);
}

int wubu_proton_regedit(const char *prefix_id, const char *reg_file) {
    const char *argv[] = {"regedit", reg_file, NULL};
    return wubu_proton_winecmd(prefix_id, argv);
}

int wubu_proton_winetricks(const char *prefix_id, const char *verb) {
    const char *argv[] = {"winetricks", "-q", verb, NULL};
    return wubu_proton_winecmd(prefix_id, argv);
}

int wubu_proton_launch_game(const char *game_id) {
    const ProtonGame *game = wubu_proton_get_game(game_id);
    if (!game) return -1;
    
    const ProtonPrefix *prefix = wubu_proton_get_prefix(game->prefix_id[0] ? game->prefix_id : "default");
    if (!prefix) {
        /* Create default prefix */
        wubu_proton_create_prefix("default", "Default", PROTON_VERSION_GE_LATEST);
        prefix = wubu_proton_get_prefix("default");
    }
    if (!prefix) return -1;
    
    /* Build command */
    char exe_path[PROTON_PATH_MAX];
    if (game->is_non_steam && game->custom_exe[0]) {
        strncpy(exe_path, game->custom_exe, sizeof(exe_path) - 1);
    } else {
        snprintf(exe_path, sizeof(exe_path), "%s/%s", game->install_path, game->exe_path);
    }
    
    const char *argv[32];
    int argc = 0;
    argv[argc++] = exe_path;
    
    if (game->launch_options[0]) {
        /* Parse launch options */
        char *copy = strdup(game->launch_options);
        char *tok = strtok(copy, " ");
        while (tok && argc < 30) {
            argv[argc++] = tok;
            tok = strtok(NULL, " ");
        }
        free(copy);
    }
    argv[argc++] = NULL;
    
    return run_wine_command(prefix->id, argv, false);
}

int wubu_proton_launch_with_prefix(const char *exe_path, const char *prefix_id, char *const argv[]) {
    return run_wine_command(prefix_id, argv, false);
}

/* Proton-GE handling */
int wubu_proton_ge_install_latest(void) {
    /* Would download latest Proton-GE from GitHub releases */
    /* For now, check if already installed */
    const char *paths[] = {
        "/usr/share/proton-ge",
        "/opt/proton-ge",
        NULL
    };
    
    for (int i = 0; paths[i]; i++) {
        if (file_exists(paths[i])) {
            strncpy(g_proton.proton_ge_path, paths[i], sizeof(g_proton.proton_ge_path) - 1);
            return 0;
        }
    }
    return -1;
}

int wubu_proton_ge_get_version(char *out_version, size_t size) {
    if (!g_proton.proton_ge_path[0]) return -1;
    
    char version_file[PROTON_PATH_MAX];
    snprintf(version_file, sizeof(version_file), "%s/version", g_proton.proton_ge_path);
    
    FILE *f = fopen(version_file, "r");
    if (!f) return -1;
    
    if (fgets(out_version, size, f)) {
        out_version[strcspn(out_version, "\n")] = '\0';
    }
    fclose(f);
    return 0;
}

const char *wubu_proton_ge_get_path(void) {
    return g_proton.proton_ge_path[0] ? g_proton.proton_ge_path : NULL;
}

/* DXVK/VKD3D */
int wubu_proton_dxvk_install(const char *prefix_id, DxvkMode mode) {
    if (mode == DXVK_MODE_OFF) return 0;
    
    const char *argv[] = {"winetricks", "-q", "dxvk", NULL};
    return wubu_proton_winecmd(prefix_id, argv);
}

int wubu_proton_vkd3d_install(const char *prefix_id, Vkd3dMode mode) {
    if (mode == VKD3D_MODE_OFF) return 0;
    
    const char *argv[] = {"winetricks", "-q", "vkd3d", NULL};
    return wubu_proton_winecmd(prefix_id, argv);
}

bool wubu_proton_dxvk_installed(const char *prefix_id) {
    const ProtonPrefix *p = wubu_proton_get_prefix(prefix_id);
    if (!p) return false;

    char dxvk_path[PROTON_PATH_MAX];
    snprintf(dxvk_path, sizeof(dxvk_path), "%s/drive_c/windows/system32/d3d11.dll", p->path);
    return file_exists(dxvk_path);
}

bool wubu_proton_vkd3d_installed(const char *prefix_id) {
    const ProtonPrefix *p = wubu_proton_get_prefix(prefix_id);
    if (!p) return false;

    char vkd3d_path[PROTON_PATH_MAX];
    snprintf(vkd3d_path, sizeof(vkd3d_path), "%s/drive_c/windows/system32/d3d12.dll", p->path);
    return file_exists(vkd3d_path);
}

/* ============================================================
 * DXVK Configuration
 * ============================================================ */

static const char *DEFAULT_DXVK_CONFIG =
    "[dxvk]\n"
    "d3d10 = true\n"
    "d3d10_1 = true\n"
    "d3d11 = true\n"
    "dxgi = true\n"
    "\n"
    "[nvapi]\n"
    "nvapi_hack = false\n"
    "\n"
    "[present]\n"
    "present_mode = auto\n"
    "\n"
    "[device]\n"
    "max_device_memory = 0\n"
    "max_shared_memory = 0\n"
    "\n"
    "[state_cache]\n"
    "enable = true\n"
    "path = \"${WINEPREFIX}/dxvk_state_cache\"\n";

static char *dxvk_config_path(const char *prefix_id) {
    static char path[PROTON_PATH_MAX];
    const ProtonPrefix *p = wubu_proton_get_prefix(prefix_id);
    if (!p) return NULL;
    snprintf(path, sizeof(path), "%s/dxvk.conf", p->path);
    return path;
}

int wubu_proton_dxvk_config_write(const char *prefix_id, const char *config_content) {
    const char *path = dxvk_config_path(prefix_id);
    if (!path || !config_content) return -1;

    FILE *f = fopen(path, "w");
    if (!f) return -1;

    fprintf(f, "%s", config_content);
    fclose(f);
    return 0;
}

int wubu_proton_dxvk_config_read(const char *prefix_id, char *out_config, size_t size) {
    const char *path = dxvk_config_path(prefix_id);
    if (!path || !out_config || size == 0) return -1;

    FILE *f = fopen(path, "r");
    if (!f) return -1;

    size_t read = fread(out_config, 1, size - 1, f);
    out_config[read] = '\0';
    fclose(f);
    return 0;
}

int wubu_proton_dxvk_set_hud(const char *prefix_id, bool enable, const char *options) {
    const ProtonPrefix *p = wubu_proton_get_prefix(prefix_id);
    if (!p) return -1;

    char config[4096];
    wubu_proton_dxvk_config_read(prefix_id, config, sizeof(config));
    if (!config[0]) strcpy(config, DEFAULT_DXVK_CONFIG);

    char *hud_line = strstr(config, "hud = ");
    if (enable) {
        if (hud_line) {
            /* Update existing */
            const char *rest = hud_line + 6; /* skip "hud = " */
            if (options && options[0]) {
                snprintf(hud_line, config + sizeof(config) - hud_line, "hud = %s", options);
            } else {
                snprintf(hud_line, config + sizeof(config) - hud_line, "hud = fps,devinfo,memory");
            }
        } else {
            /* Add under [hud] section or append */
            strcat(config, "\n[hud]\nhud = ");
            strcat(config, options ? options : "fps,devinfo,memory");
        }
    } else {
        if (hud_line) {
            /* Comment out */
            memmove(hud_line, hud_line + 1, strlen(hud_line));
        }
    }

    return wubu_proton_dxvk_config_write(prefix_id, config);
}

int wubu_proton_dxvk_set_async(const char *prefix_id, bool async) {
    const ProtonPrefix *p = wubu_proton_get_prefix(prefix_id);
    if (!p) return -1;

    char config[4096];
    wubu_proton_dxvk_config_read(prefix_id, config, sizeof(config));
    if (!config[0]) strcpy(config, DEFAULT_DXVK_CONFIG);

    char *async_line = strstr(config, "async = ");
    if (async) {
        if (async_line) {
            snprintf(async_line, config + sizeof(config) - async_line, "async = true");
        } else {
            strcat(config, "\nasync = true");
        }
    } else {
        if (async_line) {
            snprintf(async_line, config + sizeof(config) - async_line, "async = false");
        }
    }

    return wubu_proton_dxvk_config_write(prefix_id, config);
}

int wubu_proton_dxvk_set_nvapi_hack(const char *prefix_id, bool enable) {
    const ProtonPrefix *p = wubu_proton_get_prefix(prefix_id);
    if (!p) return -1;

    char config[4096];
    wubu_proton_dxvk_config_read(prefix_id, config, sizeof(config));
    if (!config[0]) strcpy(config, DEFAULT_DXVK_CONFIG);

    char *nvapi_line = strstr(config, "nvapi_hack = ");
    if (enable) {
        if (nvapi_line) {
            snprintf(nvapi_line, config + sizeof(config) - nvapi_line, "nvapi_hack = true");
        } else {
            strcat(config, "\n[nvapi]\nnvapi_hack = true");
        }
    } else {
        if (nvapi_line) {
            snprintf(nvapi_line, config + sizeof(config) - nvapi_line, "nvapi_hack = false");
        }
    }

    return wubu_proton_dxvk_config_write(prefix_id, config);
}

int wubu_proton_dxvk_set_present_mode(const char *prefix_id, bool mailbox) {
    const ProtonPrefix *p = wubu_proton_get_prefix(prefix_id);
    if (!p) return -1;

    char config[4096];
    wubu_proton_dxvk_config_read(prefix_id, config, sizeof(config));
    if (!config[0]) strcpy(config, DEFAULT_DXVK_CONFIG);

    char *present_line = strstr(config, "present_mode = ");
    if (mailbox) {
        if (present_line) {
            snprintf(present_line, config + sizeof(config) - present_line, "present_mode = mailbox");
        } else {
            strcat(config, "\n[present]\npresent_mode = mailbox");
        }
    } else {
        if (present_line) {
            snprintf(present_line, config + sizeof(config) - present_line, "present_mode = auto");
        }
    }

    return wubu_proton_dxvk_config_write(prefix_id, config);
}

int wubu_proton_dxvk_set_memory_limits(const char *prefix_id, int device_mb, int shared_mb) {
    const ProtonPrefix *p = wubu_proton_get_prefix(prefix_id);
    if (!p) return -1;

    char config[4096];
    wubu_proton_dxvk_config_read(prefix_id, config, sizeof(config));
    if (!config[0]) strcpy(config, DEFAULT_DXVK_CONFIG);

    char *dev_line = strstr(config, "max_device_memory = ");
    if (dev_line) {
        snprintf(dev_line, config + sizeof(config) - dev_line, "max_device_memory = %d", device_mb);
    } else {
        strcat(config, "\n[device]\nmax_device_memory = ");
        snprintf(config + strlen(config), sizeof(config) - strlen(config), "%d", device_mb);
    }

    char *shared_line = strstr(config, "max_shared_memory = ");
    if (shared_line) {
        snprintf(shared_line, config + sizeof(config) - shared_line, "max_shared_memory = %d", shared_mb);
    } else {
        strcat(config, "\nmax_shared_memory = ");
        snprintf(config + strlen(config), sizeof(config) - strlen(config), "%d", shared_mb);
    }

    return wubu_proton_dxvk_config_write(prefix_id, config);
}

int wubu_proton_dxvk_reset_config(const char *prefix_id) {
    return wubu_proton_dxvk_config_write(prefix_id, DEFAULT_DXVK_CONFIG);
}

/* ============================================================
 * DXVK Config UI
 * ============================================================ */

int wubu_proton_dxvk_config_ui_get(const char *prefix_id, DxvkConfigUI *out_ui) {
    if (!prefix_id || !out_ui) return -1;

    const ProtonPrefix *p = wubu_proton_get_prefix(prefix_id);
    if (!p) return -1;

    memset(out_ui, 0, sizeof(DxvkConfigUI));
    strncpy(out_ui->prefix_id, p->id, sizeof(out_ui->prefix_id) - 1);

    out_ui->dxvk_enabled = (p->dxvk_mode != DXVK_MODE_OFF);
    out_ui->dxvk_async = p->dxvk_async;
    out_ui->dxvk_hud_enabled = p->dxvk_hud_enabled;
    strncpy(out_ui->dxvk_hud_options, p->dxvk_hud_options, sizeof(out_ui->dxvk_hud_options) - 1);
    out_ui->dxvk_nvapi_hack = p->dxvk_nvapi_hack;
    out_ui->dxvk_present_mode_mailbox = p->dxvk_present_mode_mailbox;
    out_ui->dxvk_state_cache = p->dxvk_state_cache;
    out_ui->dxvk_max_device_memory = p->dxvk_max_device_memory;
    out_ui->dxvk_max_shared_memory = p->dxvk_max_shared_memory;
    out_ui->dxvk_d3d10 = p->dxvk_d3d10;
    out_ui->dxvk_d3d10_1 = p->dxvk_d3d10_1;

    return 0;
}

int wubu_proton_dxvk_config_ui_set(const char *prefix_id, const DxvkConfigUI *ui) {
    if (!prefix_id || !ui) return -1;

    ProtonPrefix *p = NULL;
    for (int i = 0; i < g_proton.prefix_count; i++) {
        if (strcmp(g_proton.prefixes[i].id, prefix_id) == 0) {
            p = &g_proton.prefixes[i];
            break;
        }
    }
    if (!p) return -1;

    /* Update prefix fields */
    p->dxvk_async = ui->dxvk_async;
    p->dxvk_hud_enabled = ui->dxvk_hud_enabled;
    strncpy(p->dxvk_hud_options, ui->dxvk_hud_options, sizeof(p->dxvk_hud_options) - 1);
    p->dxvk_nvapi_hack = ui->dxvk_nvapi_hack;
    p->dxvk_present_mode_mailbox = ui->dxvk_present_mode_mailbox;
    p->dxvk_state_cache = ui->dxvk_state_cache;
    p->dxvk_max_device_memory = ui->dxvk_max_device_memory;
    p->dxvk_max_shared_memory = ui->dxvk_max_shared_memory;
    p->dxvk_d3d10 = ui->dxvk_d3d10;
    p->dxvk_d3d10_1 = ui->dxvk_d3d10_1;

    /* Apply to config file */
    wubu_proton_dxvk_config_write(prefix_id, DEFAULT_DXVK_CONFIG);
    wubu_proton_dxvk_set_async(prefix_id, ui->dxvk_async);
    wubu_proton_dxvk_set_hud(prefix_id, ui->dxvk_hud_enabled, ui->dxvk_hud_options);
    wubu_proton_dxvk_set_nvapi_hack(prefix_id, ui->dxvk_nvapi_hack);
    wubu_proton_dxvk_set_present_mode(prefix_id, ui->dxvk_present_mode_mailbox);
    wubu_proton_dxvk_set_memory_limits(prefix_id, ui->dxvk_max_device_memory, ui->dxvk_max_shared_memory);

    return 0;
}

/* ============================================================
 * Config persistence
 * ============================================================ */
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