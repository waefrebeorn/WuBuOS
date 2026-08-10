/*
 * wubu_steamrt.h -- the Steam Runtime (sniper) integration driver.
 */
#ifndef WUBU_STEAMRT_H
#define WUBU_STEAMRT_H

#include <stddef.h>
#include <stdint.h>

/* one environment entry (a key=value pair for the launch env) */
typedef struct {
    char key[64];
    char val[600];
} wubu_steamrt_env_t;

/* SRT1: init — the compat-data root + the sniper runtime root. */
void wubu_steamrt_init(const char *compat_data_root, const char *runtime_root);

/* SRT2: the compat data path for an app (<root>/<appid>). */
int wubu_steamrt_compat_path(uint32_t appid, char *out, size_t cap);

/* SRT3: build the FULL Proton launch env (STEAM_COMPAT_* + WINEPREFIX
 * + LD_LIBRARY_PATH). Returns the env count, -1 on error. */
int wubu_steamrt_build_env(uint32_t appid, const char *game_lib,
                           const char *proton_dist, wubu_steamrt_env_t *env,
                           size_t max);

/* SRT4: is a lib name in the sniper manifest? */
int wubu_steamrt_in_manifest(const char *libname);

/* SRT5: verify a lib list against the manifest (+ host). Returns the
 * missing count (0 = fully covered). */
int wubu_steamrt_verify(const char *const *libs, size_t n, int check_host);

/* SRT6: the test hooks. */
typedef struct {
    int  initialized;
    char compat_data[512];
    int  n_manifest;
} wubu_steamrt_view_t;
int wubu_steamrt_get(wubu_steamrt_view_t *out);

#endif

/* the manifest count (for the /n subtree) */
int wubu_steamrt_get_manifest_count(void);
