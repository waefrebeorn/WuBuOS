/*
 * wubu_bottles.h  --  WuBuOS Bottles/Lutris Integration for .wubu Containers
 *
 * Cell 480: Bottles and Lutris compatibility via .wubu container format.
 *
 * Bottles (https://github.com/bottlesdevs/Bottles) and Lutris
 * are Wine prefix managers. They organize Wine prefixes with:
 *   - Dependencies (DXVK, VKD3D, vcrun, dotnet, etc.)
 *   - Runner configurations (wine-ge, wine-staging, proton, etc.)
 *   - Game-specific settings
 *   - Flatpak integration
 *
 * WuBuOS .wubu containers provide:
 *   - Arch Linux rootfs with Wine + dependencies pre-installed
 *   - GPU/HID/USB passthrough
 *   - 9P namespace for config sharing
 *   - Portable container format (tarball or squashfs)
 *
 * This module provides:
 *   - .wubu package format specification
 *   - Bottles prefix import/export
 *   - Lutris runner configuration
 *   - Dependency management (winetricks-style)
 *   - Flatpak compatibility layer
 */

#ifndef WUBU_BOTTLES_H
#define WUBU_BOTTLES_H

#include <stdint.h>
#include <stdbool.h>
#include <time.h>

/* -- .wubu Package Format ----------------------------------------- */

/*
 * .wubu container package format:
 *
 * A .wubu file is a tar.zst (or squashfs) archive containing:
 *
 * wubu/
 *   meta.json          # Package metadata
 *   rootfs/            # Arch Linux rootfs (or overlay)
 *   prefix/            # Wine prefix (drive_c, etc.)
 *   dependencies/      # Dependency manifests
 *   scripts/           # Pre/post install scripts
 *   icons/             # App icons
 *   desktop/           # .desktop files
 *
 * meta.json:
 * {
 *   "name": "MyGame",
 *   "version": "1.0.0",
 *   "wubu_version": "2",
 *   "runner": "wine-ge-9-20",
 *   "arch": "win64",
 *   "dependencies": ["dxvk", "vkd3d", "vcrun2019", "dotnet48"],
 *   "dxvk_config": { "async": true, "hud": "fps" },
 *   "gamescope": { "mode": "steam_deck", "fsr": true },
 *   "gpu_passthrough": true,
 *   "hid_passthrough": true,
 *   "environment": { "WINEDEBUG": "-all", "DXVK_ASYNC": "1" },
 *   "mounts": [
 *     { "host": "/home/user/Games/MyGame", "guest": "/home/wubu/MyGame" }
 *   ]
 * }
 */

#define WUBU_BOTTLE_MAX_NAME      128
#define WUBU_BOTTLE_MAX_DEPS      32
#define WUBU_BOTTLE_MAX_MOUNTS    16
#define WUBU_BOTTLE_MAX_ENV       32
#define WUBU_BOTTLE_MAX_RUNNER    64

/* -- Bottle Types ------------------------------------------------- */

typedef enum {
    BOTTLE_TYPE_WINE       = 0,  /* Standard Wine prefix */
    BOTTLE_TYPE_PROTON     = 1,  /* Proton (Steam) prefix */
    BOTTLE_TYPE_LUTRIS     = 2,  /* Lutris Wine prefix */
    BOTTLE_TYPE_BOTTLES    = 3,  /* Bottles prefix */
    BOTTLE_TYPE_CUSTOM     = 4,  /* Custom runner */
} WubuBottleType;

typedef enum {
    RUNNER_WINE_SYSTEM     = 0,
    RUNNER_WINE_STAGING    = 1,
    RUNNER_WINE_GE         = 2,
    RUNNER_PROTON_GE       = 3,
    RUNNER_PROTON_STEAM    = 4,
    RUNNER_PROTON_EXP      = 5,
    RUNNER_LUTRIS_WINE     = 6,
    RUNNER_CUSTOM          = 0xFF,
} WubuRunnerType;

/* -- Dependency Types --------------------------------------------- */

typedef enum {
    DEP_DXVK          = 0,
    DEP_VKD3D         = 1,
    DEP_VCRUN         = 2,  /* vcrun2019, vcrun2022, etc. */
    DEP_DOTNET        = 3,  /* dotnet48, dotnet6, etc. */
    DEP_CORE_FONTS    = 4,
    DEP_D3DCOMPILER   = 5,
    DEP_DXVK_NVAPI    = 6,
    DEP_GAMEMODE      = 7,
    DEP_MANGOHUD      = 8,
    DEP_PROTONTRICKS  = 9,
    DEP_CUSTOM        = 0xFF,
} WubuDependencyType;

typedef struct {
    WubuDependencyType type;
    char name[64];        /* e.g., "vcrun2019", "dxvk" */
    char version[32];     /* Optional version */
    bool installed;
} WubuBottleDependency;

/* -- Mount Configuration ------------------------------------------ */

typedef struct {
    char host_path[512];
    char guest_path[512];
    bool readonly;
    bool required;        /* If true, mount must exist */
} WubuBottleMount;

/* -- Environment Variable ----------------------------------------- */

typedef struct {
    char key[128];
    char value[512];
} WubuBottleEnv;

