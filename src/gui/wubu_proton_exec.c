/* wubu_proton_exec.c -- Proton execution / launch subsystem.
 *
 * Self-contained module extracted from wubu_proton.c: run_wine_command,
 * winecmd/regedit/winetricks, game launch, GE-proton install + version.
 * Uses the shared g_proton state + Proton API via wubu_proton_internal.h.
 * Minimal includes.
 */

#include "wubu_proton_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <errno.h>

static int run_wine_command(const char *prefix_id, const char *const argv[], bool wait) {
    const ProtonPrefix *prefix = wubu_proton_get_prefix(prefix_id);
    if (!prefix) return -1;
    
    /* Build bubblewrap command for wine */
    const char *bwrap_argv[64];
    int argc = 0;
    
    bwrap_argv[argc++] = "bwrap";
    bwrap_argv[argc++] = "--die-with-parent";
    bwrap_argv[argc++] = "--new-session";
    
    /* Bind mounts */
    bwrap_argv[argc++] = "--ro-bind";
    bwrap_argv[argc++] = "/usr";
    bwrap_argv[argc++] = "/usr";
    
    bwrap_argv[argc++] = "--ro-bind";
    bwrap_argv[argc++] = "/lib";
    bwrap_argv[argc++] = "/lib";
    
    bwrap_argv[argc++] = "--ro-bind";
    bwrap_argv[argc++] = "/lib64";
    bwrap_argv[argc++] = "/lib64";
    
    bwrap_argv[argc++] = "--ro-bind";
    bwrap_argv[argc++] = "/bin";
    bwrap_argv[argc++] = "/bin";
    
    bwrap_argv[argc++] = "--ro-bind";
    bwrap_argv[argc++] = "/sbin";
    bwrap_argv[argc++] = "/sbin";
    
    bwrap_argv[argc++] = "--ro-bind";
    bwrap_argv[argc++] = "/etc";
    bwrap_argv[argc++] = "/etc";
    
    /* Bind wine prefix */
    bwrap_argv[argc++] = "--bind";
    bwrap_argv[argc++] = prefix->path;
    bwrap_argv[argc++] = prefix->path;
    
    /* Bind game directory if in install_path */
    if (prefix->steam_app_id[0]) {
        char game_path[PROTON_PATH_MAX];
        snprintf(game_path, sizeof(game_path), "%s/steamapps/common", g_proton.steamapps_path);
        bwrap_argv[argc++] = "--ro-bind";
        bwrap_argv[argc++] = game_path;
        bwrap_argv[argc++] = game_path;
    }
    
    /* Bind home for configs */
    const char *home = getenv("HOME");
    if (home) {
        bwrap_argv[argc++] = "--bind";
        bwrap_argv[argc++] = home;
        bwrap_argv[argc++] = home;
    }
    
    /* GPU access */
    bwrap_argv[argc++] = "--dev-bind";
    bwrap_argv[argc++] = "/dev/dri";
    bwrap_argv[argc++] = "/dev/dri";
    
    /* Environment */
    bwrap_argv[argc++] = "--setenv";
    bwrap_argv[argc++] = "WINEPREFIX";
    bwrap_argv[argc++] = prefix->path;
    
    bwrap_argv[argc++] = "--setenv";
    bwrap_argv[argc++] = "WINEDLLOVERRIDES";
    bwrap_argv[argc++] = prefix->dll_overrides[0] != '\0' ? prefix->dll_overrides : "d3d11=n,b;dxgi=n;d3d10=n,b;d3d10_1=n,b;d3d10core=n,b";
    
    if (prefix->esync) {
        bwrap_argv[argc++] = "--setenv";
        bwrap_argv[argc++] = "WINEESYNC";
        bwrap_argv[argc++] = "1";
    }
    if (prefix->fsync) {
        bwrap_argv[argc++] = "--setenv";
        bwrap_argv[argc++] = "WINEFSYNC";
        bwrap_argv[argc++] = "1";
    }
    if (prefix->dxvk_async) {
        bwrap_argv[argc++] = "--setenv";
        bwrap_argv[argc++] = "DXVK_ASYNC";
        bwrap_argv[argc++] = "1";
    }
    
    /* Proton version path */
    const char *proton_path = NULL;
    switch (prefix->proton_version) {
        case PROTON_VERSION_GE_LATEST:
            proton_path = g_proton.proton_ge_path[0] ? g_proton.proton_ge_path : "/usr/share/proton-ge";
            break;
        case PROTON_VERSION_EXPERIMENTAL:
            proton_path = "/usr/share/proton-experimental";
            break;
        default:
            proton_path = "/usr/share/proton";
            break;
    }
    
    bwrap_argv[argc++] = "--setenv";
    bwrap_argv[argc++] = "STEAM_COMPAT_DATA_PATH";
    bwrap_argv[argc++] = prefix->path;
    
    bwrap_argv[argc++] = "--setenv";
    bwrap_argv[argc++] = "PROTON_PATH";
    bwrap_argv[argc++] = proton_path;
    
    /* Wine executable */
    bwrap_argv[argc++] = "--";
    bwrap_argv[argc++] = "wine64";
    
    /* User command */
    for (int i = 0; argv[i] && argc < 60; i++) {
        bwrap_argv[argc++] = argv[i];
    }
    bwrap_argv[argc++] = NULL;

        pid_t pid = fork();
        if (pid == 0) {
            execvp("bwrap", (char *const *)bwrap_argv);
            _exit(127);
        } else if (pid > 0) {
        if (wait) {
            int status;
            waitpid(pid, &status, 0);
            return WEXITSTATUS(status);
        }
        return 0;
    }
    return -1;
}

