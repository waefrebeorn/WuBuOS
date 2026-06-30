#ifndef WUBU_PROTON_H
#define WUBU_PROTON_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <time.h>

/* -- Proton Constants ----------------------------------------------- */

#define PROTON_MAX_PREFIXES 32
#define PROTON_MAX_GAMES 256
#define PROTON_PATH_MAX 4096

typedef enum {
    PROTON_VERSION_DEFAULT = 0,      /* System default */
    PROTON_VERSION_GE_LATEST = 1,    /* GloriousEggroll latest */
    PROTON_VERSION_EXPERIMENTAL = 2, /* Valve experimental */
    PROTON_VERSION_CUSTOM = 3        /* Custom path */
} ProtonVersion;

typedef enum {
    PROTON_RUNTIME_STEAM = 0,        /* Steam Linux Runtime */
    PROTON_RUNTIME_PRESSURE_VESSEL = 1, /* Pressure Vessel */
    PROTON_RUNTIME_HOST = 2          /* Host libraries */
} ProtonRuntime;

typedef enum {
    DXVK_MODE_AUTO = 0,
    DXVK_MODE_ON = 1,
    DXVK_MODE_OFF = 2
} DxvkMode;

typedef enum {
    VKD3D_MODE_AUTO = 0,
    VKD3D_MODE_ON = 1,
    VKD3D_MODE_OFF = 2
} Vkd3dMode;

/* -- Proton Prefix (Wine prefix) ------------------------------------ */

typedef struct {
    char id[64];                      /* Unique prefix ID */
    char path[PROTON_PATH_MAX];       /* Full path to prefix */
    char steam_app_id[32];            /* Steam App ID if applicable */
    char game_name[256];
    ProtonVersion proton_version;
    ProtonRuntime runtime;
    DxvkMode dxvk_mode;
    Vkd3dMode vkd3d_mode;

    /* Wine configuration */
    char windows_version[16];         /* "win10", "win7", etc. */
    bool esync;
    bool fsync;
    bool dxvk_async;
    int cpu_limit;                    /* CPU cores (0 = unlimited) */
    int memory_limit_mb;              /* Memory limit (0 = unlimited) */

    /* DLL overrides */
    char dll_overrides[1024];         /* Semicolon-separated: "d3d11=n,b;dxgi=n" */

    /* DXVK Configuration */
    char dxvk_config_path[PROTON_PATH_MAX];  /* Path to dxvk.conf */
    bool dxvk_hud_enabled;                   /* Enable DXVK_HUD */
    char dxvk_hud_options[256];              /* HUD options: "fps,devinfo,memory" */
    bool dxvk_nvapi_hack;                    /* NVAPI hack for DLSS */
    bool dxvk_present_mode_mailbox;          /* Present mode: mailbox (vsync off) */
    bool dxvk_state_cache;                   /* State cache (faster startup) */
    int dxvk_max_device_memory;              /* Max device memory (MB, 0 = auto) */
    int dxvk_max_shared_memory;              /* Max shared memory (MB, 0 = auto) */
    bool dxvk_d3d10;                         /* Enable D3D10 support */
    bool dxvk_d3d10_1;                       /* Enable D3D10.1 support */

    /* Environment variables */
    char env_vars[2048];              /* Key=value;Key=value */

    bool active;
    time_t created;
    time_t last_used;
} ProtonPrefix;

/* -- Game Entry ----------------------------------------------------- */

typedef struct {
    char id[64];                      /* Unique game ID */
    char name[256];
    char steam_app_id[32];            /* If from Steam */
    char install_path[PROTON_PATH_MAX];
    char exe_path[PROTON_PATH_MAX];   /* Relative to install_path */
    char launch_options[512];
    char prefix_id[64];               /* Associated proton prefix */
    
    /* Steam metadata */
    char appmanifest_path[PROTON_PATH_MAX];
    uint64_t size_bytes;
    time_t last_played;
    int playtime_minutes;
    
    /* Non-Steam */
    bool is_non_steam;
    char custom_exe[PROTON_PATH_MAX];
    char custom_args[512];
} ProtonGame;

/* -- Proton System State -------------------------------------------- */

