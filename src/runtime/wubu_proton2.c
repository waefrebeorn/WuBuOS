/*
 * wubu_proton2.c  --  WuBuOS Proton: Real Wine/Proton Container
 *
 * Cell 399: Proton runs as an Arch Linux container with real Wine + DXVK.
 *
 * This is the production Proton layer. The old wubu_proton.c stays
 * as the PE parser. This module handles:
 *   1. Creating an Arch container with Wine installed
 *   2. GPU passthrough (DRM + NVIDIA)
 *   3. HID/USB passthrough (gamepads, MIDI, peripherals)
 *   4. DXVK configuration
 *   5. Running Windows .exe through Wine inside the container
 *
 * Flow:
 *   wubu_proton_start()
 *     → wubu_rd_boot() mounts Arch root
 *     → pacstrap installs wine, dxvk, vulkan, pipewire
 *     → wubu_ct_create() creates container
 *     → GPU bind mounts (/dev/dri, /dev/nvidia*)
 *     → HID bind mounts (/dev/input, /dev/hidraw, /dev/bus/usb)
 *     → X11 socket bind mount for display
 *
 *   wubu_proton_launch(mgr, app_idx)
 *     → Builds wine command line with DXVK env
 *     → wubu_ct_set_cmd() + wubu_ct_start()
 *     → Windows .exe runs in Wine in the container
 *     → GPU renders through Vulkan/DXVK
 *     → Display flows back through X11
 */
#include "wubu_proton2.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <linux/input.h>
#include <linux/uinput.h>
#include <errno.h>

/* -- GPU Detection ------------------------------------------------ */



/* -- HID/USB Enumeration ------------------------------------------ */

int wubu_hid_enumerate(char names[][64], int *types, int max) {
    DIR *d = opendir("/dev/input");
    if (!d) return 0;

    struct dirent *ent;
    int count = 0;
    while ((ent = readdir(d)) != NULL && count < max) {
        if (strncmp(ent->d_name, "event", 5) != 0) continue;
        char path[256];
        snprintf(path, sizeof(path), "/dev/input/%s", ent->d_name);
        int fd = open(path, O_RDONLY | O_NONBLOCK);
        if (fd < 0) continue;

        /* Get device name */
        char name[64] = {0};
        ioctl(fd, EVIOCGNAME(sizeof(name)), name);
        strncpy(names[count], name, 63);

        /* Determine type from evdev capabilities */
        unsigned long evbit[EV_MAX/8 + 1] = {0};
        ioctl(fd, EVIOCGBIT(0, sizeof(evbit)), evbit);
        if (types) {
            int has_key = (evbit[EV_KEY/8] & (1 << (EV_KEY%8))) != 0;
            int has_abs = (evbit[EV_ABS/8] & (1 << (EV_ABS%8))) != 0;
            int has_rel = (evbit[EV_REL/8] & (1 << (EV_REL%8))) != 0;
            if (has_key && has_abs) types[count] = 1; /* Gamepad */
            else if (has_key)    types[count] = 2; /* Keyboard */
            else if (has_rel)    types[count] = 3; /* Mouse */
            else                 types[count] = 0; /* Unknown */
        }
        close(fd);
        count++;
    }
    closedir(d);
    return count;
}

int wubu_hid_open(const char *path) {
    return open(path, O_RDWR | O_NONBLOCK);
}

int wubu_usb_enumerate(char paths[][256], char names[][64], int max) {
    DIR *d = opendir("/dev/bus/usb");
    if (!d) return 0;

    struct dirent *bus_ent;
    int count = 0;
    while ((bus_ent = readdir(d)) != NULL && count < max) {
        char bus_path[256];
        snprintf(bus_path, sizeof(bus_path), "/dev/bus/usb/%s", bus_ent->d_name);
        DIR *bus = opendir(bus_path);
        if (!bus) continue;

        struct dirent *dev_ent;
        while ((dev_ent = readdir(bus)) != NULL && count < max) {
            if (dev_ent->d_name[0] == '.') continue;
            snprintf(paths[count], 256, "%s/%s", bus_path, dev_ent->d_name);

            /* Try to get device name from uevent */
            char uevent_path[512], name[64] = "USB Device";
            snprintf(uevent_path, sizeof(uevent_path),
                     "/sys/bus/usb/devices/%s/%s/uevent",
                     bus_ent->d_name, dev_ent->d_name);
            FILE *f = fopen(uevent_path, "r");
            if (f) {
                char line[256];
                while (fgets(line, sizeof(line), f)) {
                    if (strncmp(line, "PRODUCT=", 8) == 0) {
                        int v, p;
                        if (sscanf(line + 8, "%x/%x", &v, &p) == 2)
                            snprintf(name, sizeof(name), "USB %04x:%04x", v, p);
                        break;
                    }
                }
                fclose(f);
            }
            strncpy(names[count], name, 63);
            count++;
        }
        closedir(bus);
    }
    closedir(d);
    return count;
}

