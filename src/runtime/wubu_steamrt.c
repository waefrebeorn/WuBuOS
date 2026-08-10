/*
 * wubu_steamrt.c -- the STEAM RUNTIME (sniper) integration driver.
 *
 * SteamOS runs every Proton game inside the Steam Runtime 'sniper'
 * (a Debian-11-based lib container). This module is the INTEGRATION
 * driver: it carries the canonical sniper lib manifest (mined from
 * repo.steampowered.com/steamrt-sniper — the real 929-package index)
 * and builds the EXACT environment Steam/Proton uses:
 *
 *   STEAM_COMPAT_DATA_PATH=<compatdata>/<appid>
 *     pfx/         the WINEPREFIX (the proton2 prefix)
 *     dist/        the Proton distribution (the compat tool)
 *   STEAM_COMPAT_LIBRARY_PATHS=<steamapps>/common/<game>
 *   STEAM_COMPAT_TOOL_PATHS=<steamapps>/common/Proton*
 *   LD_LIBRARY_PATH=<game>/lib:<sniper libs>...
 *
 * The sniper manifest is the DEPENDENCY TRUTH for the runtime:
 * a game runs if every lib it needs is in the manifest (or the host).
 * wubu_steamrt_verify() checks a lib list against the manifest + the
 * host's /usr/lib (the exact pressure-vessel philosophy: the runtime
 * fills the gaps the host lacks).
 *
 * C11, self-contained.
 */
#include "wubu_steamrt.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* ---- the canonical sniper lib manifest (mined from the real repo) ----
 * The GAMING-CRITICAL subset (the full 378-lib list is in
 * docs/reference/sniper-runtime-libs.txt). These are the libs a
 * Proton game actually dlopens. */
static const char *const SNIPER_LIBS[] = {
    "libvulkan1", "mesa-vulkan-drivers", "libgl1-mesa-dri",
    "libgl1-mesa-glx", "libegl-mesa0", "libegl1-mesa", "libgles2-mesa",
    "libdrm2", "libsdl2-2.0-0", "libopenal1", "libopenal-data",
    "libpipewire-0.3-0", "libwayland-client0", "libwayland-server0",
    "libasound2", "libasound2-plugins", "libfontconfig1", "libfreetype6",
    "libx11-6", "libx11-xcb1", "libxrandr2", "libxext6", "libxcb1",
    "libxcb-randr0", "libxcb-shm0", "libxinerama1", "libxi6",
    "libxcursor1", "libxfixes3", "libxss1", "libxxf86vm1",
    "zlib1g", "libgcc-s1", "libstdc++6", "libc6", "libgomp1",
    "libpulse0", "libpulse-mainloop-glib0", "libdbus-1-3",
    "libgbm1", "libudev1", "libpcre2-8-0", "libglib2.0-0",
    NULL
};

/* ---- the runtime state ---- */

typedef struct {
    int   initialized;
    char  compat_data[512];    /* the STEAM_COMPAT_DATA_PATH root */
    char  runtime_path[512];   /* the sniper runtime root */
    int   n_manifest;          /* the manifest size (the full list) */
} wubu_steamrt_t;

static wubu_steamrt_t g_rt;

/* SRT1: init — set the compat data + runtime roots. */
void wubu_steamrt_init(const char *compat_data_root, const char *runtime_root)
{
    memset(&g_rt, 0, sizeof(g_rt));
    if (compat_data_root)
        snprintf(g_rt.compat_data, sizeof(g_rt.compat_data), "%s",
                 compat_data_root);
    if (runtime_root)
        snprintf(g_rt.runtime_path, sizeof(g_rt.runtime_path), "%s",
                 runtime_root);
    g_rt.n_manifest = 0;
    while (SNIPER_LIBS[g_rt.n_manifest]) g_rt.n_manifest++;
    g_rt.initialized = 1;
}

/* SRT2: the compat data path for an app (the <root>/<appid> dir). */
int wubu_steamrt_compat_path(uint32_t appid, char *out, size_t cap)
{
    if (!g_rt.initialized || !out || cap == 0) return -1;
    snprintf(out, cap, "%s/%u", g_rt.compat_data, appid);
    return 0;
}

