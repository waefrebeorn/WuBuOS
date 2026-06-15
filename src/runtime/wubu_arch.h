/*
 * wubu_arch.h  --  WuBuOS Arch Linux Bootstrap for Container Roots
 *
 * Cell 390: Arch as the NT-era kernel layer.
 *
 * Philosophy: Arch base rips through Linux drivers.
 * We don't rewrite drivers  --  we USE them. Arch gives us:
 *   - Preemptive multitasking via Linux kernel
 *   - DRM/KMS for native display
 *   - NVIDIA + AMD drivers via pacman
 *   - SteamOS/Proton compat layer
 *   - PulseAudio/PipeWire for audio
 *   - NetworkManager for connectivity
 *
 * WuBuOS IS a WSL2 distro on Windows.
 * WuBuOS IS a native Arch root on Linux.
 * Same binary. Same containers. Same 9P namespace.
 *
 * This module creates and manages Arch rootfs trees
 * that serve as chroot targets for wubu_host_exec containers.
 */
#ifndef WUBU_ARCH_H
#define WUBU_ARCH_H

#include <stdint.h>
#include <stdbool.h>

/* -- Arch Root Configuration -------------------------------------- */

#define WUBU_ARCH_ROOT_DEFAULT  "/var/wubu/roots/arch-base"
#define WUBU_ARCH_MIRROR_DEFAULT "https://archlinux.org/packages/"

typedef enum {
    WUBU_ARCH_IDLE      = 0,   /* No root exists */
    WUBU_ARCH_BOOTSTRAP = 1,   /* pacstrap in progress */
    WUBU_ARCH_READY     = 2,   /* Root ready for containers */
    WUBU_ARCH_UPDATING  = 3,   /* pacman -Syu in progress */
    WUBU_ARCH_FAILED    = 4,   /* Bootstrap/update failed */
} WubuArchState;

typedef struct {
    char     root_path[512];    /* Root filesystem path */
    char     mirror[512];       /* pacman mirror URL */
    WubuArchState state;        /* Current state */
    bool     has_gui;           /* X11/Wayland installed */
    bool     has_audio;         /* PulseAudio/PipeWire installed */
    bool     has_gpu_drivers;   /* mesa/nvidia installed */
    bool     has_steam;         /* Steam runtime installed */
    uint64_t root_size_mb;      /* Root filesystem size in MB */
} WubuArchRoot;

/* -- Arch Bootstrap ----------------------------------------------- */

/*
 * Bootstrap a minimal Arch rootfs using pacstrap.
 * Creates: base, base-devel, linux-firmware, dbus, systemd
 * Then: configures mirror, locale, network, hostname
 *
 * root_path: target directory (created if not exists)
 * mirror:    pacman mirror URL (NULL = default)
 * progress:  callback for progress reporting (NULL = silent)
 *
 * Returns: 0 on success, -1 on failure
 */
int wubu_arch_bootstrap(const char *root_path, const char *mirror,
                        void (*progress)(const char *msg));

/*
 * Install additional packages into an Arch root.
 * Runs: pacstrap <root> <pkg1> <pkg2> ...
 *
 * packages: space-separated package list
 * Returns: 0 on success, -1 on failure
 */
int wubu_arch_install(const char *root_path, const char *packages);

/*
 * Run pacman inside the Arch root.
 * Runs: chroot <root> pacman <args>
 *
 * args: pacman arguments (e.g., "-Syu", "-S prboom-plus")
 * Returns: exit code of pacman
 */
int wubu_arch_pacman(const char *root_path, const char *args);

/*
 * Update Arch root (pacman -Syu).
 * Returns: 0 on success, -1 on failure
 */
int wubu_arch_update(const char *root_path);

/* -- Arch Service Management -------------------------------------- */

/*
 * Enable a systemd service inside Arch root.
 * Runs: chroot <root> systemctl enable <service>
 */
int wubu_arch_enable_service(const char *root_path, const char *service);

/*
 * Configure Arch root for WuBuOS:
 *   - Set hostname to "wubuos"
 *   - Set locale to en_US.UTF-8
 *   - Enable NetworkManager
 *   - Enable dbus
 *   - Create /etc/resolv.conf
 *   - Create wubu user
 *
 * Called after bootstrap, before first container start.
 */
int wubu_arch_configure(const char *root_path);

/* -- Arch Presets ------------------------------------------------- */

/*
 * Bootstrap a GUI-capable Arch root.
 * Adds: xorg, mesa, pulseaudio, fonts, dbus
 * For: FreeDoom, Steam, desktop apps
 */
int wubu_arch_bootstrap_gui(const char *root_path, const char *mirror);

/*
 * Bootstrap a Steam-capable Arch root.
 * Adds: everything from GUI + steam, mangohud, gamemode
 * For: Proton games, SteamOS compat
 */
int wubu_arch_bootstrap_steam(const char *root_path, const char *mirror);

/* -- Arch Root Info ----------------------------------------------- */

/*
 * Check if an Arch root exists and is valid.
 * Validates: /usr/bin/pacman exists, /etc/arch-release exists
 */
bool wubu_arch_root_valid(const char *root_path);

/*
 * Get Arch root state.
 */
WubuArchRoot *wubu_arch_root_info(const char *root_path);

/*
 * Destroy Arch root info struct.
 */
void wubu_arch_root_free(WubuArchRoot *info);

#endif /* WUBU_ARCH_H */