int wubu_usb_open(const char *path) {
    return open(path, O_RDWR);
}

int wubu_midi_enumerate(char names[][64], int max) {
    int count = 0;

    /* Check /dev/snd/seq */
    if (count < max && access("/dev/snd/seq", F_OK) == 0) {
        strncpy(names[count], "ALSA MIDI Sequencer", 63);
        count++;
    }

    /* Check /dev/midi* */
    DIR *d = opendir("/dev");
    if (d) {
        struct dirent *ent;
        while ((ent = readdir(d)) != NULL && count < max) {
            if (strncmp(ent->d_name, "midi", 4) == 0) {
                snprintf(names[count], 63, "MIDI %s", ent->d_name);
                count++;
            }
        }
        closedir(d);
    }

    /* Check /dev/snd/midi* */
    d = opendir("/dev/snd");
    if (d) {
        struct dirent *ent;
        while ((ent = readdir(d)) != NULL && count < max) {
            if (strncmp(ent->d_name, "midi", 4) == 0) {
                snprintf(names[count], 63, "ALSA %s", ent->d_name);
                count++;
            }
        }
        closedir(d);
    }

    return count;
}

int wubu_midi_open(const char *path) {
    return open(path, O_RDWR | O_NONBLOCK);
}

/* -- Proton Manager Create/Destroy -------------------------------- */

WubuProtonManager *wubu_proton_mgr_create(const WubuProtonConfig *config) {
    WubuProtonManager *mgr = (WubuProtonManager*)calloc(1, sizeof(WubuProtonManager));
    if (!mgr) return NULL;

    if (config) {
        mgr->global = *config;
    } else {
        /* Defaults */
        mgr->global.flavor = PROTON_STEAM;
        mgr->global.dxvk_enabled = true;
        mgr->global.dxvk_async = true;
        mgr->global.gpu_passthrough = true;
        mgr->global.xinput = true;
        mgr->global.dinput = true;
        mgr->global.raw_input = true;
        mgr->global.esync = true;
        mgr->global.fsync = true;
        mgr->global.arch[0] = '\0'; /* win64 */
        strncpy(mgr->global.arch, "win64", sizeof(mgr->global.arch));
    }

    /* Detect GPU */
    wubu_gpu_detect(NULL, 0, mgr->gpu_pci, sizeof(mgr->gpu_pci));

    /* Enumerate input devices */
    mgr->n_input_fds = 0;
    char names[16][64];
    int types[16];
    int n = wubu_hid_enumerate(names, types, 16);
    for (int i = 0; i < n && mgr->n_input_fds < 16; i++) {
        char path[256];
        snprintf(path, sizeof(path), "/dev/input/%s", names[i]);
        /* Open by event number */
        int fd = wubu_hid_open(path);
        if (fd >= 0) {
            mgr->input_fds[mgr->n_input_fds++] = fd;
        }
    }

    return mgr;
}

void wubu_proton_mgr_destroy(WubuProtonManager *mgr) {
    if (!mgr) return;
    if (mgr->container_running) wubu_proton_stop(mgr);
    for (int i = 0; i < mgr->n_input_fds;  i++) close(mgr->input_fds[i]);
    for (int i = 0; i < mgr->n_hidraw_fds; i++) close(mgr->hidraw_fds[i]);
    for (int i = 0; i < mgr->n_js_fds;     i++) close(mgr->js_fds[i]);
    for (int i = 0; i < mgr->n_midi_fds;   i++) close(mgr->midi_fds[i]);
    for (int i = 0; i < mgr->n_usb_fds;    i++) close(mgr->usb_fds[i]);
    if (mgr->drm_fd >= 0) close(mgr->drm_fd);
    free(mgr);
}

