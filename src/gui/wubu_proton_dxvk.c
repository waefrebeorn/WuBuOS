/* wubu_proton_dxvk.c -- Proton DXVK/VKD3D config subsystem (GUI desktop-proton).
 *
 * Uses the shared wubu_dxvk_conf.c core for all config-file manipulation (read/
 * write + line set/replace/remove + UI parse/build), eliminating the logic that
 * was previously duplicated with src/runtime/wubu_proton.c. This file retains the
 * GUI-specific concerns: prefix-state (g_proton) glue, DEFAULT_DXVK_CONFIG
 * seeding, and the installed/install/vkd3d helpers.
 */
#include "wubu_proton_internal.h"
#include "wubu_dxvk_conf.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

bool wubu_proton_dxvk_installed(const char *prefix_id) {
    const ProtonPrefix *p = wubu_proton_get_prefix(prefix_id);
    if (!p) return false;

    char dxvk_path[PROTON_PATH_MAX];
    snprintf(dxvk_path, sizeof(dxvk_path), "%s/drive_c/windows/system32/d3d11.dll", p->path);
    return file_exists(dxvk_path);
}

bool wubu_proton_vkd3d_installed(const char *prefix_id) {
    const ProtonPrefix *p = wubu_proton_get_prefix(prefix_id);
    if (!p) return false;

    char vkd3d_path[PROTON_PATH_MAX];
    snprintf(vkd3d_path, sizeof(vkd3d_path), "%s/drive_c/windows/system32/d3d12.dll", p->path);
    return file_exists(vkd3d_path);
}

/* ============================================================
 * DXVK Configuration
 * ============================================================ */

static const char *DEFAULT_DXVK_CONFIG =
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

static char *dxvk_config_path(const char *prefix_id) {
    static char path[PROTON_PATH_MAX];
    const ProtonPrefix *p = wubu_proton_get_prefix(prefix_id);
    if (!p) return NULL;
    snprintf(path, sizeof(path), "%s/dxvk.conf", p->path);
    return path;
}

/* Read the prefix's dxvk.conf, seeding DEFAULT_DXVK_CONFIG if empty. */
static int dxvk_read_seeded(const char *prefix_id, char *buf, size_t bufsz) {
    const char *path = dxvk_config_path(prefix_id);
    if (!path) return -1;
    if (dxvk_conf_read(path, buf, bufsz) < 0 || buf[0] == '\0')
        snprintf(buf, bufsz, "%s", DEFAULT_DXVK_CONFIG);
    return 0;
}

int wubu_proton_dxvk_config_write(const char *prefix_id, const char *config_content) {
    const char *path = dxvk_config_path(prefix_id);
    if (!path || !config_content) return -1;
    return dxvk_conf_write(path, config_content);
}

int wubu_proton_dxvk_config_read(const char *prefix_id, char *out_config, size_t size) {
    const char *path = dxvk_config_path(prefix_id);
    if (!path || !out_config || size == 0) return -1;
    return dxvk_conf_read(path, out_config, size);
}

int wubu_proton_dxvk_set_hud(const char *prefix_id, bool enable, const char *options) {
    const char *path = dxvk_config_path(prefix_id);
    if (!path) return -1;
    char buf[8192];
    dxvk_read_seeded(prefix_id, buf, sizeof(buf));
    dxvk_conf_set_key(buf, sizeof(buf), "hud",
                      enable ? (options && options[0] ? options : "fps,devinfo,memory") : NULL);
    return dxvk_conf_write(path, buf);
}

int wubu_proton_dxvk_set_async(const char *prefix_id, bool async) {
    const char *path = dxvk_config_path(prefix_id);
    if (!path) return -1;
    char buf[8192];
    dxvk_read_seeded(prefix_id, buf, sizeof(buf));
    dxvk_conf_set_key(buf, sizeof(buf), "async", async ? "true" : "false");
    return dxvk_conf_write(path, buf);
}

int wubu_proton_dxvk_set_nvapi_hack(const char *prefix_id, bool enable) {
    const char *path = dxvk_config_path(prefix_id);
    if (!path) return -1;
    char buf[8192];
    dxvk_read_seeded(prefix_id, buf, sizeof(buf));
    dxvk_conf_set_key(buf, sizeof(buf), "nvapiHack", enable ? "true" : "false");
    return dxvk_conf_write(path, buf);
}

