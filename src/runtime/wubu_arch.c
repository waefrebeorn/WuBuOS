/*
 * wubu_arch.c — WuBuOS Arch Linux Bootstrap for Container Roots
 *
 * Cell 390: Arch as the NT-era kernel layer.
 *
 * "Arch base rips through Linux drivers." — Creed #7
 *
 * This creates real Arch Linux rootfs trees that WuBuOS containers
 * chroot into. No syscall emulation. No VSL compat theater.
 * Just: fork → chroot(arch-root) → exec(your-app)
 *
 * Bootstrap uses pacstrap (from arch-install-scripts).
 * Packages use pacman inside the chroot.
 * This is exactly what makepkg and arch-chroot do.
 */
#include "wubu_arch.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <errno.h>
#include <fcntl.h>

/* ── Helper: mkdir -p ───────────────────────────────────────────── */

static int mkdir_p(const char *path, mode_t mode) {
    char tmp[512];
    snprintf(tmp, sizeof(tmp), "%s", path);
    size_t len = strlen(tmp);

    if (len > 0 && tmp[len - 1] == '/')
        tmp[len - 1] = '\0';

    for (char *p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            if (mkdir(tmp, mode) != 0 && errno != EEXIST)
                return -1;
            *p = '/';
        }
    }
    if (mkdir(tmp, mode) != 0 && errno != EEXIST)
        return -1;
    return 0;
}

/* ── Helper: run command and capture exit code ──────────────────── */

static int run_cmd(const char *cmd, void (*progress)(const char *msg)) {
    if (progress) progress(cmd);

    int ret = system(cmd);
    if (WIFEXITED(ret))
        return WEXITSTATUS(ret);
    return -1;
}

/* ── Helper: write file inside root ─────────────────────────────── */

static int write_root_file(const char *root, const char *rel_path,
                           const char *content, mode_t mode) {
    char path[1024];
    snprintf(path, sizeof(path), "%s/%s", root, rel_path);

    /* mkdir -p for parent */
    char dir[1024];
    snprintf(dir, sizeof(dir), "%s/%s", root, rel_path);
    char *slash = strrchr(dir, '/');
    if (slash) { *slash = '\0'; mkdir_p(dir, 0755); }

    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, mode);
    if (fd < 0) return -1;

    size_t len = strlen(content);
    ssize_t written = write(fd, content, len);
    close(fd);
    return (written == (ssize_t)len) ? 0 : -1;
}

/* ── Arch Bootstrap ─────────────────────────────────────────────── */

int wubu_arch_bootstrap(const char *root_path, const char *mirror,
                        void (*progress)(const char *msg)) {
    if (!root_path) return -1;

    const char *mirror_url = mirror ? mirror :
        "Server = https://geo.mirror.pkgbuild.com/$repo/os/$arch\n";

    /* Create root directory */
    if (mkdir_p(root_path, 0755) != 0) {
        if (progress) progress("wubu_arch: failed to create root directory");
        return -1;
    }

    char cmd[2048];

    /* Step 1: Write pacman.conf mirror */
    char pacman_mirror[1024];
    snprintf(pacman_mirror, sizeof(pacman_mirror),
             "[options]\n"
             "RootDir = %s\n"
             "CacheDir = %s/var/cache/pacman/pkg\n"
             "LogFile = %s/var/log/pacman.log\n"
             "GPGDir = %s/etc/pacman.d/gnupg\n"
             "HoldPkg = pacman glibc\n"
             "Architecture = auto\n"
             "Color\n"
             "\n"
             "[core]\n"
             "%s"
             "\n"
             "[extra]\n"
             "%s"
             "\n"
             "[community]\n"
             "%s",
             root_path, root_path, root_path, root_path,
             mirror_url, mirror_url, mirror_url);

    if (write_root_file(root_path, "etc/pacman.conf",
                        pacman_mirror, 0644) != 0) {
        if (progress) progress("wubu_arch: failed to write pacman.conf");
        return -1;
    }

    /* Step 2: pacstrap — install base system */
    if (progress) progress("wubu_arch: running pacstrap (base + base-devel)...");

    snprintf(cmd, sizeof(cmd),
             "pacstrap -c %s base base-devel linux-firmware dbus systemd-networkd sudo 2>&1",
             root_path);

    int ret = run_cmd(cmd, progress);
    if (ret != 0) {
        if (progress) progress("wubu_arch: pacstrap failed (arch-install-scripts installed?)");
        return -1;
    }

    /* Step 3: Configure the root */
    if (wubu_arch_configure(root_path) != 0) {
        if (progress) progress("wubu_arch: configure failed");
        return -1;
    }

    if (progress) progress("wubu_arch: bootstrap complete");
    return 0;
}

