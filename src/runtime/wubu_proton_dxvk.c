/*
 * wubu_proton_dxvk.c -- canonical Proton DXVK/VKD3D config core (dedup home).
 *
 * Writes/reads the per-prefix dxvk.conf and exposes the wubu_proton_dxvk_*
 * API used by both the runtime (VSL-proton layout) and the GUI
 * (flat desktop-proton layout). All config-file manipulation is delegated to the
 * shared wubu_dxvk_conf.c engine (dxvk_conf_read/write/set_key/parse_ui/
 * build_ui); this file owns the prefix_id -> conf_path mapping (via a
 * registered resolver) and, optionally, prefix-STATE round-tripping via
 * registered pull/push callbacks (GUI only).
 *
 * This is the SINGLE source of truth for the dxvk config logic. The two
 * legacy copies (src/runtime/wubu_proton.c, src/gui/wubu_proton_dxvk.c)
 * were divergent duplicates; they now only register their resolver/state
 * hooks and call into this core.
 */
#include "wubu_proton_dxvk.h"
#include "wubu_dxvk_conf.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Default dxvk.conf content seeded when a prefix has none. */
const char *WUBU_PROTON_DXVK_DEFAULT_CONFIG =
    "[dxvk]\n"
    "d3d10 = true\n"
    "d3d10_1 = true\n"
    "d3d11 = true\n"
    "dxgi = true\n"
    "\n"
    "[nvapi]\n"
    "nvapi_hack = false\n"
    "\n"
    "[present]\n"
    "present_mode = auto\n"
    "\n"
    "[device]\n"
    "max_device_memory = 0\n"
    "max_shared_memory = 0\n"
    "\n"
    "[state_cache]\n"
    "enable = true\n"
    "path = \"${WINEPREFIX}/dxvk_state_cache\"\n";

/* Active layout resolver + optional prefix-state hooks (set at init). */
static wubu_proton_dxvk_path_resolver   g_resolver;
static wubu_proton_dxvk_state_pull     g_state_pull;
static wubu_proton_dxvk_state_push     g_state_push;

void wubu_proton_dxvk_set_resolver(wubu_proton_dxvk_path_resolver r) {
    g_resolver = r;
}
void wubu_proton_dxvk_set_state_callbacks(wubu_proton_dxvk_state_pull pull,
                                          wubu_proton_dxvk_state_push push) {
    g_state_pull = pull;
    g_state_push = push;
}

const char *wubu_proton_dxvk_conf_path(const char *prefix_id) {
    static char path[1024];
    if (!g_resolver) return NULL;
    if (g_resolver(prefix_id, path, sizeof(path)) < 0) return NULL;
    return path;
}

/* Read the prefix's dxvk.conf, seeding the default if it is missing/empty. */
static int dxvk_read_seeded(const char *prefix_id, char *buf, size_t bufsz) {
    const char *path = wubu_proton_dxvk_conf_path(prefix_id);
    if (!path) return -1;
    if (dxvk_conf_read(path, buf, bufsz) < 0 || buf[0] == '\0')
        snprintf(buf, bufsz, "%s", WUBU_PROTON_DXVK_DEFAULT_CONFIG);
    return 0;
}

int wubu_proton_dxvk_config_write(const char *prefix_id, const char *config_content) {
    if (!prefix_id || !config_content) return -1;
    const char *path = wubu_proton_dxvk_conf_path(prefix_id);
    if (!path) return -1;
    return dxvk_conf_write(path, config_content);
}

int wubu_proton_dxvk_config_read(const char *prefix_id, char *out_config, size_t size) {
    if (!prefix_id || !out_config || size == 0) return -1;
    const char *path = wubu_proton_dxvk_conf_path(prefix_id);
    if (!path) return -1;
    return dxvk_conf_read(path, out_config, size);
}

