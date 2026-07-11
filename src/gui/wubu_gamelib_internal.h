/* wubu_gamelib_internal.h -- WuBuOS Game Library internal header.
 * Shared declarations for gamelib sub-modules (scan).
 * Public API + types in wubu_gamelib.h.
 */

#ifndef WUBU_GAMELIB_INTERNAL_H
#define WUBU_GAMELIB_INTERNAL_H

#include "wubu_gamelib.h"

/* -- Shared helpers (implemented in wubu_gamelib_scan.c) ----------- */
void    generate_game_id(GameSource source, const char *source_id, char *out_id, size_t size);
uint64_t gamelib_get_dir_size(const char *path);
char    *make_sort_name(const char *name);

/* -- Scanners (implemented in wubu_gamelib_scan.c) --------------- */
int wubu_gamelib_scan_steam(void);
int wubu_gamelib_scan_heroic(void);
int wubu_gamelib_scan_lutris(void);
int wubu_gamelib_scan_custom_dir(const char *path);
int wubu_gamelib_full_scan(void);

#endif /* WUBU_GAMELIB_INTERNAL_H */

/* Shared module-global (promoted from wubu_gamelib.c so submodules link one impl). */
extern GameLibraryState g_gamelib;

/* Module-local dir helper (renamed from ensure_dir to avoid collision with
   wubu_proton_util.c's non-static ensure_dir -- same dedup-pitfall handling as
   archd_rm_rf / bottles_rm_rf). */
bool gamelib_ensure_dir(const char *path);