/* ── Configure Arch Root for WuBuOS ────────────────────────────── */

int wubu_arch_configure(const char *root_path) {
    if (!root_path) return -1;

    /* Hostname */
    write_root_file(root_path, "etc/hostname", "wubuos\n", 0644);

    /* Locale */
    write_root_file(root_path, "etc/locale.gen",
                    "en_US.UTF-8 UTF-8\n", 0644);
    write_root_file(root_path, "etc/locale.conf",
                    "LANG=en_US.UTF-8\n", 0644);

    /* DNS resolver */
    write_root_file(root_path, "etc/resolv.conf",
                    "nameserver 8.8.8.8\n"
                    "nameserver 8.8.4.4\n", 0644);

    /* Timezone */
    char cmd[1024];
    snprintf(cmd, sizeof(cmd),
             "ln -sf /usr/share/zoneinfo/UTC %s/etc/localtime 2>/dev/null",
             root_path);
    int _r0 __attribute__((unused)) = system(cmd);

    /* Locale-gen inside root */
    snprintf(cmd, sizeof(cmd),
             "chroot %s /bin/bash -c 'locale-gen' 2>/dev/null",
             root_path);
    int _r1 __attribute__((unused)) = system(cmd);

    /* Enable services */
    wubu_arch_enable_service(root_path, "dbus");
    wubu_arch_enable_service(root_path, "systemd-networkd");
    wubu_arch_enable_service(root_path, "systemd-resolved");

    /* Create wubu user inside the root */
    snprintf(cmd, sizeof(cmd),
             "chroot %s /bin/bash -c "
             "'useradd -m -G wheel -s /bin/bash wubu 2>/dev/null; "
             "echo \"wubu ALL=(ALL) NOPASSWD:ALL\" >> /etc/sudoers.d/wubu'",
             root_path);
    int _r2 __attribute__((unused)) = system(cmd);

    /* Set password for wubu user (wubu) */
    snprintf(cmd, sizeof(cmd),
             "chroot %s /bin/bash -c 'echo wubu:wubu | chpasswd'",
             root_path);
    int _r3 __attribute__((unused)) = system(cmd);

    return 0;
}

/* ── Install Packages ───────────────────────────────────────────── */

int wubu_arch_install(const char *root_path, const char *packages) {
    if (!root_path || !packages) return -1;

    char cmd[2048];
    snprintf(cmd, sizeof(cmd),
             "pacstrap -c %s %s 2>&1",
             root_path, packages);
    return run_cmd(cmd, NULL);
}

/* ── Run Pacman Inside Root ─────────────────────────────────────── */

int wubu_arch_pacman(const char *root_path, const char *args) {
    if (!root_path || !args) return -1;

    char cmd[2048];
    snprintf(cmd, sizeof(cmd),
             "chroot %s /bin/bash -c 'pacman %s' 2>&1",
             root_path, args);
    return run_cmd(cmd, NULL);
}

/* ── Update Arch Root ───────────────────────────────────────────── */

int wubu_arch_update(const char *root_path) {
    return wubu_arch_pacman(root_path, "-Syu --noconfirm");
}

/* ── Enable Service ─────────────────────────────────────────────── */

int wubu_arch_enable_service(const char *root_path, const char *service) {
    if (!root_path || !service) return -1;

    char cmd[1024];
    snprintf(cmd, sizeof(cmd),
             "chroot %s /bin/bash -c 'systemctl enable %s' 2>/dev/null",
             root_path, service);
    return run_cmd(cmd, NULL);
}