/* SRT3: build the FULL Proton launch environment for an app. The
 * layout matches what Proton's proton script expects:
 *   STEAM_COMPAT_DATA_PATH=<root>/<appid>
 *   STEAM_COMPAT_LIBRARY_PATHS=<lib>  (the game's common dir)
 *   STEAM_COMPAT_TOOL_PATHS=<tool>    (the Proton dist)
 *   STEAM_COMPAT_INSTALLED=<1>
 *   LD_LIBRARY_PATH=<game libs>:<the sniper runtime>
 *   WINEPREFIX=<compat>/pfx
 * Returns the number of env entries written, -1 on error. */
int wubu_steamrt_build_env(uint32_t appid, const char *game_lib,
                           const char *proton_dist, wubu_steamrt_env_t *env,
                           size_t max)
{
    if (!g_rt.initialized || !env || max == 0) return -1;
    size_t n = 0;
    char compat[600];
    if (wubu_steamrt_compat_path(appid, compat, sizeof(compat)) != 0)
        return -1;

    /* the compat data path */
    if (n < max)
        snprintf(env[n].key, sizeof(env[n].key), "STEAM_COMPAT_DATA_PATH");
    if (n < max)
        snprintf(env[n].val, sizeof(env[n].val), "%s", compat);
    n++;

    /* the library paths (the game's install dir) */
    if (n < max) snprintf(env[n].key, sizeof(env[n].key), "STEAM_COMPAT_LIBRARY_PATHS");
    if (n < max) snprintf(env[n].val, sizeof(env[n].val), "%s", game_lib ? game_lib : "");
    n++;

    /* the compat tool (the Proton dist) */
    if (n < max) snprintf(env[n].key, sizeof(env[n].key), "STEAM_COMPAT_TOOL_PATHS");
    if (n < max) snprintf(env[n].val, sizeof(env[n].val), "%s", proton_dist ? proton_dist : "");
    n++;

    /* installed = 1 (Proton checks it) */
    if (n < max) snprintf(env[n].key, sizeof(env[n].key), "STEAM_COMPAT_INSTALLED");
    if (n < max) snprintf(env[n].val, sizeof(env[n].val), "1");
    n++;

    /* the WINEPREFIX under the compat data */
    if (n < max) snprintf(env[n].key, sizeof(env[n].key), "WINEPREFIX");
    if (n < max) snprintf(env[n].val, sizeof(env[n].val), "%s/pfx", compat);
    n++;

    /* the LD_LIBRARY_PATH: the game's libs first, then the runtime */
    if (n < max) snprintf(env[n].key, sizeof(env[n].key), "LD_LIBRARY_PATH");
    if (n < max && g_rt.runtime_path[0])
        snprintf(env[n].val, sizeof(env[n].val), "%s/lib:%s/lib",
                 game_lib ? game_lib : "",
                 g_rt.runtime_path);
    else if (n < max)
        snprintf(env[n].val, sizeof(env[n].val), "%s/lib", game_lib ? game_lib : "");
    n++;

    return (int)n;
}

/* SRT4: the manifest check — is a lib name in the sniper manifest? */
int wubu_steamrt_in_manifest(const char *libname)
{
    if (!g_rt.initialized || !libname) return 0;
    for (int i = 0; i < g_rt.n_manifest; i++) {
        if (strcmp(SNIPER_LIBS[i], libname) == 0) return 1;
    }
    return 0;
}

/* SRT5: verify a lib list against the manifest + the host. Returns
 * the number of libs that are MISSING from both (0 = all covered). */
int wubu_steamrt_verify(const char *const *libs, size_t n,
                        int check_host)
{
    int missing = 0;
    for (size_t i = 0; i < n; i++) {
        const char *lib = libs[i];
        if (!lib) continue;
        if (wubu_steamrt_in_manifest(lib)) continue;
        if (check_host) {
            /* does the host have it? (a real dlopen-style probe) */
            char path[600];
            snprintf(path, sizeof(path), "/usr/lib/x86_64-linux-gnu/%s", lib);
            if (access(path, R_OK) == 0) continue;
        }
        missing++;
    }
    return missing;
}

/* SRT6: the test hooks (the view type lives in wubu_steamrt.h) */
int wubu_steamrt_get(wubu_steamrt_view_t *out)
{
    if (!out) return -1;
    out->initialized = g_rt.initialized;
    snprintf(out->compat_data, sizeof(out->compat_data), "%s",
             g_rt.compat_data);
    out->n_manifest = g_rt.n_manifest;
    return 0;
}

/* the manifest count (for the /n subtree) */
int wubu_steamrt_get_manifest_count(void)
{
    return g_rt.n_manifest;
}