typedef struct {
    char proton_base_path[PROTON_PATH_MAX];    /* ~/.local/share/wubu/proton */
    char prefixes_path[PROTON_PATH_MAX];       /* ../proton/prefixes */
    char steam_path[PROTON_PATH_MAX];          /* Steam installation */
    char steamapps_path[PROTON_PATH_MAX];      /* Steam library folders */
    
    ProtonPrefix prefixes[PROTON_MAX_PREFIXES];
    int prefix_count;
    
    ProtonGame games[PROTON_MAX_GAMES];
    int game_count;
    
    ProtonVersion default_proton_version;
    DxvkMode default_dxvk_mode;
    Vkd3dMode default_vkd3d_mode;
    
    /* Proton-GE management */
    char proton_ge_path[PROTON_PATH_MAX];
    bool proton_ge_auto_update;
} ProtonState;

/* -- Proton API ----------------------------------------------------- */

/* Initialize Proton subsystem */
int  wubu_proton_init(void);
void wubu_proton_shutdown(void);

/* Get global state */
ProtonState *wubu_proton_state(void);

/* Steam detection */
int wubu_proton_detect_steam(void);
int wubu_proton_scan_steam_library(void);
int wubu_proton_scan_steam_games(void);

/* Prefix management */
int wubu_proton_create_prefix(const char *id, const char *game_name, ProtonVersion version);
const ProtonPrefix *wubu_proton_get_prefix(const char *id);
int wubu_proton_remove_prefix(const char *id);
int wubu_proton_set_default_prefix(const char *id);
const ProtonPrefix *wubu_proton_get_default_prefix(void);

/* Game management */
int wubu_proton_add_game(const ProtonGame *game);
const ProtonGame *wubu_proton_get_game(const char *id);
int wubu_proton_remove_game(const char *id);
int wubu_proton_list_games(ProtonGame **out_games, int *out_count);

/* Launch game */
int wubu_proton_launch_game(const char *game_id);
int wubu_proton_launch_with_prefix(const char *exe_path, const char *prefix_id, char *const argv[]);

/* Proton-GE */
int wubu_proton_ge_install_latest(void);
int wubu_proton_ge_get_version(char *out_version, size_t size);
const char *wubu_proton_ge_get_path(void);

/* Wine utilities */
int wubu_proton_winecmd(const char *prefix_id, const char *const argv[]);
int wubu_proton_regedit(const char *prefix_id, const char *reg_file);
int wubu_proton_winetricks(const char *prefix_id, const char *verb);

/* DXVK/VKD3D */
int wubu_proton_dxvk_install(const char *prefix_id, DxvkMode mode);
int wubu_proton_vkd3d_install(const char *prefix_id, Vkd3dMode mode);
bool wubu_proton_dxvk_installed(const char *prefix_id);
bool wubu_proton_vkd3d_installed(const char *prefix_id);

/* DXVK Configuration */
int wubu_proton_dxvk_config_write(const char *prefix_id, const char *config_content);
int wubu_proton_dxvk_config_read(const char *prefix_id, char *out_config, size_t size);
int wubu_proton_dxvk_set_hud(const char *prefix_id, bool enable, const char *options);
int wubu_proton_dxvk_set_async(const char *prefix_id, bool async);
int wubu_proton_dxvk_set_nvapi_hack(const char *prefix_id, bool enable);
int wubu_proton_dxvk_set_present_mode(const char *prefix_id, bool mailbox);
int wubu_proton_dxvk_set_memory_limits(const char *prefix_id, int device_mb, int shared_mb);
int wubu_proton_dxvk_reset_config(const char *prefix_id);

/* DXVK Config UI - for GUI integration */
typedef struct {
    char prefix_id[64];
    bool dxvk_enabled;
    bool dxvk_async;
    bool dxvk_hud_enabled;
    char dxvk_hud_options[256];
    bool dxvk_nvapi_hack;
    bool dxvk_present_mode_mailbox;
    bool dxvk_state_cache;
    int dxvk_max_device_memory;
    int dxvk_max_shared_memory;
    bool dxvk_d3d10;
    bool dxvk_d3d10_1;
} DxvkConfigUI;

int wubu_proton_dxvk_config_ui_get(const char *prefix_id, DxvkConfigUI *out_ui);
int wubu_proton_dxvk_config_ui_set(const char *prefix_id, const DxvkConfigUI *ui);

/* Configuration persistence */
int wubu_proton_save_config(void);
int wubu_proton_load_config(void);

#endif /* WUBU_PROTON_H */