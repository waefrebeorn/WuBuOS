/* wubu_proton2_gamescope.c -- WuBuOS proton2: gamescope/wine launch-command builder.
 * Extracted from wubu_proton2.c (separable leaf). Self-contained: builds a
 * launch command string from WubuProtonConfig (gamescope/wine). C11, minimal includes.
 */
#include "wubu_proton2.h"

#include <string.h>
#include <stdio.h>

int wubu_proton_gamescope_cmd(WubuProtonManager *mgr, int app_idx,
                               char *out_cmd, size_t size) {
    if (!mgr || !out_cmd || size == 0) return -1;
    if (app_idx < 0 || app_idx >= mgr->n_apps) return -1;
    
    WubuProtonApp *app = &mgr->apps[app_idx];
    WubuProtonConfig *cfg = app->use_global_config ? &mgr->global : &app->config;
    
    if (cfg->gamescope_mode == GAMESCOPE_MODE_OFF) {
        /* No gamescope - just return the wine command */
        const char *wine = cfg->wine_path[0] ? cfg->wine_path : "/usr/bin/wine";
        if (app->args[0]) {
            snprintf(out_cmd, size, "%s '%s' %s", wine, app->exe_path, app->args);
        } else {
            snprintf(out_cmd, size, "%s '%s'", wine, app->exe_path);
        }
        return 0;
    }
    
    /* Build gamescope command */
    char gamescope_args[1024] = {0};
    int pos = 0;
    
    /* Basic gamescope */
    pos += snprintf(gamescope_args + pos, sizeof(gamescope_args) - pos, "gamescope");
    
    /* Fullscreen mode */
    if (cfg->gamescope_fullscreen) {
        pos += snprintf(gamescope_args + pos, sizeof(gamescope_args) - pos, " -f");
    } else {
        pos += snprintf(gamescope_args + pos, sizeof(gamescope_args) - pos, " -b");
    }
    
    /* Resolution (internal render resolution) */
    if (cfg->gamescope_width > 0 && cfg->gamescope_height > 0) {
        pos += snprintf(gamescope_args + pos, sizeof(gamescope_args) - pos,
                        " -r %dx%d", cfg->gamescope_width, cfg->gamescope_height);
    }
    
    /* Output resolution (display resolution) - use desktop if not specified */
    if (cfg->width > 0 && cfg->height > 0) {
        pos += snprintf(gamescope_args + pos, sizeof(gamescope_args) - pos,
                        " -W %d -H %d", cfg->width, cfg->height);
    }
    
    /* Refresh rate */
    if (cfg->gamescope_refresh > 0) {
        pos += snprintf(gamescope_args + pos, sizeof(gamescope_args) - pos,
                        " -R %d", cfg->gamescope_refresh);
    }
    
    /* FSR upscaling */
    if (cfg->gamescope_fsr) {
        pos += snprintf(gamescope_args + pos, sizeof(gamescope_args) - pos, " -U");
        if (cfg->gamescope_filter[0]) {
            pos += snprintf(gamescope_args + pos, sizeof(gamescope_args) - pos,
                            " --filter %s", cfg->gamescope_filter);
        }
    }
    
    /* HDR */
    if (cfg->gamescope_hdr) {
        pos += snprintf(gamescope_args + pos, sizeof(gamescope_args) - pos, " --hdr");
    }
    
    /* Force grab cursor for fullscreen games */
    if (cfg->gamescope_fullscreen) {
        pos += snprintf(gamescope_args + pos, sizeof(gamescope_args) - pos, " -g");
    }
    
    /* Extra options */
    if (cfg->gamescope_opts[0]) {
        pos += snprintf(gamescope_args + pos, sizeof(gamescope_args) - pos, " %s", cfg->gamescope_opts);
    }
    
    /* Wine command */
    const char *wine = cfg->wine_path[0] ? cfg->wine_path : "/usr/bin/wine";
    if (app->args[0]) {
        pos += snprintf(gamescope_args + pos, sizeof(gamescope_args) - pos,
                        " -- %s '%s' %s", wine, app->exe_path, app->args);
    } else {
        pos += snprintf(gamescope_args + pos, sizeof(gamescope_args) - pos,
                        " -- %s '%s'", wine, app->exe_path);
    }
    
    /* Copy to output */
    strncpy(out_cmd, gamescope_args, size - 1);
    out_cmd[size - 1] = '\0';
    
    return 0;
}