/* -- DXVK Config (inline) ---------------------------------------- */

typedef struct {
    bool enabled;
    bool async;
    char hud[64];
    int frame_rate_limit;
    bool nvapi_hack;
    bool present_mode_mailbox;
} WubuBottleDxvkConfig;

/* -- GameScope Config (inline) ----------------------------------- */

typedef struct {
    int mode;             /* 0=off, 1=steam_deck, 2=fullscreen, 3=windowed, 4=hdr */
    bool fsr;
    int width, height;
    char filter[32];
    int refresh;
    bool hdr;
    bool fullscreen;
} WubuBottleGamescopeConfig;

/* -- Bottle Metadata ---------------------------------------------- */

typedef struct {
    char name[WUBU_BOTTLE_MAX_NAME];
    char version[32];
    char description[512];
    WubuBottleType type;
    WubuRunnerType runner;
    char runner_version[WUBU_BOTTLE_MAX_RUNNER];
    char arch[8];         /* win32 or win64 */

    WubuBottleDependency deps[WUBU_BOTTLE_MAX_DEPS];
    int dep_count;

    WubuBottleMount mounts[WUBU_BOTTLE_MAX_MOUNTS];
    int mount_count;

    WubuBottleEnv env[WUBU_BOTTLE_MAX_ENV];
    int env_count;

    WubuBottleDxvkConfig dxvk;
    WubuBottleGamescopeConfig gamescope;

    bool gpu_passthrough;
    bool hid_passthrough;
    bool audio_passthrough;
    bool network_isolated;

    /* Paths */
    char prefix_path[512];
    char rootfs_path[512];
    char exe_path[512];
    char args[1024];
    char work_dir[512];

    /* Icons/Desktop */
    char icon_path[512];
    char desktop_file[512];

    /* State */
    bool installed;
    bool verified;
    uint64_t install_size;
    time_t created;
    time_t last_run;
} WubuBottle;

/* ==================================================================
 * API: Bottle Management
 * ================================================================== */

/* Create a new empty bottle */
WubuBottle *wubu_bottle_create(const char *name, WubuBottleType type);

/* Destroy bottle struct */
void wubu_bottle_destroy(WubuBottle *bottle);

/* Load bottle from .wubu package */
WubuBottle *wubu_bottle_load(const char *package_path);

/* Save bottle to .wubu package */
int wubu_bottle_save(WubuBottle *bottle, const char *output_path);

/* Install bottle (extract rootfs, setup prefix, install deps) */
int wubu_bottle_install(WubuBottle *bottle, const char *install_dir);

/* Uninstall bottle */
int wubu_bottle_uninstall(WubuBottle *bottle);

/* Run bottle (launch game) */
int wubu_bottle_run(WubuBottle *bottle);

/* ==================================================================
 * API: Dependency Management
 * ================================================================== */

/* Add dependency to bottle */
int wubu_bottle_add_dep(WubuBottle *bottle, WubuDependencyType type, const char *version);

/* Remove dependency */
int wubu_bottle_remove_dep(WubuBottle *bottle, WubuDependencyType type);

/* Install dependencies into prefix (winetricks-style) */
int wubu_bottle_install_deps(WubuBottle *bottle, const char *prefix_path);

/* Check if dependency is available in runner */
bool wubu_bottle_dep_available(WubuRunnerType runner, WubuDependencyType type);

/* ==================================================================
 * API: Mount Management
 * ================================================================== */

/* Add mount point */
int wubu_bottle_add_mount(WubuBottle *bottle, const char *host, const char *guest, bool readonly);

/* Remove mount point */
int wubu_bottle_remove_mount(WubuBottle *bottle, const char *guest_path);

/* ==================================================================
 * API: Environment
 * ================================================================== */

/* Set environment variable */
int wubu_bottle_set_env(WubuBottle *bottle, const char *key, const char *value);

/* Get environment variable */
const char *wubu_bottle_get_env(WubuBottle *bottle, const char *key);

/* ==================================================================
 * API: Bottles Import/Export
 * ================================================================== */

/* Import from Bottles prefix directory */
int wubu_bottle_import_bottles(const char *bottles_prefix, WubuBottle *bottle);

/* Export to Bottles-compatible format */
int wubu_bottle_export_bottles(WubuBottle *bottle, const char *output_dir);

/* Import from Lutris prefix */
int wubu_bottle_import_lutris(const char *lutris_prefix, WubuBottle *bottle);

/* Export to Lutris configuration */
int wubu_bottle_export_lutris(WubuBottle *bottle, const char *output_path);

/* ==================================================================
 * API: Flatpak Integration
 * ================================================================== */

/* Generate Flatpak manifest from bottle */
int wubu_bottle_flatpak_manifest(WubuBottle *bottle, const char *output_path);

/* Check Flatpak runtime availability */
bool wubu_bottle_flatpak_runtime_available(const char *runtime);

/* ==================================================================
 * API: Query
 * ================================================================== */

int wubu_bottle_list(const char *install_dir, WubuBottle ***out_bottles, int *count);
const WubuBottle *wubu_bottle_find(const char *install_dir, const char *name);
bool wubu_bottle_verify(WubuBottle *bottle);

#endif /* WUBU_BOTTLES_H */
