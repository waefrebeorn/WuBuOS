#include "wubu_gamelib_internal.h"
#include "wubu_gamelib.h"
#include "wubu_proton.h"
#include "wubu_settings.h"
#include "wubu_theme.h"
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
#include <ctype.h>
#include <strings.h>

/* ============================================================
 * Internal State
 * ============================================================ */
GameLibraryState g_gamelib = {0};

/* ============================================================
 * Helpers
 * ============================================================ */
static void str_lower(char *s) {
    for (; *s; s++) *s = tolower(*s);
}

bool gamelib_ensure_dir(const char *path) {
    struct stat st;
    if (stat(path, &st) == 0) return S_ISDIR(st.st_mode);
    
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

static int compare_games(const void *a, const void *b) {
    const GameLibraryEntry *ga = *(const GameLibraryEntry *const *)a;
    const GameLibraryEntry *gb = *(const GameLibraryEntry *const *)b;
    GameLibraryState *state = wubu_gamelib_state();
    bool asc = state->sort_ascending;
    int mult = asc ? 1 : -1;
    
    if (strcmp(state->sort_mode, "name") == 0) {
        return mult * strcasecmp(ga->sort_name, gb->sort_name);
    } else if (strcmp(state->sort_mode, "last_played") == 0) {
        return mult * (ga->last_played > gb->last_played ? -1 : 1);
    } else if (strcmp(state->sort_mode, "playtime") == 0) {
        return mult * (ga->playtime_forever > gb->playtime_forever ? -1 : 1);
    } else if (strcmp(state->sort_mode, "size") == 0) {
        return mult * (ga->install_size > gb->install_size ? -1 : 1);
    }
    return 0;
}

static bool match_filter(const GameLibraryEntry *game, const char *filter) {
    if (!filter || !filter[0]) return true;
    
    char f[512];
    strncpy(f, filter, sizeof(f) - 1);
    f[sizeof(f) - 1] = '\0';
    str_lower(f);
    
    char combined[4096];
    snprintf(combined, sizeof(combined), "%s %s %s %s %s", 
             game->name, game->genres, game->tags, game->developer, game->publisher);
    str_lower(combined);
    
    return strstr(combined, f) != NULL;
}

/* ============================================================
 * Game Library API
 * ============================================================ */

GameLibraryState *wubu_gamelib_state(void) {
    return &g_gamelib;
}

int wubu_gamelib_init(void) {
    memset(&g_gamelib, 0, sizeof(g_gamelib));
    
    /* Set up paths */
    const char *home = getenv("HOME");
    if (!home) home = ".";
    
    const char *xdg_data = getenv("XDG_DATA_HOME");
    if (xdg_data) {
        snprintf(g_gamelib.library_path, sizeof(g_gamelib.library_path), "%s/wubu/games", xdg_data);
    } else {
        snprintf(g_gamelib.library_path, sizeof(g_gamelib.library_path), "%s/.local/share/wubu/games", home);
    }
    gamelib_ensure_dir(g_gamelib.library_path);
    
    /* Default categories */
    GameCategory cats[] = {
        {1, "All Games", "games", 0, true, ""},
        {2, "Favorites", "star", 1, true, "favorite:true"},
        {3, "Recently Played", "clock", 2, true, "last_played:7d"},
        {4, "Steam", "steam", 10, true, "source:steam"},
        {5, "Epic Games", "epic", 11, false, "source:epic"},
        {6, "GOG", "gog", 12, false, "source:gog"},
        {7, "Ubisoft Connect", "ubisoft", 13, false, "source:ubisoft"},
        {8, "EA App", "ea", 14, false, "source:ea"},
        {9, "Battle.net", "battlenet", 15, false, "source:battlenet"},
        {10, "Heroic", "heroic", 16, false, "source:heroic"},
        {11, "Lutris", "lutris", 17, false, "source:lutris"},
        {12, "Native Linux", "linux", 20, true, "native:true"},
        {13, "Windows (Proton)", "windows", 21, true, "uses_proton:true"},
        {14, "Not Installed", "download", 30, false, "status:not_installed"},
    };
    
    for (int i = 0; i < (int)(sizeof(cats)/sizeof(cats[0])); i++) {
        g_gamelib.categories[g_gamelib.category_count++] = cats[i];
    }
    
    /* Default sort */
    strcpy(g_gamelib.sort_mode, "name");
    g_gamelib.sort_ascending = true;
    g_gamelib.auto_refresh = true;
    g_gamelib.refresh_interval_minutes = 60;
    g_gamelib.show_non_steam = true;
    g_gamelib.show_hidden = false;
    
    /* Load config */
    wubu_gamelib_load_config();
    
    return 0;
}

void wubu_gamelib_shutdown(void) {
    wubu_gamelib_save_config();
    g_gamelib.game_count = 0;
    g_gamelib.category_count = 0;
}

int wubu_gamelib_add_game(const GameLibraryEntry *game) {
    if (!game || g_gamelib.game_count >= GAME_LIB_MAX_ENTRIES) return -1;
    
    /* Check for duplicate */
    for (int i = 0; i < g_gamelib.game_count; i++) {
        if (strcmp(g_gamelib.games[i].id, game->id) == 0) {
            g_gamelib.games[i] = *game;
            return 0;
        }
    }
    
    GameLibraryEntry *g = &g_gamelib.games[g_gamelib.game_count++];
    *g = *game;
    
    if (!g->sort_name[0]) {
        char *sn = make_sort_name(g->name);
        strncpy(g->sort_name, sn, sizeof(g->sort_name) - 1);
        free(sn);
    }
    
    return 0;
}

int wubu_gamelib_remove_game(const char *id) {
    for (int i = 0; i < g_gamelib.game_count; i++) {
        if (strcmp(g_gamelib.games[i].id, id) == 0) {
            for (int j = i; j < g_gamelib.game_count - 1; j++) {
                g_gamelib.games[j] = g_gamelib.games[j + 1];
            }
            g_gamelib.game_count--;
            return 0;
        }
    }
    return -1;
}

int wubu_gamelib_update_game(const GameLibraryEntry *game) {
    for (int i = 0; i < g_gamelib.game_count; i++) {
        if (strcmp(g_gamelib.games[i].id, game->id) == 0) {
            g_gamelib.games[i] = *game;
            return 0;
        }
    }
    return -1;
}

const GameLibraryEntry *wubu_gamelib_get_game(const char *id) {
    for (int i = 0; i < g_gamelib.game_count; i++) {
        if (strcmp(g_gamelib.games[i].id, id) == 0) {
            return &g_gamelib.games[i];
        }
    }
    return NULL;
}

int wubu_gamelib_list_games(GameLibraryEntry **out, int *count, const char *filter) {
    static GameLibraryEntry *filtered[GAME_LIB_MAX_ENTRIES];
    int n = 0;
    
    for (int i = 0; i < g_gamelib.game_count; i++) {
        GameLibraryEntry *g = &g_gamelib.games[i];
        
        if (g->hidden && !g_gamelib.show_hidden) continue;
        if (g->source != GAME_SOURCE_STEAM && !g_gamelib.show_non_steam) continue;
        if (g_gamelib.filter_favorites && !g->favorite) continue;
        if (g_gamelib.filter_installed && g->status != GAME_STATUS_INSTALLED) continue;
        if (g_gamelib.filter_source != GAME_SOURCE_STEAM && g->source != g_gamelib.filter_source) continue;
        if (g_gamelib.filter_genre[0] && strstr(g->genres, g_gamelib.filter_genre) == NULL) continue;
        if (g_gamelib.filter_tag[0] && strstr(g->tags, g_gamelib.filter_tag) == NULL) continue;
        if (!match_filter(g, g_gamelib.current_filter)) continue;
        
        filtered[n++] = g;
    }
    
    qsort(filtered, n, sizeof(GameLibraryEntry*), compare_games);
    
    if (out) *out = filtered;
    if (count) *count = n;
    return n;
}

int wubu_gamelib_add_category(const GameCategory *cat) {
    if (!cat || g_gamelib.category_count >= GAME_LIB_MAX_CATEGORIES) return -1;
    
    g_gamelib.categories[g_gamelib.category_count++] = *cat;
    return 0;
}

int wubu_gamelib_remove_category(int id) {
    for (int i = 0; i < g_gamelib.category_count; i++) {
        if (g_gamelib.categories[i].id == id) {
            for (int j = i; j < g_gamelib.category_count - 1; j++) {
                g_gamelib.categories[j] = g_gamelib.categories[j + 1];
            }
            g_gamelib.category_count--;
            return 0;
        }
    }
    return -1;
}

int wubu_gamelib_update_category(const GameCategory *cat) {
    for (int i = 0; i < g_gamelib.category_count; i++) {
        if (g_gamelib.categories[i].id == cat->id) {
            g_gamelib.categories[i] = *cat;
            return 0;
        }
    }
    return -1;
}

const GameCategory *wubu_gamelib_get_category(int id) {
    for (int i = 0; i < g_gamelib.category_count; i++) {
        if (g_gamelib.categories[i].id == id) {
            return &g_gamelib.categories[i];
        }
    }
    return NULL;
}

int wubu_gamelib_assign_game_to_category(const char *game_id, int cat_id) {
    const GameLibraryEntry *game = wubu_gamelib_get_game(game_id);
    if (!game) return -1;
    
    GameLibraryEntry *mut = (GameLibraryEntry*)game; /* FIXME: const */
    mut->category_id = cat_id;
    return 0;
}

/* Steam scanning */

/* Launch game */
int wubu_gamelib_launch_game(const char *game_id) {
    GameLibraryEntry *game = (GameLibraryEntry*)wubu_gamelib_get_game(game_id);
    if (!game) return -1;
    
    /* Update playtime */
    game->last_played = time(NULL);
    
    if (game->native_linux && game->native_exe[0]) {
        /* Launch native */
        char *argv[] = {(char*)game->native_exe, NULL};
        pid_t pid = fork();
        if (pid == 0) {
            execv(game->native_exe, argv);
            _exit(127);
        }
        return 0;
    }
    
    /* Launch via Proton */
    return wubu_proton_launch_game(game_id);
}

int wubu_gamelib_launch_with_proton(const char *game_id, const char *proton_version) {
    /* Would override proton version for this launch */
    return wubu_gamelib_launch_game(game_id);
}

/* Favorites */
int wubu_gamelib_toggle_favorite(const char *game_id) {
    GameLibraryEntry *game = (GameLibraryEntry*)wubu_gamelib_get_game(game_id);
    if (!game) return -1;
    
    game->favorite = !game->favorite;
    if (game->favorite) {
        game->category_id = 2; /* Favorites category */
    }
    return 0;
}

int wubu_gamelib_get_favorites(GameLibraryEntry **out, int *count) {
    static GameLibraryEntry *favs[GAME_LIB_MAX_ENTRIES];
    int n = 0;
    
    for (int i = 0; i < g_gamelib.game_count; i++) {
        if (g_gamelib.games[i].favorite) {
            favs[n++] = &g_gamelib.games[i];
        }
    }
    
    if (out) *out = favs;
    if (count) *count = n;
    return n;
}

/* Playtime tracking */
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

/* Config persistence */