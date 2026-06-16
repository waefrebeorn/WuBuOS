/*
 * wubu_ct_bwrap.c  --  WuBuOS Bubblewrap Container Runtime (Hosted Demo)
 *
 * Cell 340/391: Container execution via bubblewrap for hosted Wayland demo.
 * No chroot required  --  uses host filesystem with bind mounts.
 * Shares Wayland socket so GUI apps (dsda-doom) create native Wayland toplevels.
 * WM tracks external toplevels via PID for taskbar/focus management.
 *
 * Uses shared wubu_ct_wait/kill/state from wubu_host_exec.c
 */

#define _GNU_SOURCE
#include "wubu_host_exec.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>

/* -- Internal: Build bwrap command line ------------------------------ */

static void add_gpu_binds_to_args(char **argv, int *argc, int max_args) {
    if (*argc >= max_args - 14) return;
    
    argv[(*argc)++] = "--ro-bind";
    argv[(*argc)++] = "/dev/dri";
    argv[(*argc)++] = "/dev/dri";
    
    argv[(*argc)++] = "--ro-bind";
    argv[(*argc)++] = "/dev/nvidia0";
    argv[(*argc)++] = "/dev/nvidia0";
    
    argv[(*argc)++] = "--ro-bind";
    argv[(*argc)++] = "/dev/nvidiactl";
    argv[(*argc)++] = "/dev/nvidiactl";
    
    argv[(*argc)++] = "--ro-bind";
    argv[(*argc)++] = "/dev/nvidia-uvm";
    argv[(*argc)++] = "/dev/nvidia-uvm";
    
    argv[(*argc)++] = "--ro-bind";
    argv[(*argc)++] = "/dev/nvidia-modeset";
    argv[(*argc)++] = "/dev/nvidia-modeset";
    
    argv[(*argc)++] = "--bind";
    argv[(*argc)++] = "/dev/shm";
    argv[(*argc)++] = "/dev/shm";
}

static int build_bwrap_args(WubuCt *ct, char **argv, int max_args) {
    int argc = 0;
    
    argv[argc++] = "bwrap";
    
    /* Basic isolation: private namespaces */
    argv[argc++] = "--unshare-all";
    argv[argc++] = "--share-net";      /* Need network for pulseaudio/etc */
    argv[argc++] = "--die-with-parent";
    
    /* Filesystem: ro-bind host root, then overlay binds */
    argv[argc++] = "--ro-bind";
    argv[argc++] = "/";
    argv[argc++] = "/";
    
    /* Procfs for ps/top inside container */
    argv[argc++] = "--proc";
    argv[argc++] = "/proc";
    
    /* Devpts for terminal */
    argv[argc++] = "--dev";
    argv[argc++] = "/dev";
    
    /* GPU passthrough - directly add to bwrap args */
    if (ct->gpu_passthrough) {
        add_gpu_binds_to_args(argv, &argc, max_args);
    }
    
    /* Wayland socket  --  critical for GUI apps */
    const char *wayland_display = getenv("WAYLAND_DISPLAY");
    if (wayland_display) {
        char sock_path[512];
        const char *xdg_runtime = getenv("XDG_RUNTIME_DIR");
        if (xdg_runtime) {
            snprintf(sock_path, sizeof(sock_path), "%s/%s", xdg_runtime, wayland_display);
            argv[argc++] = "--ro-bind";
            argv[argc++] = sock_path;
            argv[argc++] = sock_path;
        }
    }
    /* Also bind the whole runtime dir for pulseaudio, etc. */
    const char *xdg_runtime_dir = getenv("XDG_RUNTIME_DIR");
    if (xdg_runtime_dir) {
        argv[argc++] = "--bind";
        argv[argc++] = (char*)xdg_runtime_dir;
        argv[argc++] = (char*)xdg_runtime_dir;
    }
    
    /* X11 socket for XWayland apps */
    argv[argc++] = "--bind";
    argv[argc++] = "/tmp/.X11-unix";
    argv[argc++] = "/tmp/.X11-unix";
    
    /* Custom bind mounts from container config */
    for (int i = 0; i < ct->n_binds && argc < max_args - 4; i++) {
        argv[argc++] = ct->binds[i].readonly ? "--ro-bind" : "--bind";
        argv[argc++] = ct->binds[i].host;
        argv[argc++] = ct->binds[i].guest;
    }
    
    /* Environment variables */
    for (int i = 0; i < WUBU_CT_MAX_ENV && ct->envp[i] && argc < max_args - 2; i++) {
        argv[argc++] = "--setenv";
        argv[argc++] = ct->envp[i];
    }
    
    /* Working directory */
    argv[argc++] = "--chdir";
    argv[argc++] = "/";
    
    /* Command */
    for (int i = 0; i < WUBU_CT_MAX_ARGS && ct->argv[i] && argc < max_args - 1; i++) {
        argv[argc++] = ct->argv[i];
    }
    
    argv[argc] = NULL;
    return argc;
}

/* -- Container Start (bwrap fork + exec) ----------------------------- */

int wubu_ct_start_bwrap(WubuCt *ct) {
    if (!ct || ct->state == CT_RUNNING) return -1;
    if (!ct->argv[0]) return -1;
    
    /* bwrap doesn't need chroot rootfs  --  use host fs with binds */
    if (!ct->root[0]) {
        strncpy(ct->root, "/", WUBU_CT_ROOT_MAX - 1);
    }
    
    ct->state = CT_STARTING;
    
    /* bwrap version doesn't need per-container Styx socket or chroot-specific GPU binds.
     * Styx is for 9P namespace - not needed for simple GUI apps.
     * GPU binds are handled directly in bwrap args. */
    
    /* Build bwrap command */
    char *bwrap_argv[512];
    int argc = build_bwrap_args(ct, bwrap_argv, 512);
    
    /* Fork */
    pid_t pid = fork();
    if (pid < 0) {
        ct->state = CT_FAILED;
        return -1;
    }
    
    if (pid == 0) {
        /* CHILD: exec bwrap */
        execvp("bwrap", bwrap_argv);
        _exit(127);
    }
    
    /* PARENT */
    ct->pid = pid;
    ct->state = CT_RUNNING;
    
    return 0;
}

/* -- Preset: Bwrap Native Container --------------------------------- */

WubuCt *wubu_ct_bwrap_native(const char *name) {
    WubuCt *ct = wubu_ct_create(name, "/", CT_NATIVE);
    if (!ct) return NULL;
    
    ct->net_enabled = true;
    return ct;
}

/* -- Preset: Bwrap FreeDoom Container ------------------------------- */

WubuCt *wubu_ct_bwrap_freedoom(const char *name) {
    WubuCt *ct = wubu_ct_bwrap_native(name);
    if (!ct) return NULL;
    
    ct->gpu_passthrough = true;
    
    /* FreeDoom needs Wayland display + audio */
    const char *wayland_disp = getenv("WAYLAND_DISPLAY");
    if (wayland_disp) {
        char env[256];
        snprintf(env, sizeof(env), "WAYLAND_DISPLAY=%s", wayland_disp);
        wubu_ct_add_env(ct, env);
    }
    wubu_ct_add_env(ct, "SDL_VIDEODRIVER=wayland");
    wubu_ct_add_env(ct, "SDL_AUDIODRIVER=pulseaudio");
    
    /* Command: dsda-doom with freedoom.wad */
    char *args[] = {
        "/usr/games/dsda-doom",
        "-iwad", "/usr/share/games/doom/freedoom1.wad",
        "-geometry", "1024x768",
        "-window",
        NULL
    };
    wubu_ct_set_cmd(ct, 6, args);
    
    return ct;
}
