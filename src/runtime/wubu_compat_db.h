/*
 * wubu_compat_db.h -- WuBuOS per-app Windows-compat database (SteamOS ProtonDB
 * + shader-cache lesson).
 *
 * Real compatibility layer: each Windows title has an entry recording the
 * Proton/proton-flags, env overrides, and DLL overrides to apply at launch
 * (the ProtonDB lesson), plus a per-title cache directory (shader/proton
 * cache analog). Stored on disk under ~/.wubu/compat/<title>.json so it
 * survives reboots and is inspectable -- no SQLite dependency.
 */

#ifndef WUBU_COMPAT_DB_H
#define WUBU_COMPAT_DB_H

#include <stdbool.h>

#define WUBU_COMPAT_TITLE_MAX   128
#define WUBU_COMPAT_FLAG_MAX    256
#define WUBU_COMPAT_ENV_MAX     1024   /* newline-separated KEY=VALUE list */
#define WUBU_COMPAT_STORE      "~/.wubu/compat"

/* A single title's compatibility profile. */
typedef struct {
    char title[WUBU_COMPAT_TITLE_MAX];   /* normalized title key */
    char proton_ver[WUBU_COMPAT_FLAG_MAX];/* e.g. "proton-8.0", "GE-Proton7-43" */
    char proton_flags[WUBU_COMPAT_FLAG_MAX]; /* e.g. "dxvk_async,fsync" */
    char env_overrides[WUBU_COMPAT_ENV_MAX]; /* "KEY=VAL\nKEY=VAL" */
    char dll_overrides[WUBU_COMPAT_FLAG_MAX]; /* "d3d11=n;dxgi=n" */
    int  rating;                          /* 0=unknown .. 5=platinum */
    bool cache_enabled;                   /* per-title shader/proton cache on? */
} WubuCompatEntry;

/* Initialize the compat DB (ensure store dir). Returns 0 on success. */
int  wubu_compat_db_init(void);

/* Save an entry for `title` (upsert). Returns 0 on success. */
int  wubu_compat_db_set(const WubuCompatEntry *e);

/* Load an entry for `title` into `out`. Returns 0 if found, -1 if not. */
int  wubu_compat_db_get(const char *title, WubuCompatEntry *out);

/* Delete an entry. Returns 0 on success (or if it didn't exist). */
int  wubu_compat_db_del(const char *title);

/* Resolve the per-title cache directory path (creates it if enabled).
 * Returns 0 on success and writes the path into out_path. */
int  wubu_compat_cache_dir(const char *title, char *out_path, int path_len);

/* Normalize a raw title into the stored key (lowercase, spaces->underscore,
 * strip punctuation). Writes into out (size title_max). */
void wubu_compat_normalize_title(const char *raw, char *out, int out_len);

#endif /* WUBU_COMPAT_DB_H */
