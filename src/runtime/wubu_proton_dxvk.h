/*
 * wubu_proton_dxvk.h -- canonical Proton DXVK/VKD3D config core (dedup home).
 *
 * Previously duplicated (subtly divergently) in:
 *   - src/runtime/wubu_proton.c   (VSL-proton prefix layout)
 *   - src/gui/wubu_proton_dxvk.c  (flat desktop-proton prefix layout)
 *
 * Both are now thin adapters that register a LAYOUT resolver (and, for the
 * GUI, a prefix-STATE pull/push pair) and let THIS core own every
 * wubu_proton_dxvk_* operation. No config logic is duplicated.
 *
 * C11, minimal includes, opaque to callers (they pass a prefix_id).
 */
#ifndef WUBU_PROTON_DXVK_H
#define WUBU_PROTON_DXVK_H

#include <stdbool.h>
#include <stddef.h>
#include "wubu_proton.h"   /* DxvkConfigUI, DxvkMode, Vkd3dMode, ProtonPrefix */

/* --- Layout resolver (per build) -------------------------------------
 * Fills `out` (size `sz`) with the absolute dxvk.conf path for `prefix_id`;
 * returns 0 on success or -1 if the prefix is unknown. Each build
 * (runtime VSL layout vs GUI flat layout) supplies its own. */
typedef int  (*wubu_proton_dxvk_path_resolver)(const char *prefix_id,
                                                 char *out, size_t sz);
void wubu_proton_dxvk_set_resolver(wubu_proton_dxvk_path_resolver r);

/* --- Prefix-STATE callbacks (GUI only) ----------------------------
 * The config FILE fields are handled by the core. The GUI additionally
 * keeps live prefix state in g_proton; it registers pull/push hooks so
 * the core's config_ui_get/set can round-trip that state without the
 * core knowing about g_proton. Runtime registers NULL (file-only). */
typedef void (*wubu_proton_dxvk_state_pull)(const char *prefix_id,
                                                DxvkConfigUI *ui);
typedef int  (*wubu_proton_dxvk_state_push)(const char *prefix_id,
                                                const DxvkConfigUI *ui);
void wubu_proton_dxvk_set_state_callbacks(wubu_proton_dxvk_state_pull pull,
                                          wubu_proton_dxvk_state_push push);

/* Resolve a conf path via the registered resolver. Returns the path buffer
 * (static, valid until next call) or NULL on failure. */
const char *wubu_proton_dxvk_conf_path(const char *prefix_id);

/* --- Canonical config operations (layout-agnostic) -------------------- */
int  wubu_proton_dxvk_config_write(const char *prefix_id, const char *config_content);
int  wubu_proton_dxvk_config_read (const char *prefix_id, char *out_config, size_t size);
int  wubu_proton_dxvk_set_hud        (const char *prefix_id, bool enable, const char *options);
int  wubu_proton_dxvk_set_async      (const char *prefix_id, bool async);
int  wubu_proton_dxvk_set_nvapi_hack(const char *prefix_id, bool enable);
int  wubu_proton_dxvk_set_present_mode(const char *prefix_id, bool mailbox);
int  wubu_proton_dxvk_set_memory_limits(const char *prefix_id, int device_mb, int shared_mb);
int  wubu_proton_dxvk_reset_config  (const char *prefix_id);
int  wubu_proton_dxvk_config_ui_get (const char *prefix_id, DxvkConfigUI *out_ui);
int  wubu_proton_dxvk_config_ui_set (const char *prefix_id, const DxvkConfigUI *ui);

/* Default dxvk.conf content seeded when a prefix has none. */
extern const char *WUBU_PROTON_DXVK_DEFAULT_CONFIG;

#endif /* WUBU_PROTON_DXVK_H */