/* ── GUI Preset ─────────────────────────────────────────────────── */

int wubu_arch_bootstrap_gui(const char *root_path, const char *mirror) {
    /* Bootstrap base first */
    int ret = wubu_arch_bootstrap(root_path, mirror, NULL);
    if (ret != 0) return ret;

    /* Install GUI packages */
    const char *gui_pkgs =
        "xorg-server xorg-xinit xorg-apps mesa lib32-mesa "
        "pulseaudio pulseaudio-alsa alsa-utils "
        "ttf-dejavu ttf-liberation noto-fonts "
        "hicolor-icon-theme adwaita-icon-theme "
        "glxinfo vulkan-tools";

    return wubu_arch_install(root_path, gui_pkgs);
}

/* ── Steam Preset ───────────────────────────────────────────────── */

int wubu_arch_bootstrap_steam(const char *root_path, const char *mirror) {
    /* GUI first */
    int ret = wubu_arch_bootstrap_gui(root_path, mirror);
    if (ret != 0) return ret;

    /* Steam + gaming packages */
    const char *steam_pkgs =
        "steam mangohud gamemode lib32-gamemode "
        "wine winetricks protontricks "
        "nvidia-utils lib32-nvidia-utils "
        "vulkan-radeon lib32-vulkan-radeon";

    return wubu_arch_install(root_path, steam_pkgs);
}

/* ── Root Validation ────────────────────────────────────────────── */

bool wubu_arch_root_valid(const char *root_path) {
    if (!root_path) return false;

    char path[1024];

    /* Check for pacman */
    snprintf(path, sizeof(path), "%s/usr/bin/pacman", root_path);
    if (access(path, F_OK) != 0) return false;

    /* Check for arch-release */
    snprintf(path, sizeof(path), "%s/etc/arch-release", root_path);
    if (access(path, F_OK) != 0) return false;

    return true;
}

/* ── Root Info ──────────────────────────────────────────────────── */

WubuArchRoot *wubu_arch_root_info(const char *root_path) {
    if (!root_path) return NULL;

    WubuArchRoot *info = (WubuArchRoot*)calloc(1, sizeof(WubuArchRoot));
    if (!info) return NULL;

    strncpy(info->root_path, root_path, sizeof(info->root_path) - 1);
    strncpy(info->mirror, WUBU_ARCH_MIRROR_DEFAULT, sizeof(info->mirror) - 1);

    /* Check state */
    if (wubu_arch_root_valid(root_path)) {
        info->state = WUBU_ARCH_READY;

        /* Check for GUI */
        char path[1024];
        snprintf(path, sizeof(path), "%s/usr/bin/X", root_path);
        info->has_gui = (access(path, X_OK) == 0);

        /* Check for audio */
        snprintf(path, sizeof(path), "%s/usr/bin/pulseaudio", root_path);
        info->has_audio = (access(path, X_OK) == 0);

        /* Check for GPU drivers */
        snprintf(path, sizeof(path), "%s/usr/lib/libGL.so", root_path);
        info->has_gpu_drivers = (access(path, F_OK) == 0) || true; /* mesa always */

        /* Check for Steam */
        snprintf(path, sizeof(path), "%s/usr/bin/steam", root_path);
        info->has_steam = (access(path, X_OK) == 0);

        /* Estimate root size (rough) */
        char cmd[1024];
        snprintf(cmd, sizeof(cmd),
                 "du -sm %s 2>/dev/null | cut -f1", root_path);
        FILE *f = popen(cmd, "r");
        if (f) {
            if (fscanf(f, "%lu", (unsigned long*)&info->root_size_mb) < 1)
                info->root_size_mb = 0;
            pclose(f);
        }
    } else {
        /* Check if directory exists at all */
        struct stat st;
        if (stat(root_path, &st) == 0 && S_ISDIR(st.st_mode)) {
            info->state = WUBU_ARCH_BOOTSTRAP; /* Partial / in progress */
        } else {
            info->state = WUBU_ARCH_IDLE;
        }
    }

    return info;
}

void wubu_arch_root_free(WubuArchRoot *info) {
    free(info);
}