int wubu_proton_dxvk_set_present_mode(const char *prefix_id, bool mailbox) {
    const char *path = dxvk_config_path(prefix_id);
    if (!path) return -1;
    char buf[8192];
    dxvk_read_seeded(prefix_id, buf, sizeof(buf));
    dxvk_conf_set_key(buf, sizeof(buf), "presentMode", mailbox ? "1" : "0");
    return dxvk_conf_write(path, buf);
}

int wubu_proton_dxvk_set_memory_limits(const char *prefix_id, int device_mb, int shared_mb) {
    const char *path = dxvk_config_path(prefix_id);
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
    return wubu_proton_dxvk_config_write(prefix_id, DEFAULT_DXVK_CONFIG);
}

int wubu_proton_dxvk_install(const char *prefix_id, DxvkMode mode) {
    if (mode == DXVK_MODE_OFF) return 0;
    const char *argv[] = {"winetricks", "-q", "dxvk", NULL};
    return wubu_proton_winecmd(prefix_id, argv);
}

int wubu_proton_vkd3d_install(const char *prefix_id, Vkd3dMode mode) {
    if (mode == VKD3D_MODE_OFF) return 0;
    const char *argv[] = {"winetricks", "-q", "vkd3d", NULL};
    return wubu_proton_winecmd(prefix_id, argv);
}

int wubu_proton_dxvk_config_ui_get(const char *prefix_id, DxvkConfigUI *out_ui) {
    if (!prefix_id || !out_ui) return -1;

    const ProtonPrefix *p = wubu_proton_get_prefix(prefix_id);
    if (!p) return -1;

    char buf[8192];
    dxvk_read_seeded(prefix_id, buf, sizeof(buf));
    dxvk_conf_parse_ui(buf, out_ui);

    /* Prefix-state fields (not derivable from the config file). */
    strncpy(out_ui->prefix_id, p->id, sizeof(out_ui->prefix_id) - 1);
    out_ui->dxvk_enabled = (p->dxvk_mode != DXVK_MODE_OFF);
    out_ui->dxvk_async = p->dxvk_async;
    out_ui->dxvk_hud_enabled = p->dxvk_hud_enabled;
    strncpy(out_ui->dxvk_hud_options, p->dxvk_hud_options, sizeof(out_ui->dxvk_hud_options) - 1);
    out_ui->dxvk_nvapi_hack = p->dxvk_nvapi_hack;
    out_ui->dxvk_present_mode_mailbox = p->dxvk_present_mode_mailbox;
    out_ui->dxvk_state_cache = p->dxvk_state_cache;
    out_ui->dxvk_max_device_memory = p->dxvk_max_device_memory;
    out_ui->dxvk_max_shared_memory = p->dxvk_max_shared_memory;
    out_ui->dxvk_d3d10 = p->dxvk_d3d10;
    out_ui->dxvk_d3d10_1 = p->dxvk_d3d10_1;

    return 0;
}

int wubu_proton_dxvk_config_ui_set(const char *prefix_id, const DxvkConfigUI *ui) {
    if (!prefix_id || !ui) return -1;

    ProtonPrefix *p = NULL;
    for (int i = 0; i < g_proton.prefix_count; i++) {
        if (strcmp(g_proton.prefixes[i].id, prefix_id) == 0) {
            p = &g_proton.prefixes[i];
            break;
        }
    }
    if (!p) return -1;

    /* Update prefix state. */
    p->dxvk_async = ui->dxvk_async;
    p->dxvk_hud_enabled = ui->dxvk_hud_enabled;
    strncpy(p->dxvk_hud_options, ui->dxvk_hud_options, sizeof(p->dxvk_hud_options) - 1);
    p->dxvk_nvapi_hack = ui->dxvk_nvapi_hack;
    p->dxvk_present_mode_mailbox = ui->dxvk_present_mode_mailbox;
    p->dxvk_state_cache = ui->dxvk_state_cache;
    p->dxvk_max_device_memory = ui->dxvk_max_device_memory;
    p->dxvk_max_shared_memory = ui->dxvk_max_shared_memory;
    p->dxvk_d3d10 = ui->dxvk_d3d10;
    p->dxvk_d3d10_1 = ui->dxvk_d3d10_1;

    /* Apply to config file via the shared core. */
    char buf[8192];
    dxvk_conf_build_ui(ui, buf, sizeof(buf));
    return dxvk_conf_write(dxvk_config_path(prefix_id), buf);
}
