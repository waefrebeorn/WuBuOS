#ifndef WUBU_GAME_LIBRARY_H
#define WUBU_GAME_LIBRARY_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <time.h>

/* -- Game Library Constants ----------------------------------------- */

#define GAME_LIB_MAX_ENTRIES 512
#define GAME_LIB_MAX_CATEGORIES 32

typedef enum {
    GAME_SOURCE_STEAM = 0,
    GAME_SOURCE_EPIC = 1,
    GAME_SOURCE_GOG = 2,
    GAME_SOURCE_UBISOFT = 3,
    GAME_SOURCE_EA = 4,
    GAME_SOURCE_BATTLENET = 5,
    GAME_SOURCE_HEROIC = 6,
    GAME_SOURCE_LUTRIS = 7,
    GAME_SOURCE_CUSTOM = 255
} GameSource;

typedef enum {
    GAME_STATUS_INSTALLED = 0,
    GAME_STATUS_UPDATING = 1,
    GAME_STATUS_DOWNLOADING = 2,
    GAME_STATUS_NOT_INSTALLED = 3,
    GAME_STATUS_ERROR = 4
} GameStatus;

/* -- Game Library Entry --------------------------------------------- */

typedef struct {
    char id[64];                      /* Unique library entry ID */
    char name[256];                   /* Display name */
    char sort_name[256];              /* For sorting (no "The ", etc.) */
    GameSource source;
    char source_id[64];               /* Platform-specific ID (Steam AppID, etc.) */
    
    /* Installation */
    char install_path[4096];
    char exe_path[4096];              /* Relative to install_path */
    char launch_options[512];
    GameStatus status;
    uint64_t install_size;
    time_t install_time;
    time_t last_played;
    int playtime_minutes;
    int playtime_forever;
    
    /* Metadata */
    char developer[256];
    char publisher[256];
    char release_date[32];
    char genres[512];                 /* Comma-separated */
    char tags[512];                   /* User tags */
    char description[2048];
    char header_image[512];           /* Steam header image URL/path */
    char capsule_image[512];          /* Steam capsule image */
    char background_image[512];
    
    /* Proton/Wine */
    char proton_prefix_id[64];
    char proton_version[32];
    char dxvk_version[32];
    char vkd3d_version[32];
    bool uses_proton;
    char wine_overrides[1024];
    
    /* Runtime */
    bool native_linux;                /* Native Linux build available */
    char native_exe[4096];
    
    /* UI */
    int category_id;                  /* Category for Start menu */
    bool favorite;                    /* Pinned to top */
    bool hidden;                      /* Hidden from library view */
    
    /* Controller */
    bool controller_support;          /* Full/partial/none */
    char controller_hints[256];
    
    /* Cloud */
    bool cloud_sync;
    char cloud_path[4096];
} GameLibraryEntry;

/* -- Category ------------------------------------------------------- */

typedef struct {
    int id;
    char name[64];
    char icon[64];
    int sort_order;
    bool expanded;
    char filter_query[256];           /* Auto-filter: "source:steam genre:rpg" */
} GameCategory;

/* -- Game Library State --------------------------------------------- */

typedef struct {
    GameLibraryEntry games[GAME_LIB_MAX_ENTRIES];
    int game_count;
    
    GameCategory categories[GAME_LIB_MAX_CATEGORIES];
    int category_count;
    
    /* Settings */
    char library_path[4096];          /* ~/.local/share/wubu/games */
    bool auto_refresh;
    int refresh_interval_minutes;
    bool show_non_steam;
    bool show_hidden;
    char sort_mode[32];               /* "name", "last_played", "playtime", "size" */
    bool sort_ascending;
    
    /* Filters */
    char current_filter[512];
    GameSource filter_source;
    char filter_genre[64];
    char filter_tag[64];
    bool filter_favorites;
    bool filter_installed;
    
    /* Scanning */
    time_t last_scan;
    bool scanning;

    /* Start-menu integration registry (real: populated by
     * wubu_gamelib_build_start_menu, cleared by wubu_gamelib_clear_start_menu).
     * Self-contained so the gamelib needs no link dependency on the start-menu
     * GUI module. */
    struct {
        char name[48];
        char id[128];
    } startmenu_entries[GAME_LIB_MAX_CATEGORIES * 8];
    int startmenu_count;
} GameLibraryState;

/* -- Game Library API ----------------------------------------------- */

/* Initialize game library */
int  wubu_gamelib_init(void);
void wubu_gamelib_shutdown(void);

/* Get state */
GameLibraryState *wubu_gamelib_state(void);

/* Game management */
int wubu_gamelib_add_game(const GameLibraryEntry *game);
int wubu_gamelib_remove_game(const char *id);
int wubu_gamelib_update_game(const GameLibraryEntry *game);
const GameLibraryEntry *wubu_gamelib_get_game(const char *id);
int wubu_gamelib_list_games(GameLibraryEntry **out, int *count, const char *filter);

/* Category management */
int wubu_gamelib_add_category(const GameCategory *cat);
int wubu_gamelib_remove_category(int id);
int wubu_gamelib_update_category(const GameCategory *cat);
const GameCategory *wubu_gamelib_get_category(int id);
int wubu_gamelib_assign_game_to_category(const char *game_id, int cat_id);

/* Scanning */
int wubu_gamelib_scan_steam(void);
int wubu_gamelib_scan_heroic(void);
int wubu_gamelib_scan_lutris(void);
int wubu_gamelib_scan_custom_dir(const char *path);
int wubu_gamelib_full_scan(void);

/* Launch */
int wubu_gamelib_launch_game(const char *game_id);
int wubu_gamelib_launch_with_proton(const char *game_id, const char *proton_version);

/* Favorites */
int wubu_gamelib_toggle_favorite(const char *game_id);
int wubu_gamelib_get_favorites(GameLibraryEntry **out, int *count);

/* Playtime tracking */
void wubu_gamelib_record_playtime(const char *game_id, int minutes);
int wubu_gamelib_get_playtime(const char *game_id);

/* Config persistence */
int wubu_gamelib_save_config(void);
int wubu_gamelib_load_config(void);

/* Start menu integration */
int wubu_gamelib_build_start_menu(void);        /* Populate start menu entries */
void wubu_gamelib_clear_start_menu(void);

#endif /* WUBU_GAME_LIBRARY_H */