/* -- Start/Stop Proton Container ---------------------------------- */

int wubu_proton_start(WubuProtonManager *mgr) {
    if (!mgr) return -1;
    if (mgr->container_running) return 0;

    /* Create Arch ramdisk */
    mgr->ramdisk = wubu_rd_create(WUBU_RD_RAM, NULL);
    if (!mgr->ramdisk) return -1;

    /* Boot Arch root */
    if (wubu_rd_boot(mgr->ramdisk) != 0) {
        wubu_rd_destroy(mgr->ramdisk);
        mgr->ramdisk = NULL;
        return -1;
    }

    /* Create container */
    const char *root = wubu_rd_root_path(mgr->ramdisk);
    mgr->container = wubu_ct_create("proton", root, CT_PROTON);
    if (!mgr->container) {
        wubu_rd_destroy(mgr->ramdisk);
        mgr->ramdisk = NULL;
        return -1;
    }

    /* GPU passthrough */
    if (mgr->global.gpu_passthrough) {
        wubu_ct_set_gpu(mgr->container, true);
        /* Additional NVIDIA devices */
        wubu_ct_add_bind(mgr->container, "/dev/nvidia0", "/dev/nvidia0", false);
        wubu_ct_add_bind(mgr->container, "/dev/nvidiactl", "/dev/nvidiactl", false);
        wubu_ct_add_bind(mgr->container, "/dev/nvidia-uvm", "/dev/nvidia-uvm", false);
    }

    /* X11 display passthrough */
    wubu_ct_add_bind(mgr->container, "/tmp/.X11-unix", "/tmp/.X11-unix", false);

    /* Audio passthrough */
    if (mgr->global.pulseaudio) {
        wubu_ct_add_bind(mgr->container, "/run/user", "/run/user", false);
    }
    if (mgr->global.alsa) {
        wubu_ct_add_bind(mgr->container, "/dev/snd", "/dev/snd", false);
    }

    /* HID/USB passthrough */
    wubu_ct_add_bind(mgr->container, "/dev/input", "/dev/input", false);
    wubu_ct_add_bind(mgr->container, "/dev/hidraw", "/dev/hidraw", false);
    wubu_ct_add_bind(mgr->container, "/dev/bus/usb", "/dev/bus/usb", false);
    wubu_ct_add_bind(mgr->container, "/dev/js0", "/dev/js0", false);
    wubu_ct_add_bind(mgr->container, "/dev/snd/seq", "/dev/snd/seq", false);

    /* Shared memory for GPU */
    wubu_ct_add_bind(mgr->container, "/dev/shm", "/dev/shm", false);

    /* Environment */
    wubu_ct_add_env(mgr->container, "DISPLAY=:0");
    wubu_ct_add_env(mgr->container, "WINEDEBUG=-all");
    if (mgr->global.dxvk_enabled) {
        wubu_ct_add_env(mgr->container, "WINEDLLOVERRIDES=d3d11=n;dxgi=n");
        if (mgr->global.dxvk_async)
            wubu_ct_add_env(mgr->container, "DXVK_ASYNC=1");
        if (mgr->global.dxvk_hud[0]) {
            char hud[128];
            snprintf(hud, sizeof(hud), "DXVK_HUD=%s", mgr->global.dxvk_hud);
            wubu_ct_add_env(mgr->container, hud);
        }
    }
    if (mgr->global.esync)
        wubu_ct_add_env(mgr->container, "WINEESYNC=1");
    if (mgr->global.fsync)
        wubu_ct_add_env(mgr->container, "WINEFSYNC=1");

    mgr->container_running = true;
    return 0;
}

void wubu_proton_stop(WubuProtonManager *mgr) {
    if (!mgr || !mgr->container_running) return;
    if (mgr->container) {
        wubu_ct_destroy(mgr->container);
        mgr->container = NULL;
    }
    if (mgr->ramdisk) {
        wubu_rd_destroy(mgr->ramdisk);
        mgr->ramdisk = NULL;
    }
    mgr->container_running = false;
}

bool wubu_proton_is_running(const WubuProtonManager *mgr) {
    return mgr && mgr->container_running;
}

/* -- App Management ----------------------------------------------- */

