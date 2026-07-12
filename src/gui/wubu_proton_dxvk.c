/*
 * wubu_proton_dxvk.c -- Proton DXVK/VKD3D config subsystem (GUI desktop-proton).
 *
 * Thin adapter over the canonical core src/runtime/wubu_proton_dxvk.c.
 * This file owns ONLY the GUI-specific concerns:
 *   - the FLAT conf path  <prefix>/dxvk.conf  (vs the runtime's
 *     VSL layout) -- supplied as the core's layout resolver;
 *   - prefix-STATE glue (g_proton fields) -- supplied as the core's
 *     state pull/push hooks, so config_ui_get/set round-trip live state
 *     without the core knowing about g_proton;
 *   - DXVK/VKD3D install detection (winetricks), GUI-only.
 *
 * All config-file logic (read/write, key set, UI parse/build, default
 * seeding) lives in the shared core. Nothing is duplicated with the
 * runtime copy anymore.
 */
#include "wubu_proton_internal.h"
#include "wubu_proton_dxvk.h"
#include "wubu_dxvk_conf.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

/* --- GUI flat-layout resolver --------------------------------------- */
static int dxvk_gui_conf_path(const char *prefix_id, char *out, size_t sz) {
    const ProtonPrefix *p = wubu_proton_get_prefix(prefix_id);
    if (!p) return -1;
    snprintf(out, sz, "%s/dxvk.conf", p->path);
    return 0;
}

/* --- Prefix-state pull/push (GUI-only glue) --------------------- */
static void dxvk_gui_state_pull(const char *prefix_id, DxvkConfigUI *ui) {
    const ProtonPrefix *p = wubu_proton_get_prefix(prefix_id);
    if (!p) return;
    ui->dxvk_enabled       = (p->dxvk_mode != DXVK_MODE_OFF);
    ui->dxvk_async         = p->dxvk_async;
    ui->dxvk_hud_enabled  = p->dxvk_hud_enabled;
    strncpy(ui->dxvk_hud_options, p->dxvk_hud_options,
            sizeof(ui->dxvk_hud_options) - 1);
    ui->dxvk_nvapi_hack       = p->dxvk_nvapi_hack;
    ui->dxvk_present_mode_mailbox = p->dxvk_present_mode_mailbox;
    ui->dxvk_state_cache    = p->dxvk_state_cache;
    ui->dxvk_max_device_memory = p->dxvk_max_device_memory;
    ui->dxvk_max_shared_memory = p->dxvk_max_shared_memory;
    ui->dxvk_d3d10   = p->dxvk_d3d10;
    ui->dxvk_d3d10_1 = p->dxvk_d3d10_1;
}

static int dxvk_gui_state_push(const char *prefix_id, const DxvkConfigUI *ui) {
    ProtonPrefix *p = NULL;
    for (int i = 0; i < g_proton.prefix_count; i++) {
        if (strcmp(g_proton.prefixes[i].id, prefix_id) == 0) {
            p = &g_proton.prefixes[i];
            break;
        }
    }
    if (!p) return -1;
    p->dxvk_async         = ui->dxvk_async;
    p->dxvk_hud_enabled  = ui->dxvk_hud_enabled;
    strncpy(p->dxvk_hud_options, ui->dxvk_hud_options,
            sizeof(p->dxvk_hud_options) - 1);
    p->dxvk_nvapi_hack       = ui->dxvk_nvapi_hack;
    p->dxvk_present_mode_mailbox = ui->dxvk_present_mode_mailbox;
    p->dxvk_state_cache    = ui->dxvk_state_cache;
    p->dxvk_max_device_memory = ui->dxvk_max_device_memory;
    p->dxvk_max_shared_memory = ui->dxvk_max_shared_memory;
    p->dxvk_d3d10   = ui->dxvk_d3d10;
    p->dxvk_d3d10_1 = ui->dxvk_d3d10_1;
    return 0;
}

__attribute__((constructor))
static void dxvk_gui_register(void) {
    wubu_proton_dxvk_set_resolver(dxvk_gui_conf_path);
    wubu_proton_dxvk_set_state_callbacks(dxvk_gui_state_pull,
                                        dxvk_gui_state_push);
}

/* --- Install detection (GUI-only) ------------------------------ */
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
