/* wubu_proton_exec.c -- Proton execution / launch subsystem.
 *
 * Self-contained module extracted from wubu_proton.c: run_wine_command,
 * winecmd/regedit/winetricks, game launch, GE-proton install + version.
 * Uses the shared g_proton state + Proton API via wubu_proton_internal.h.
 * Minimal includes.
 */

#include "../egojar.h"
#include "../egojar_net.h"
#include "wubu_proton_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

/* Resolve the prefix for a launch. NULL/empty id means "default". The
 * default prefix is resolved-or-created through the real prefix machinery
 * (wubu_proton_create_prefix -> wineboot -u), so a desktop PE launch never
 * runs against a NULL prefix. The manager is initialized on first use. */
static const ProtonPrefix *resolve_prefix(const char *prefix_id) {
    if (!g_proton.prefixes_path[0]) wubu_proton_init();
    const char *id = (prefix_id && prefix_id[0]) ? prefix_id : "default";
    const ProtonPrefix *p = wubu_proton_get_prefix(id);
    if (!p) {
        /* Only the default prefix is auto-created; an unknown named prefix
         * is an honest failure, not a silent substitute. */
        if (strcmp(id, "default") != 0) return NULL;
        if (wubu_proton_create_prefix("default", "Default", PROTON_VERSION_DEFAULT) != 0)
            return NULL;
        p = wubu_proton_get_prefix("default");
    }
    return p;
}

/* Find an executable in $PATH (returns a static buffer or NULL). Plain
 * "wine64" is not in PATH on every distro (Ubuntu ships /usr/bin/wine and
 * keeps wine64 under /usr/lib/wine), so the launcher must probe, not guess. */
static const char *find_in_path(const char *name) {
    static char buf[PROTON_PATH_MAX];
    const char *path = getenv("PATH");
    if (!path) path = "/usr/bin:/bin";
    const char *p = path;
    while (*p) {
        const char *colon = strchr(p, ':');
        size_t len = colon ? (size_t)(colon - p) : strlen(p);
        if (len > 0 && len < sizeof(buf) - strlen(name) - 2) {
            memcpy(buf, p, len);
            snprintf(buf + len, sizeof(buf) - len, "/%s", name);
            if (access(buf, X_OK) == 0) return buf;
        }
        if (!colon) break;
        p = colon + 1;
    }
    return NULL;
}

/* Locate the wine loader. Prefer wine64 in $PATH; Ubuntu keeps wine64 in
 * /usr/lib/wine (out of PATH) and /usr/bin/wine is a wrapper that demands
 * wine32 for non-PE tools like wineboot -- so probe the libexec dirs too.
 * The distro wrapper is the last resort (it picks the loader for real PEs). */
static const char *find_wine_binary(void) {
    const char *p = find_in_path("wine64");
    if (p) return p;
    static const char *libdirs[] = {
        "/usr/lib/wine", "/usr/lib64/wine", "/usr/local/lib/wine", NULL
    };
    static char libbuf[PROTON_PATH_MAX];
    for (int i = 0; libdirs[i]; i++) {
        snprintf(libbuf, sizeof(libbuf), "%s/wine64", libdirs[i]);
        if (access(libbuf, X_OK) == 0) return libbuf;
    }
    return find_in_path("wine");
}

/* Build the sandboxed wine command line for a resolved prefix + user argv
 * (argv[0] is the program to run under wine). One owner for the command
 * shape: run_wine_command execs it, tests assert on it. Returns argc, -1 on
 * a bad prefix or overflow. */