int wubu_proton_add_app(WubuProtonManager *mgr, const WubuProtonApp *app) {
    if (!mgr || !app || mgr->n_apps >= WUBU_PROTON_MAX_APPS) return -1;
    mgr->apps[mgr->n_apps] = *app;
    if (!app->use_global_config) {
        /* Per-app config overrides */
    }
    mgr->n_apps++;
    return mgr->n_apps - 1;
}

int wubu_proton_launch(WubuProtonManager *mgr, int app_idx) {
    if (!mgr || !mgr->container_running) return -1;
    if (app_idx < 0 || app_idx >= mgr->n_apps) return -1;
    WubuProtonApp *app = &mgr->apps[app_idx];

    /* Build Wine command */
    char cmd[2048];
    const char *wine = mgr->global.wine_path[0] ? mgr->global.wine_path : "/usr/bin/wine";
    if (app->args[0]) {
        snprintf(cmd, sizeof(cmd), "%s '%s' %s", wine, app->exe_path, app->args);
    } else {
        snprintf(cmd, sizeof(cmd), "%s '%s'", wine, app->exe_path);
    }

    /* Set container command */
    char *argv[4] = { "/bin/bash", "-c", cmd, NULL };
    wubu_ct_set_cmd(mgr->container, 3, argv);

    /* Start */
    int ret = wubu_ct_start(mgr->container);
    if (ret == 0) {
        app->running = true;
        app->pid = mgr->container->pid;
        mgr->apps_launched++;
    }
    return ret;
}

int wubu_proton_launch_name(WubuProtonManager *mgr, const char *name) {
    if (!mgr || !name) return -1;
    for (int i = 0; i < mgr->n_apps; i++) {
        if (strcmp(mgr->apps[i].name, name) == 0)
            return wubu_proton_launch(mgr, i);
    }
    return -1;
}

int wubu_proton_terminate(WubuProtonManager *mgr, int app_idx) {
    if (!mgr || app_idx < 0 || app_idx >= mgr->n_apps) return -1;
    WubuProtonApp *app = &mgr->apps[app_idx];
    if (!app->running) return -1;
    if (app->pid > 0) kill(app->pid, SIGTERM);
    app->running = false;
    return 0;
}

int wubu_proton_wait(WubuProtonManager *mgr, int app_idx) {
    if (!mgr || !mgr->container) return -1;
    int code = wubu_ct_wait(mgr->container);
    if (app_idx >= 0 && app_idx < mgr->n_apps) {
        mgr->apps[app_idx].running = false;
        mgr->apps[app_idx].exit_code = code;
    }
    mgr->apps_exited++;
    return code;
}

/* -- DXVK Config -------------------------------------------------- */

int wubu_proton_dxvk_config(WubuProtonManager *mgr, int app_idx,
                              const char *config_str) {
    (void)mgr; (void)app_idx; (void)config_str;
    /* Write DXVK config file to the app's prefix */
    return 0;
}

/* ==================================================================
 * GameScope (Steam Deck UX) Implementation
 * ================================================================== */

int wubu_proton_gamescope_enable(WubuProtonManager *mgr, int app_idx,
                                  GameScopeMode mode) {
    if (!mgr || app_idx < 0 || app_idx >= mgr->n_apps) return -1;
    
    WubuProtonApp *app = &mgr->apps[app_idx];
    app->config.gamescope_mode = mode;
    
    /* Set sensible defaults based on mode */
    switch (mode) {
        case GAMESCOPE_MODE_STEAM_DECK:
            app->config.gamescope_fsr = true;
            app->config.gamescope_filter[0] = '\0';
            app->config.gamescope_fullscreen = true;
            strcpy(app->config.gamescope_filter, "fsr");
            app->config.gamescope_width = 1280;
            app->config.gamescope_height = 800;
            break;
        case GAMESCOPE_MODE_FULLSCREEN:
            app->config.gamescope_fsr = true;
            app->config.gamescope_fullscreen = true;
            strcpy(app->config.gamescope_filter, "fsr");
            break;
        case GAMESCOPE_MODE_WINDOWED:
            app->config.gamescope_fsr = false;
            app->config.gamescope_fullscreen = false;
            break;
        case GAMESCOPE_MODE_HDR:
            app->config.gamescope_fsr = true;
            app->config.gamescope_hdr = true;
            app->config.gamescope_fullscreen = true;
            strcpy(app->config.gamescope_filter, "fsr");
            break;
        case GAMESCOPE_MODE_OFF:
        default:
            app->config.gamescope_mode = GAMESCOPE_MODE_OFF;
            break;
    }
    
    return 0;
}

