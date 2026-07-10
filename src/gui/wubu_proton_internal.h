/*
 * wubu_proton_internal.h -- WuBuOS Proton layer internal header.
 * Shared state + declarations for proton sub-modules (util, dxvk).
 * C11 opaque-struct pattern: public types/API in wubu_proton.h, private here.
 */

#ifndef WUBU_PROTON_INTERNAL_H
#define WUBU_PROTON_INTERNAL_H

#include "wubu_proton.h"

/* Shared proton state (defined in wubu_proton.c facade). */
extern ProtonState g_proton;

/* -- Util helpers (implemented in wubu_proton_util.c) -------------- */
bool     file_exists(const char *path);
int      rm_rf(const char *path);
bool     ensure_dir(const char *path);
uint64_t get_dir_size(const char *path);
char    *find_steam_path(void);
void     parse_vdf_pairs(const char *data, size_t len, char ***keys, char ***vals, int *count);
int      parse_appmanifest(const char *path, char *appid, size_t aid_len,
                           char *name, size_t name_len, char *installdir, size_t idir_len);

#endif /* WUBU_PROTON_INTERNAL_H */