int wubu_proton_dxvk_set_hud(const char *prefix_id, bool enable, const char *options) {
    if (!prefix_id) return -1;
    const char *path = wubu_proton_dxvk_conf_path(prefix_id);
    if (!path) return -1;
    char buf[8192];
    dxvk_read_seeded(prefix_id, buf, sizeof(buf));
    dxvk_conf_set_key(buf, sizeof(buf), "hud",
                     enable ? (options && options[0] ? options : "fps,devinfo,memory") : NULL);
    return dxvk_conf_write(path, buf);
}

int wubu_proton_dxvk_set_async(const char *prefix_id, bool async) {
    if (!prefix_id) return -1;
    const char *path = wubu_proton_dxvk_conf_path(prefix_id);
    if (!path) return -1;
    char buf[8192];
    dxvk_read_seeded(prefix_id, buf, sizeof(buf));
    dxvk_conf_set_key(buf, sizeof(buf), "async", async ? "true" : "false");
    return dxvk_conf_write(path, buf);
}

int wubu_proton_dxvk_set_nvapi_hack(const char *prefix_id, bool enable) {
    if (!prefix_id) return -1;
    const char *path = wubu_proton_dxvk_conf_path(prefix_id);
    if (!path) return -1;
    char buf[8192];
    dxvk_read_seeded(prefix_id, buf, sizeof(buf));
    dxvk_conf_set_key(buf, sizeof(buf), "nvapiHack", enable ? "true" : "false");
    return dxvk_conf_write(path, buf);
}

int wubu_proton_dxvk_set_present_mode(const char *prefix_id, bool mailbox) {
    if (!prefix_id) return -1;
    const char *path = wubu_proton_dxvk_conf_path(prefix_id);
    if (!path) return -1;
    char buf[8192];
    dxvk_read_seeded(prefix_id, buf, sizeof(buf));
    dxvk_conf_set_key(buf, sizeof(buf), "presentMode", mailbox ? "1" : "0");
    return dxvk_conf_write(path, buf);
}

int wubu_proton_dxvk_set_memory_limits(const char *prefix_id, int device_mb, int shared_mb) {
    if (!prefix_id) return -1;
    const char *path = wubu_proton_dxvk_conf_path(prefix_id);
    if (!path) return -1;
    char buf[8192];
    dxvk_read_seeded(prefix_id, buf, sizeof(buf));
    char v[32];
    snprintf(v, sizeof(v), "%d", device_mb);
    dxvk_conf_set_key(buf, sizeof(buf), "maxDeviceMemory", v);
    snprintf(v, sizeof(v), "%d", shared_mb);
    dxvk_conf_set_key(buf, sizeof(buf), "maxSharedMemory", v);
    return dxvk_conf_write(path, buf);
}

int wubu_proton_dxvk_reset_config(const char *prefix_id) {
    if (!prefix_id) return -1;
    const char *path = wubu_proton_dxvk_conf_path(prefix_id);
    if (!path) return -1;
    return dxvk_conf_write(path, WUBU_PROTON_DXVK_DEFAULT_CONFIG);
}

int wubu_proton_dxvk_config_ui_get(const char *prefix_id, DxvkConfigUI *out_ui) {
    if (!prefix_id || !out_ui) return -1;
    const char *path = wubu_proton_dxvk_conf_path(prefix_id);
    if (!path) return -1;
    char buf[8192];
    dxvk_read_seeded(prefix_id, buf, sizeof(buf));
    dxvk_conf_parse_ui(buf, out_ui);
    strncpy(out_ui->prefix_id, prefix_id, sizeof(out_ui->prefix_id) - 1);
    out_ui->prefix_id[sizeof(out_ui->prefix_id) - 1] = '\0';
    if (g_state_pull) g_state_pull(prefix_id, out_ui);
    return 0;
}

int wubu_proton_dxvk_config_ui_set(const char *prefix_id, const DxvkConfigUI *ui) {
    if (!prefix_id || !ui) return -1;
    const char *path = wubu_proton_dxvk_conf_path(prefix_id);
    if (!path) return -1;
    if (g_state_push && g_state_push(prefix_id, ui) < 0) return -1;
    char buf[8192];
    dxvk_conf_build_ui(ui, buf, sizeof(buf));
    return dxvk_conf_write(path, buf);
}