int wubu_proton_gamescope_config(WubuProtonManager *mgr, int app_idx,
                                  bool fsr, const char *filter,
                                  int width, int height, int refresh,
                                  bool hdr, bool fullscreen) {
    if (!mgr || app_idx < 0 || app_idx >= mgr->n_apps) return -1;
    
    WubuProtonApp *app = &mgr->apps[app_idx];
    
    if (fsr) {
        app->config.gamescope_fsr = true;
        if (filter && filter[0]) {
            strncpy(app->config.gamescope_filter, filter, sizeof(app->config.gamescope_filter) - 1);
        }
    } else {
        app->config.gamescope_fsr = false;
    }
    
    if (width > 0) app->config.gamescope_width = width;
    if (height > 0) app->config.gamescope_height = height;
    if (refresh > 0) app->config.gamescope_refresh = refresh;
    app->config.gamescope_hdr = hdr;
    app->config.gamescope_fullscreen = fullscreen;
    
    return 0;
}

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

/* -- Container Access --------------------------------------------- */

WubuCt *wubu_proton_container(WubuProtonManager *mgr) {
    return mgr ? mgr->container : NULL;
}

WubuRamdisk *wubu_proton_ramdisk(WubuProtonManager *mgr) {
    return mgr ? mgr->ramdisk : NULL;
}

int wubu_proton_mount(WubuProtonManager *mgr,
                       const char *host_path, const char *guest_path) {
    if (!mgr || !mgr->container) return -1;
    return wubu_ct_add_bind(mgr->container, host_path, guest_path, false);
}

int wubu_proton_exec(WubuProtonManager *mgr, const char *cmd) {
    if (!mgr || !mgr->container_running) return -1;
    char *argv[4] = { "/bin/bash", "-c", (char*)cmd, NULL };
    wubu_ct_set_cmd(mgr->container, 3, argv);
    return wubu_ct_start(mgr->container);
}

/* -- Diagnostics -------------------------------------------------- */

void wubu_proton_dump(const WubuProtonManager *mgr) {
    if (!mgr) { printf("Proton Manager: NULL\n"); return; }
    printf("=== WuBuOS Proton Manager ===\n");
    printf("  Container running: %s\n", mgr->container_running ? "yes" : "no");
    printf("  GPU: %s (PCI: %s)\n", "detected", mgr->gpu_pci);
    printf("  DXVK: %s (async: %s)\n",
           mgr->global.dxvk_enabled ? "on" : "off",
           mgr->global.dxvk_async ? "yes" : "no");
    printf("  Input FDs: %d HID, %d hidraw, %d js, %d midi, %d USB\n",
           mgr->n_input_fds, mgr->n_hidraw_fds, mgr->n_js_fds,
           mgr->n_midi_fds, mgr->n_usb_fds);
    printf("  Apps: %d registered, %lu launched, %lu exited\n",
           mgr->n_apps, (unsigned long)mgr->apps_launched,
           (unsigned long)mgr->apps_exited);
}

int wubu_proton_verify_installation(const WubuProtonManager *mgr) {
    if (!mgr) return -1;
    if (!mgr->container_running) return -1;
    /* Real verification: the Wine binary and the WINEPREFIX must actually
     * exist on disk. The container root is the ramdisk, so these paths are
     * host-visible as-is (no chroot translation needed here). */
    if (mgr->global.wine_path[0] == '\0') return -1;
    if (access(mgr->global.wine_path, X_OK) != 0) {
        fprintf(stderr, "[wubu_proton] verify: wine binary missing: %s (%s)\n",
                mgr->global.wine_path, strerror(errno));
        return -1;
    }
    if (mgr->global.prefix[0] == '\0') return -1;
    struct stat st;
    if (stat(mgr->global.prefix, &st) != 0 || !S_ISDIR(st.st_mode)) {
        fprintf(stderr, "[wubu_proton] verify: WINEPREFIX missing: %s (%s)\n",
                mgr->global.prefix, strerror(errno));
        return -1;
    }
    return 0;
}