int wubu_proton_build_wine_cmd(const char *prefix_id, const char *const argv[],
                               const char **out, int max) {
    const ProtonPrefix *prefix = resolve_prefix(prefix_id);
    if (!prefix || !argv || !argv[0] || max < 64) return -1;

    int argc = 0;
    out[argc++] = "bwrap";
    out[argc++] = "--die-with-parent";
    out[argc++] = "--new-session";

    /* Bind mounts */
    out[argc++] = "--ro-bind"; out[argc++] = "/usr";   out[argc++] = "/usr";
    out[argc++] = "--ro-bind"; out[argc++] = "/lib";   out[argc++] = "/lib";
    out[argc++] = "--ro-bind"; out[argc++] = "/lib64"; out[argc++] = "/lib64";
    out[argc++] = "--ro-bind"; out[argc++] = "/bin";   out[argc++] = "/bin";
    out[argc++] = "--ro-bind"; out[argc++] = "/sbin";  out[argc++] = "/sbin";
    out[argc++] = "--ro-bind"; out[argc++] = "/etc";   out[argc++] = "/etc";

    /* /tmp carries the staged PE image (and the X11 socket for GUI apps);
     * without this bind wine cannot even open the program it was told to run. */
    out[argc++] = "--bind"; out[argc++] = "/tmp"; out[argc++] = "/tmp";
    out[argc++] = "--proc"; out[argc++] = "/proc";
    out[argc++] = "--dev";  out[argc++] = "/dev";

    /* Bind wine prefix */
    out[argc++] = "--bind";
    out[argc++] = prefix->path;
    out[argc++] = prefix->path;

    /* Bind game directory if in install_path */
    if (prefix->steam_app_id[0]) {
        static char game_path[PROTON_PATH_MAX];
        snprintf(game_path, sizeof(game_path), "%s/steamapps/common", g_proton.steamapps_path);
        out[argc++] = "--ro-bind";
        out[argc++] = game_path;
        out[argc++] = game_path;
    }

    /* Bind home for configs */
    const char *home = getenv("HOME");
    if (home) {
        out[argc++] = "--bind";
        out[argc++] = home;
        out[argc++] = home;
    }

    /* wineserver keeps its socket under /run/user/<uid> (wine 9 on Ubuntu
     * computes that path from the uid, NOT from XDG_RUNTIME_DIR) and ABORTS
     * -- "free(): invalid pointer" / "double free or corruption", SIGABRT --
     * when it cannot create it. Without a rw bind of that directory every
     * sandboxed launch dies.
     *
     * Measured, both ways: relying on the ambient XDG_RUNTIME_DIR made this
     * gate depend on whoever started make (set by a desktop session -> pass;
     * unset from cron/CI/a bare shell -> fail), and pointing wine at a
     * private runtime dir (under /tmp or under the prefix) STILL aborted,
     * because wine insists on /run/user/<uid>. So: resolve the session
     * runtime dir from the environment or from the uid, bind it rw, and pass
     * it through explicitly. No session dir at all is an honest failure, not
     * something we can paper over. */
    char session_rt[PROTON_PATH_MAX];
    const char *xdg_rt = getenv("XDG_RUNTIME_DIR");
    if (!(xdg_rt && xdg_rt[0] && access(xdg_rt, F_OK) == 0)) {
        snprintf(session_rt, sizeof(session_rt), "/run/user/%u", (unsigned)getuid());
        xdg_rt = access(session_rt, F_OK) == 0 ? session_rt : NULL;
    }
    if (xdg_rt) {
        out[argc++] = "--bind";
        out[argc++] = xdg_rt;
        out[argc++] = xdg_rt;
        out[argc++] = "--setenv";
        out[argc++] = "XDG_RUNTIME_DIR";
        out[argc++] = xdg_rt;
    }

    /* GPU access (only when the host actually has a render node) */
    if (access("/dev/dri", F_OK) == 0) {
        out[argc++] = "--dev-bind";
        out[argc++] = "/dev/dri";
        out[argc++] = "/dev/dri";
    }

    /* Environment */
    out[argc++] = "--setenv"; out[argc++] = "WINEPREFIX"; out[argc++] = prefix->path;

    out[argc++] = "--setenv";
    out[argc++] = "WINEDLLOVERRIDES";
    out[argc++] = prefix->dll_overrides[0] != '\0' ? prefix->dll_overrides : "d3d11=n,b;dxgi=n;d3d10=n,b;d3d10_1=n,b;d3d10core=n,b";

    if (prefix->esync) {
        out[argc++] = "--setenv"; out[argc++] = "WINEESYNC"; out[argc++] = "1";
    }
    if (prefix->fsync) {
        out[argc++] = "--setenv"; out[argc++] = "WINEFSYNC"; out[argc++] = "1";
    }
    if (prefix->dxvk_async) {
        out[argc++] = "--setenv"; out[argc++] = "DXVK_ASYNC"; out[argc++] = "1";
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

    out[argc++] = "--setenv"; out[argc++] = "STEAM_COMPAT_DATA_PATH"; out[argc++] = prefix->path;
    out[argc++] = "--setenv"; out[argc++] = "PROTON_PATH"; out[argc++] = proton_path;

    /* Wine executable: wine64 when the host ships it (PATH or libexec),
     * otherwise the distro wrapper (picks the 64-bit loader for real PEs). */
    const char *wine_bin = find_wine_binary();
    if (!wine_bin) return -1;   /* no wine on this host -- honest failure */

    out[argc++] = "--";
    out[argc++] = wine_bin;

    /* User command (argv[0] = the PE/exe path, then its arguments) */
    for (int i = 0; argv[i] && argc < max - 2; i++) {
        out[argc++] = argv[i];
    }
    out[argc] = NULL;
    return argc;
}

static int run_wine_command(const char *prefix_id, const char *const argv[], bool wait) {
    const char *bwrap_argv[128];
    int argc = wubu_proton_build_wine_cmd(prefix_id, argv, bwrap_argv, 128);
    if (argc < 0) {
        if (getenv("WUBU_PROTON_TRACE"))
            fprintf(stderr, "[proton] cannot build the command (prefix=%s)\n",
                    prefix_id ? prefix_id : "?");
        return -1;
    }
    /* WUBU_PROTON_TRACE=1 prints the exact sandbox command and the child's
     * exit status: a PE launch has many moving parts (bwrap binds, WINEPREFIX,
     * wine64 vs wrapper) and "it did not run" is not a diagnosis. */
    const int trace = getenv("WUBU_PROTON_TRACE") != NULL;
    if (trace) {
        fprintf(stderr, "[proton] exec:");
        for (int i = 0; i < argc; ++i) fprintf(stderr, " %s", bwrap_argv[i]);
        fprintf(stderr, "\n");
    }

    pid_t pid = fork();
    if (pid == 0) {
        /* Best-effort: join the cgroup the launcher staged for this foreign
         * process (hosted PE executor sets WUBU_PE_CGROUP). */
        const char *cg = getenv("WUBU_PE_CGROUP");
        if (cg && cg[0]) {
            char procs[PROTON_PATH_MAX];
            snprintf(procs, sizeof(procs), "%s/cgroup.procs", cg);
            FILE *f = fopen(procs, "w");
            if (f) { fprintf(f, "%d", (int)getpid()); fclose(f); }
        }
        execvp("bwrap", (char *const *)bwrap_argv);
        _exit(127);
    } else if (pid > 0) {
        if (wait) {
            int status;
            waitpid(pid, &status, 0);
            int rc = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
            if (trace) fprintf(stderr, "[proton] child exited rc=%d\n", rc);
            return rc;
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

/* Launch exe_path under wine in the given prefix and WAIT for it to exit.
 * exe_path is ALWAYS argv[0] of the wine command (the program to run);
 * `argv` supplies optional extra arguments (NULL-terminated, may be NULL).
 * prefix_id NULL/"" resolves-or-creates the default prefix. Returns the
 * program's exit status, or -1 when the launch could not be set up. */
int wubu_proton_launch_with_prefix(const char *exe_path, const char *prefix_id, char *const argv[]) {
    if (!exe_path || !exe_path[0]) return -1;
    const char *full[32];
    int n = 0;
    full[n++] = exe_path;
    if (argv) {
        for (int i = 0; argv[i] && n < 30; i++) full[n++] = argv[i];
    }
    full[n] = NULL;
    return run_wine_command(prefix_id, full, true);
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
