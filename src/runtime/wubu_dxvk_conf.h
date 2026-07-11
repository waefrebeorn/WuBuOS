/* wubu_dxvk_conf.h -- Shared DXVK config-file core.
 *
 * Extracted to eliminate the duplicated dxvk config-file logic that previously
 * lived inline in BOTH src/runtime/wubu_proton.c (runtime VSL-proton) and
 * src/gui/wubu_proton_dxvk.c (GUI desktop-proton). This module is path-agnostic
 * and state-free: callers resolve their own prefix path and (for the GUI) apply
 * their own prefix-state glue. The strstr/replace/parse/build loops now live here
 * exactly once.
 *
 * C11, minimal includes. No proton manager state, no globals.
 */
#ifndef WUBU_DXVK_CONF_H
#define WUBU_DXVK_CONF_H

#include "wubu_proton.h"   /* DxvkConfigUI + dxvk prototypes (shared header) */

#ifdef __cplusplus
extern "C" {
#endif

/* Write `content` verbatim to `path`. Returns 0 on success, -1 on error. */
int dxvk_conf_write(const char *path, const char *content);

/* Read `path` into `out` (NUL-terminated, size-1 bytes max). Returns 0 on
 * success, -1 if the file cannot be opened (out is set to empty string). */
int dxvk_conf_read(const char *path, char *out, size_t size);

/* Set key=value inside an in-memory config buffer `buf` (bufsz capacity).
 *   - If the line "key" (followed by optional spaces then '=') exists, its
 *     value is replaced with `value`.
 *   - If absent, the line "key = value" is appended (under a [dxvk] section if
 *     one exists, otherwise at end of file).
 *   - If `value` is NULL, the matching line is removed instead.
 * Returns 0 on success, -1 on buffer overflow / bad args. */
int dxvk_conf_set_key(char *buf, size_t bufsz, const char *key, const char *value);

/* Get the value for `key` from an in-memory config buffer into `out` (size).
 * Returns 0 if found, -1 if absent. */
int dxvk_conf_get_key(const char *buf, const char *key, char *out, size_t size);

/* Parse a config string into a DxvkConfigUI (file-derived fields only). */
int dxvk_conf_parse_ui(const char *config, DxvkConfigUI *ui);

/* Build a config string from a DxvkConfigUI into buf (bufsz). Uses the
 * [dxvk]-section layout shared by both subsystems. */
int dxvk_conf_build_ui(const DxvkConfigUI *ui, char *buf, size_t bufsz);

#ifdef __cplusplus
}
#endif

#endif /* WUBU_DXVK_CONF_H */
