/* wubu_bottles_internal.h -- Internal helpers shared by wubu_bottles sub-modules.
 * Public API + types in wubu_bottles.h. The JSON parsing helpers live in
 * wubu_bottles_json.c and are declared here so all submodules link the SAME
 * implementation (no double-coding).
 */

#ifndef WUBU_BOTTLES_INTERNAL_H
#define WUBU_BOTTLES_INTERNAL_H

#include "wubu_bottles.h"
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>

/* -- JSON literal extraction (wubu_bottles_json.c) ---------------- */
const char *json_find_string_literal(const char *json, const char *key);
int         json_find_int_literal(const char *json, const char *key);
bool        json_find_bool_literal(const char *json, const char *key);
/* -- Recursive delete (wubu_bottles_fs.c, wraps wubu_fs_util) -- */
int bottles_rm_rf(const char *path);

#endif /* WUBU_BOTTLES_INTERNAL_H */