int wubu_proton_winecmd(const char *prefix_id, const char *const argv[]) {
    return run_wine_command(prefix_id, argv, true);
}

int wubu_proton_regedit(const char *prefix_id, const char *reg_file) {
    const char *argv[] = {"regedit", reg_file, NULL};
    return wubu_proton_winecmd(prefix_id, argv);
}

int wubu_proton_winetricks(const char *prefix_id, const char *verb) {
    const char *argv[] = {"winetricks", "-q", verb, NULL};
    return wubu_proton_winecmd(prefix_id, argv);
}

int wubu_proton_launch_game(const char *game_id) {
    const ProtonGame *game = wubu_proton_get_game(game_id);
    if (!game) return -1;
    
    const ProtonPrefix *prefix = wubu_proton_get_prefix(game->prefix_id[0] ? game->prefix_id : "default");
    if (!prefix) {
        /* Create default prefix */
        wubu_proton_create_prefix("default", "Default", PROTON_VERSION_GE_LATEST);
        prefix = wubu_proton_get_prefix("default");
    }
    if (!prefix) return -1;
    
    /* Build command */
    char exe_path[PROTON_PATH_MAX];
    if (game->is_non_steam && game->custom_exe[0]) {
        strncpy(exe_path, game->custom_exe, sizeof(exe_path) - 1);
    } else {
        snprintf(exe_path, sizeof(exe_path), "%s/%s", game->install_path, game->exe_path);
    }
    
    const char *argv[32];
    int argc = 0;
    argv[argc++] = exe_path;
    
    if (game->launch_options[0]) {
        /* Parse launch options */
        char *copy = strdup(game->launch_options);
        char *tok = strtok(copy, " ");
        while (tok && argc < 30) {
            argv[argc++] = tok;
            tok = strtok(NULL, " ");
        }
        free(copy);
    }
    argv[argc++] = NULL;
    
    return run_wine_command(prefix->id, argv, false);
}

int wubu_proton_launch_with_prefix(const char *exe_path, const char *prefix_id, char *const argv[]) {
    return run_wine_command(prefix_id, argv, false);
}

int wubu_proton_ge_install_latest(void) {
    /* Would download latest Proton-GE from GitHub releases */
    /* For now, check if already installed */
    const char *paths[] = {
        "/usr/share/proton-ge",
        "/opt/proton-ge",
        NULL
    };
    
    for (int i = 0; paths[i]; i++) {
        if (file_exists(paths[i])) {
            strncpy(g_proton.proton_ge_path, paths[i], sizeof(g_proton.proton_ge_path) - 1);
            return 0;
        }
    }
    return -1;
}

int wubu_proton_ge_get_version(char *out_version, size_t size) {
    if (!g_proton.proton_ge_path[0]) return -1;
    
    char version_file[PROTON_PATH_MAX];
    snprintf(version_file, sizeof(version_file), "%s/version", g_proton.proton_ge_path);
    
    FILE *f = fopen(version_file, "r");
    if (!f) return -1;
    
    if (fgets(out_version, size, f)) {
        out_version[strcspn(out_version, "\n")] = '\0';
    }
    fclose(f);
    return 0;
}

const char *wubu_proton_ge_get_path(void) {
    return g_proton.proton_ge_path[0] ? g_proton.proton_ge_path : NULL;
}
