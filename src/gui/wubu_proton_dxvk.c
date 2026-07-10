/* wubu_proton_dxvk.c -- Proton DXVK/VKD3D config subsystem.
 *
 * Self-contained module extracted from wubu_proton.c. Owns DXVK config file
 * read/write and the dxvk_set_* tuning knobs. Uses the shared g_proton state
 * + Proton API via wubu_proton_internal.h. Minimal includes.
 */

#include "wubu_proton_internal.h"
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

int wubu_proton_dxvk_config_write(const char *prefix_id, const char *config_content) {
    const char *path = dxvk_config_path(prefix_id);
    if (!path || !config_content) return -1;

    FILE *f = fopen(path, "w");
    if (!f) return -1;

    fprintf(f, "%s", config_content);
    fclose(f);
    return 0;
}

int wubu_proton_dxvk_config_read(const char *prefix_id, char *out_config, size_t size) {
    const char *path = dxvk_config_path(prefix_id);
    if (!path || !out_config || size == 0) return -1;

    FILE *f = fopen(path, "r");
    if (!f) return -1;

    size_t read = fread(out_config, 1, size - 1, f);
    out_config[read] = '\0';
    fclose(f);
    return 0;
}

int wubu_proton_dxvk_set_hud(const char *prefix_id, bool enable, const char *options) {
    const ProtonPrefix *p = wubu_proton_get_prefix(prefix_id);
    if (!p) return -1;

    char config[4096];
    wubu_proton_dxvk_config_read(prefix_id, config, sizeof(config));
    if (!config[0]) strcpy(config, DEFAULT_DXVK_CONFIG);

    char *hud_line = strstr(config, "hud = ");
    if (enable) {
        if (hud_line) {
            /* Update existing */
            const char *rest = hud_line + 6; /* skip "hud = " */
            if (options && options[0]) {
                snprintf(hud_line, config + sizeof(config) - hud_line, "hud = %s", options);
            } else {
                snprintf(hud_line, config + sizeof(config) - hud_line, "hud = fps,devinfo,memory");
            }
        } else {
            /* Add under [hud] section or append */
            strcat(config, "\n[hud]\nhud = ");
            strcat(config, options ? options : "fps,devinfo,memory");
        }
    } else {
        if (hud_line) {
            /* Comment out */
            memmove(hud_line, hud_line + 1, strlen(hud_line));
        }
    }

    return wubu_proton_dxvk_config_write(prefix_id, config);
}

int wubu_proton_dxvk_set_async(const char *prefix_id, bool async) {
    const ProtonPrefix *p = wubu_proton_get_prefix(prefix_id);
    if (!p) return -1;

    char config[4096];
    wubu_proton_dxvk_config_read(prefix_id, config, sizeof(config));
    if (!config[0]) strcpy(config, DEFAULT_DXVK_CONFIG);

    char *async_line = strstr(config, "async = ");
    if (async) {
        if (async_line) {
            snprintf(async_line, config + sizeof(config) - async_line, "async = true");
        } else {
            strcat(config, "\nasync = true");
        }
    } else {
        if (async_line) {
            snprintf(async_line, config + sizeof(config) - async_line, "async = false");
        }
    }

    return wubu_proton_dxvk_config_write(prefix_id, config);
}

int wubu_proton_dxvk_set_nvapi_hack(const char *prefix_id, bool enable) {
    const ProtonPrefix *p = wubu_proton_get_prefix(prefix_id);
    if (!p) return -1;

    char config[4096];
    wubu_proton_dxvk_config_read(prefix_id, config, sizeof(config));
    if (!config[0]) strcpy(config, DEFAULT_DXVK_CONFIG);

    char *nvapi_line = strstr(config, "nvapi_hack = ");
    if (enable) {
        if (nvapi_line) {
            snprintf(nvapi_line, config + sizeof(config) - nvapi_line, "nvapi_hack = true");
        } else {
            strcat(config, "\n[nvapi]\nnvapi_hack = true");
        }
    } else {
        if (nvapi_line) {
            snprintf(nvapi_line, config + sizeof(config) - nvapi_line, "nvapi_hack = false");
        }
    }

    return wubu_proton_dxvk_config_write(prefix_id, config);
}

int wubu_proton_dxvk_set_present_mode(const char *prefix_id, bool mailbox) {
    const ProtonPrefix *p = wubu_proton_get_prefix(prefix_id);
    if (!p) return -1;

    char config[4096];
    wubu_proton_dxvk_config_read(prefix_id, config, sizeof(config));
    if (!config[0]) strcpy(config, DEFAULT_DXVK_CONFIG);

    char *present_line = strstr(config, "present_mode = ");
    if (mailbox) {
        if (present_line) {
            snprintf(present_line, config + sizeof(config) - present_line, "present_mode = mailbox");
        } else {
            strcat(config, "\n[present]\npresent_mode = mailbox");
        }
    } else {
        if (present_line) {
            snprintf(present_line, config + sizeof(config) - present_line, "present_mode = auto");
        }
    }

    return wubu_proton_dxvk_config_write(prefix_id, config);
}

int wubu_proton_dxvk_set_memory_limits(const char *prefix_id, int device_mb, int shared_mb) {
    const ProtonPrefix *p = wubu_proton_get_prefix(prefix_id);
    if (!p) return -1;

    char config[4096];
    wubu_proton_dxvk_config_read(prefix_id, config, sizeof(config));
    if (!config[0]) strcpy(config, DEFAULT_DXVK_CONFIG);

    char *dev_line = strstr(config, "max_device_memory = ");
    if (dev_line) {
        snprintf(dev_line, config + sizeof(config) - dev_line, "max_device_memory = %d", device_mb);
    } else {
        strcat(config, "\n[device]\nmax_device_memory = ");
        snprintf(config + strlen(config), sizeof(config) - strlen(config), "%d", device_mb);
    }

    char *shared_line = strstr(config, "max_shared_memory = ");
    if (shared_line) {
        snprintf(shared_line, config + sizeof(config) - shared_line, "max_shared_memory = %d", shared_mb);
    } else {
        strcat(config, "\nmax_shared_memory = ");
        snprintf(config + strlen(config), sizeof(config) - strlen(config), "%d", shared_mb);
    }

    return wubu_proton_dxvk_config_write(prefix_id, config);
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

    memset(out_ui, 0, sizeof(DxvkConfigUI));
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

    /* Update prefix fields */
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

    /* Apply to config file */
    wubu_proton_dxvk_config_write(prefix_id, DEFAULT_DXVK_CONFIG);
    wubu_proton_dxvk_set_async(prefix_id, ui->dxvk_async);
    wubu_proton_dxvk_set_hud(prefix_id, ui->dxvk_hud_enabled, ui->dxvk_hud_options);
    wubu_proton_dxvk_set_nvapi_hack(prefix_id, ui->dxvk_nvapi_hack);
    wubu_proton_dxvk_set_present_mode(prefix_id, ui->dxvk_present_mode_mailbox);
    wubu_proton_dxvk_set_memory_limits(prefix_id, ui->dxvk_max_device_memory, ui->dxvk_max_shared_memory);

    return 0;
}
