/**
 * wubu_deploy.c - WuBuOS Multi-Target Deployment Implementation
 * 
 * Build scripts for bare metal, WSL2, OCI, and macOS AVF targets.
 * Uses system tools: mkinitcpio, tar, docker/buildah, xcodebuild
 */

#include "wubu_deploy.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <errno.h>
#include <dirent.h>
#include <time.h>

/* ============================================================
 * Internal Helpers
 * ============================================================ */

bool wubu_deploy_create_rootfs(const char* rootfs_dir, const char* wubu_binary) {
    /* Create directory structure */
    const char* dirs[] = {
        "bin", "sbin", "etc", "proc", "sys", "dev", "run", "tmp",
        "var", "var/log", "var/lib", "var/cache", "root", "home",
        "home/wubu", "usr/bin", "usr/sbin", "usr/lib", "usr/share",
        "lib", "lib64", "opt", "mnt", "media", "srv", NULL
    };
    for (int i = 0; dirs[i]; i++) {
        char path[512];
        snprintf(path, sizeof(path), "%s/%s", rootfs_dir, dirs[i]);
        mkdir_p(path, 0755);
    }

    /* Copy wubu binary */
    char dst[512];
    snprintf(dst, sizeof(dst), "%s/usr/bin/wubu", rootfs_dir);
    if (!copy_file(wubu_binary, dst)) return false;
    chmod(dst, 0755);

    /* Create symlinks in rootfs */
    char link_path[512];
    snprintf(link_path, sizeof(link_path), "%s/bin/wubu", rootfs_dir);
    symlink("/usr/bin/wubu", link_path);
    snprintf(link_path, sizeof(link_path), "%s/sbin/wubu", rootfs_dir);
    symlink("/usr/bin/wubu", link_path);

    /* Essential files */
    char file_path[512];
    snprintf(file_path, sizeof(file_path), "%s/etc/passwd", rootfs_dir);
    write_file(file_path, 
        "root:x:0:0:root:/root:/bin/sh\n"
        "wubu:x:1000:1000:WuBuOS User:/home/wubu:/bin/sh\n");
    snprintf(file_path, sizeof(file_path), "%s/etc/group", rootfs_dir);
    write_file(file_path,
        "root:x:0:\n"
        "wubu:x:1000:\n"
        "video:x:44:\n"
        "input:x:999:\n");
    snprintf(file_path, sizeof(file_path), "%s/etc/hostname", rootfs_dir);
    write_file(file_path, "wubuos\n");
    snprintf(file_path, sizeof(file_path), "%s/etc/hosts", rootfs_dir);
    write_file(file_path,
        "127.0.0.1\tlocalhost\n"
        "::1\t\tlocalhost\n"
        "127.0.1.1\twubuos.localdomain\twubuos\n");
    snprintf(file_path, sizeof(file_path), "%s/etc/resolv.conf", rootfs_dir);
    write_file(file_path,
        "nameserver 1.1.1.1\n"
        "nameserver 8.8.8.8\n");
    snprintf(file_path, sizeof(file_path), "%s/etc/fstab", rootfs_dir);
    write_file(file_path,
        "# <fs>\t<mount>\t<type>\t<opts>\t<dump>\t<pass>\n"
        "proc\t/proc\tproc\tdefaults\t0\t0\n"
        "sysfs\t/sys\tsysfs\tdefaults\t0\t0\n"
        "tmpfs\t/run\ttmpfs\tdefaults\t0\t0\n"
        "tmpfs\t/tmp\ttmpfs\tdefaults\t0\t0\n");
    snprintf(file_path, sizeof(file_path), "%s/etc/profile", rootfs_dir);
    write_file(file_path,
        "export PATH=/usr/bin:/usr/sbin:/bin:/sbin\n"
        "export HOME=/home/wubu\n"
        "export USER=wubu\n"
        "export TERM=xterm-256color\n"
        "[ -f /etc/bashrc ] && . /etc/bashrc\n");
    snprintf(file_path, sizeof(file_path), "%s/etc/bashrc", rootfs_dir);
    write_file(file_path,
        "alias ll='ls -la'\n"
        "alias wubu='wubu -w 1024 768'\n"
        "PS1='\\[\\033[1;32m\\]wubuos\\[\\033[0m\\]:\\[\\033[1;34m\\]\\w\\[\\033[0m\\]\\$ '\n");

    /* Init script for bare metal */
    snprintf(file_path, sizeof(file_path), "%s/init", rootfs_dir);
    write_file(file_path,
        "#!/bin/sh\n"
        "mount -t proc proc /proc\n"
        "mount -t sysfs sysfs /sys\n"
        "mount -t devtmpfs devtmpfs /dev\n"
        "mount -t tmpfs tmpfs /run\n"
        "mkdir -p /dev/pts /dev/shm\n"
        "mount -t devpts devpts /dev/pts\n"
        "mount -t tmpfs tmpfs /dev/shm\n"
        "\n"
        "# Load kernel modules\n"
        "modprobe virtio_gpu 2>/dev/null\n"
        "modprobe virtio_input 2>/dev/null\n"
        "modprobe i915 2>/dev/null\n"
        "modprobe amdgpu 2>/dev/null\n"
        "\n"
        "# Setup networking\n"
        "ip link set lo up\n"
        "ip link set eth0 up 2>/dev/null || true\n"
        "udhcpc -i eth0 -b 2>/dev/null || true\n"
        "\n"
        "# Start WuBuOS\n"
        "exec /usr/bin/wubu -w 1024 768\n");
    chmod(file_path, 0755);

    /* WSL-specific init */
    snprintf(file_path, sizeof(file_path), "%s/etc/wsl.conf", rootfs_dir);
    write_file(file_path,
        "[boot]\n"
        "systemd=true\n"
        "\n"
        "[automount]\n"
        "enabled=true\n"
        "root=/mnt/\n"
        "options=\"metadata,uid=1000,gid=1000,umask=022,fmask=111\"\n"
        "\n"
        "[network]\n"
        "generateResolvConf=true\n"
        "generateHosts=true\n"
        "\n"
        "[interop]\n"
        "enabled=true\n"
        "appendWindowsPath=true\n"
        "\n"
        "[user]\n"
        "default=wubu\n");

    return true;
}

bool wubu_deploy_generate_limine_conf(const char* output_path, const char* kernel_cmdline) {
    char content[4096];
    snprintf(content, sizeof(content),
        "# WuBuOS Limine Configuration\n"
        "# Generated by wubu_deploy\n"
        "\n"
        "timeout: 5\n"
        "default_entry: WuBuOS\n"
        "\n"
        ":WuBuOS\n"
        "    protocol: linux\n"
        "    kernel_path: /boot/vmlinuz\n"
        "    cmdline: %s\n"
        "    module_path: /boot/initramfs.img\n"
        "\n"
        ":WuBuOS (Recovery)\n"
        "    protocol: linux\n"
        "    kernel_path: /boot/vmlinuz\n"
        "    cmdline: %s single\n"
        "    module_path: /boot/initramfs.img\n"
        "\n"
        ":Reboot\n"
        "    protocol: reboot\n"
        "\n"
        ":Power Off\n"
        "    protocol: poweroff\n",
        kernel_cmdline ? kernel_cmdline : "quiet loglevel=3",
        kernel_cmdline ? kernel_cmdline : "quiet loglevel=3");
    return write_file(output_path, content);
}

/* ============================================================
 * WSL Config Generation
 * ============================================================ */

bool wubu_deploy_generate_wsl_conf(const char* output_path, bool systemd) {
    char content[2048];
    snprintf(content, sizeof(content),
        "# WuBuOS WSL Configuration\n"
        "# Generated by wubu_deploy\n"
        "\n"
        "[boot]\n"
        "systemd=%s\n"
        "\n"
        "[automount]\n"
        "enabled=true\n"
        "root=/mnt/\n"
        "options=\"metadata,uid=1000,gid=1000,umask=022,fmask=111\"\n"
        "\n"
        "[network]\n"
        "generateResolvConf=true\n"
        "generateHosts=true\n"
        "\n"
        "[interop]\n"
        "enabled=true\n"
        "appendWindowsPath=true\n"
        "\n"
        "[user]\n"
        "default=wubu\n",
        systemd ? "true" : "false");
    return write_file(output_path, content);
}

/* ============================================================
 * Dockerfile Generation
 * ============================================================ */

bool wubu_deploy_generate_dockerfile(const wubu_oci_config_t* config, const char* output_path) {
    const char* base = config->base_image ? config->base_image : "scratch";
    const char* entrypoint = config->entrypoint ? config->entrypoint : "/usr/bin/wubu";
    
    char content[8192];
    char* p = content;
    p += snprintf(p, content + sizeof(content) - p,
        "# WuBuOS OCI Container Image\n"
        "# Generated by wubu_deploy\n"
        "# Build: docker build -t %s .\n"
        "\n"
        "FROM %s\n"
        "\n",
        config->image_name ? config->image_name : "wubuos:latest",
        base);

    if (strcmp(base, "scratch") == 0) {
        p += snprintf(p, content + sizeof(content) - p,
            "# Scratch base - copy rootfs\n"
            "COPY rootfs/ /\n"
            "\n");
    } else if (strstr(base, "alpine")) {
        p += snprintf(p, content + sizeof(content) - p,
            "# Alpine base - install dependencies\n"
            "RUN apk add --no-cache \\\n"
            "    libwayland-client \\\n"
            "    libxkbcommon \\\n"
            "    mesa-dri-gallium \\\n"
            "    libdrm \\\n"
            "    libgbm \\\n"
            "    fontconfig \\\n"
            "    ttf-dejavu \\\n"
            "    ca-certificates\n"
            "\n"
            "COPY rootfs/ /\n"
            "\n");
    } else if (strstr(base, "debian") || strstr(base, "ubuntu")) {
        p += snprintf(p, content + sizeof(content) - p,
            "# Debian/Ubuntu base - install dependencies\n"
            "RUN apt-get update && apt-get install -y --no-install-recommends \\\n"
            "    libwayland-client0 \\\n"
            "    libxkbcommon0 \\\n"
            "    mesa-vulkan-drivers \\\n"
            "    libdrm2 \\\n"
            "    libgbm1 \\\n"
            "    fontconfig \\\n"
            "    fonts-dejavu-core \\\n"
            "    ca-certificates \\\n"
            "    && rm -rf /var/lib/apt/lists/*\n"
            "\n"
            "COPY rootfs/ /\n"
            "\n");
    }

    /* Environment variables */
    if (config->env_vars) {
        for (int i = 0; config->env_vars[i]; i++) {
            p += snprintf(p, content + sizeof(content) - p, "ENV %s\n", config->env_vars[i]);
        }
        p += snprintf(p, content + sizeof(content) - p, "\n");
    }

    /* Exposed ports */
    if (config->ports) {
        for (int i = 0; config->ports[i]; i++) {
            p += snprintf(p, content + sizeof(content) - p, "EXPOSE %s\n", config->ports[i]);
        }
        p += snprintf(p, content + sizeof(content) - p, "\n");
    }

    /* Volumes */
    if (config->volumes) {
        for (int i = 0; config->volumes[i]; i++) {
            p += snprintf(p, content + sizeof(content) - p, "VOLUME [\"%s\"]\n", config->volumes[i]);
        }
        p += snprintf(p, content + sizeof(content) - p, "\n");
    }

    /* User */
    p += snprintf(p, content + sizeof(content) - p,
        "# Run as non-root user\n"
        "USER 1000:1000\n"
        "WORKDIR /home/wubu\n"
        "\n");

    /* Entrypoint */
    p += snprintf(p, content + sizeof(content) - p,
        "ENTRYPOINT [\"%s\"]\n"
        "CMD [\"-w\", \"1024\", \"768\"]\n",
        entrypoint);

    return write_file(output_path, content);
}

/* ============================================================
 * macOS Entitlements Generation
 * ============================================================ */

bool wubu_deploy_generate_entitlements(const wubu_macos_config_t* config, const char* output_path) {
    char content[4096];
    snprintf(content, sizeof(content),
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n"
        "<plist version=\"1.0\">\n"
        "<dict>\n"
        "    <key>com.apple.security.app-sandbox</key>\n"
        "    <true/>\n"
        "    <key>com.apple.security.hypervisor</key>\n"
        "    <true/>\n"
        "    <key>com.apple.security.vm.networking</key>\n"
        "%s"
        "    <key>com.apple.security.files.user-selected.read-write</key>\n"
        "    <true/>\n"
        "    <key>com.apple.security.device.usb</key>\n"
        "    <true/>\n"
        "    <key>com.apple.security.device.bluetooth</key>\n"
        "    <true/>\n"
        "</dict>\n"
        "</plist>\n",
        config->rosetta ? 
        "    <key>com.apple.security.cs.allow-dyld-environment-variables</key>\n"
        "    <true/>\n"
        "    <key>com.apple.security.cs.allow-unsigned-executable-memory</key>\n"
        "    <true/>\n" : "");
    return write_file(output_path, content);
}

/* ============================================================
 * macOS Info.plist Generation
 * ============================================================ */

bool wubu_deploy_generate_infoplist(const wubu_macos_config_t* config, const char* output_path) {
    char content[4096];
    snprintf(content, sizeof(content),
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n"
        "<plist version=\"1.0\">\n"
        "<dict>\n"
        "    <key>CFBundleIdentifier</key>\n"
        "    <string>%s</string>\n"
        "    <key>CFBundleName</key>\n"
        "    <string>%s</string>\n"
        "    <key>CFBundleDisplayName</key>\n"
        "    <string>%s</string>\n"
        "    <key>CFBundleVersion</key>\n"
        "    <string>1.0.0</string>\n"
        "    <key>CFBundleShortVersionString</key>\n"
        "    <string>1.0.0</string>\n"
        "    <key>CFBundleExecutable</key>\n"
        "    <string>wubu-macos</string>\n"
        "    <key>CFBundlePackageType</key>\n"
        "    <string>APPL</string>\n"
        "    <key>CFBundleSignature</key>\n"
        "    <string>WUBU</string>\n"
        "    <key>LSMinimumSystemVersion</key>\n"
        "    <string>13.0</string>\n"
        "    <key>NSHighResolutionCapable</key>\n"
        "    <true/>\n"
        "    <key>LSApplicationCategoryType</key>\n"
        "    <string>public.app-category.utilities</string>\n"
        "</dict>\n"
        "</plist>\n",
        config->app_bundle_id ? config->app_bundle_id : "com.wubuos.app",
        config->app_name ? config->app_name : "WuBuOS",
        config->app_name ? config->app_name : "WuBuOS");
    return write_file(output_path, content);
}

/* ============================================================
 * Deployment Targets Implementation
 * ============================================================ */

bool wubu_deploy_init(void) {
    return true;
}

void wubu_deploy_shutdown(void) {
}

void wubu_deploy_get_default_baremetal(wubu_baremetal_config_t* config) {
    memset(config, 0, sizeof(*config));
    config->kernel_path = "/boot/vmlinuz-linux";
    config->initramfs_path = "/boot/initramfs-wubuos.img";
    config->limine_cfg_path = "/boot/limine.conf";
    config->output_iso = "wubuos-baremetal.iso";
    config->wubu_binary = "/home/wubu/.hermes/profiles/mind-palace/home/myseed/src/hosted/wubu";
    config->rootfs_dir = "/tmp/wubuos-rootfs";
    config->include_firmware = true;
    config->include_drivers = true;
    config->kernel_cmdline = "quiet loglevel=3 mitigations=off";
}

void wubu_deploy_get_default_wsl2(wubu_wsl2_config_t* config) {
    memset(config, 0, sizeof(*config));
    config->distro_name = "WuBuOS";
    config->rootfs_path = "/tmp/wubuos-wsl2-rootfs";
    config->output_tar = "wubuos-wsl2.tar.gz";
    config->wubu_binary = "/home/wubu/.hermes/profiles/mind-palace/home/myseed/src/hosted/wubu";
    config->default_user = "wubu";
    config->systemd = true;
    config->wsl_conf = NULL; /* Use generated */
}

void wubu_deploy_get_default_oci(wubu_oci_config_t* config) {
    memset(config, 0, sizeof(*config));
    config->image_name = "wubuos:latest";
    config->dockerfile_path = "Dockerfile";
    config->context_dir = ".";
    config->wubu_binary = "/home/wubu/.hermes/profiles/mind-palace/home/myseed/src/hosted/wubu";
    config->base_image = "scratch";
    config->entrypoint = "/usr/bin/wubu";
    config->env_vars = (const char*[]) {
        "WAYLAND_DISPLAY=wayland-0",
        "XDG_RUNTIME_DIR=/tmp/runtime-wubu",
        "HOME=/home/wubu",
        "USER=wubu",
        NULL
    };
    config->ports = (const char*[]) { "wayland", NULL };
    config->volumes = (const char*[]) { "/tmp/.X11-unix", "/run/user/1000", NULL };
    config->multi_arch = false;
}

void wubu_deploy_get_default_macos(wubu_macos_config_t* config) {
    memset(config, 0, sizeof(*config));
    config->app_bundle_id = "com.wubuos.app";
    config->app_name = "WuBuOS";
    config->kernel_path = "/home/wubu/.hermes/profiles/mind-palace/home/myseed/build/vmlinuz";
    config->initramfs_path = "/home/wubu/.hermes/profiles/mind-palace/home/myseed/build/initramfs.img";
    config->output_app = "/home/wubu/.hermes/profiles/mind-palace/home/myseed/build/WuBuOS.app";
    config->wubu_binary = "/home/wubu/.hermes/profiles/mind-palace/home/myseed/src/hosted/wubu";
    config->entitlements = "WuBuOS.entitlements";
    config->vm_memory_mb = 4096;
    config->vm_cpus = 4;
    config->gui_enabled = true;
    config->rosetta = true;
}

/* ============================================================
 * Bare Metal Build
 * ============================================================ */

bool wubu_deploy_baremetal(const wubu_baremetal_config_t* config) {
    printf("[deploy] Building bare metal target...\n");

    /* Create rootfs */
    if (!wubu_deploy_create_rootfs(config->rootfs_dir, config->wubu_binary)) {
        fprintf(stderr, "[deploy] Failed to create rootfs\n");
        return false;
    }

    /* Generate init script */
    const char* init_path = "/home/wubu/.hermes/profiles/mind-palace/home/myseed/src/init";
    /* Already created in create_rootfs */

    /* Generate limine.conf */
    if (!wubu_deploy_generate_limine_conf(config->limine_cfg_path, config->kernel_cmdline)) {
        fprintf(stderr, "[deploy] Failed to generate limine.conf\n");
        return false;
    }

    /* Copy limine.conf to rootfs */
    char limine_dst[512];
    snprintf(limine_dst, sizeof(limine_dst), "%s/boot/limine.conf", config->rootfs_dir);
    mkdir_p("/home/wubu/.hermes/profiles/mind-palace/home/myseed/src/boot", 0755);
    copy_file(config->limine_cfg_path, limine_dst);

    /* Copy kernel */
    char kernel_dst[512];
    snprintf(kernel_dst, sizeof(kernel_dst), "%s/boot/vmlinuz", config->rootfs_dir);
    if (!copy_file(config->kernel_path, kernel_dst)) {
        fprintf(stderr, "[deploy] Failed to copy kernel\n");
        return false;
    }

    /* Create initramfs using mkinitcpio or cpio */
    char cmd[1024];
    snprintf(cmd, sizeof(cmd),
        "cd %s && find . -print0 | cpio --null -ov --format=newc | gzip -9 > %s",
        config->rootfs_dir, config->initramfs_path);
    if (!run_command(cmd, NULL)) {
        fprintf(stderr, "[deploy] Failed to create initramfs\n");
        return false;
    }

    /* Copy initramfs to rootfs/boot */
    char initramfs_dst[512];
    snprintf(initramfs_dst, sizeof(initramfs_dst), "%s/boot/initramfs.img", config->rootfs_dir);
    copy_file(config->initramfs_path, initramfs_dst);

    /* Build ISO using limine */
    snprintf(cmd, sizeof(cmd),
        "limine bios-install %s && "
        "xorriso -as mkisofs -o %s "
        "-b boot/limine-bios.img -no-emul-boot -boot-load-size 4 -boot-info-table "
        "-iso-level 3 -full-iso9660-filenames "
        "%s",
        config->rootfs_dir, config->output_iso, config->rootfs_dir);
    if (!run_command(cmd, NULL)) {
        fprintf(stderr, "[deploy] Failed to build ISO\n");
        return false;
    }

    printf("[deploy] Bare metal ISO: %s\n", config->output_iso);
    return true;
}

/* ============================================================
 * WSL2 Build
 * ============================================================ */

bool wubu_deploy_wsl2(const wubu_wsl2_config_t* config) {
    printf("[deploy] Building WSL2 distro package...\n");

    /* Create rootfs */
    if (!wubu_deploy_create_rootfs(config->rootfs_path, config->wubu_binary)) {
        fprintf(stderr, "[deploy] Failed to create rootfs\n");
        return false;
    }

    /* Generate wsl.conf */
    char wsl_conf_path[512];
    snprintf(wsl_conf_path, sizeof(wsl_conf_path), "%s/etc/wsl.conf", config->rootfs_path);
    if (!wubu_deploy_generate_wsl_conf(wsl_conf_path, config->systemd)) {
        fprintf(stderr, "[deploy] Failed to generate wsl.conf\n");
        return false;
    }

    /* Create tar.gz */
    char cmd[1024];
    snprintf(cmd, sizeof(cmd),
        "cd %s && tar --numeric-owner -czf %s .",
        config->rootfs_path, config->output_tar);
    if (!run_command(cmd, NULL)) {
        fprintf(stderr, "[deploy] Failed to create tar.gz\n");
        return false;
    }

    printf("[deploy] WSL2 package: %s\n", config->output_tar);
    printf("[deploy] Install: wsl --import %s <path> %s --version 2\n", 
           config->distro_name, config->output_tar);
    return true;
}

/* ============================================================
 * OCI Build
 * ============================================================ */

bool wubu_deploy_oci(const wubu_oci_config_t* config) {
    printf("[deploy] Building OCI container image...\n");

    /* Create build context */
    char context_rootfs[512];
    snprintf(context_rootfs, sizeof(context_rootfs), "%s/rootfs", config->context_dir);
    mkdir_p(context_rootfs, 0755);

    /* Create rootfs in context */
    if (!wubu_deploy_create_rootfs(context_rootfs, config->wubu_binary)) {
        fprintf(stderr, "[deploy] Failed to create rootfs\n");
        return false;
    }

    /* Generate Dockerfile */
    char dockerfile_path[512];
    snprintf(dockerfile_path, sizeof(dockerfile_path), "%s/%s", config->context_dir, config->dockerfile_path);
    if (!wubu_deploy_generate_dockerfile(config, dockerfile_path)) {
        fprintf(stderr, "[deploy] Failed to generate Dockerfile\n");
        return false;
    }

    /* Build image */
    char cmd[1024];
    if (config->multi_arch) {
        snprintf(cmd, sizeof(cmd),
            "docker buildx build --platform linux/amd64,linux/arm64 "
            "-t %s -f %s %s",
            config->image_name, config->dockerfile_path, config->context_dir);
    } else {
        snprintf(cmd, sizeof(cmd),
            "docker build -t %s -f %s %s",
            config->image_name, config->dockerfile_path, config->context_dir);
    }
    if (!run_command(cmd, NULL)) {
        fprintf(stderr, "[deploy] Failed to build OCI image\n");
        return false;
    }

    printf("[deploy] OCI image: %s\n", config->image_name);
    return true;
}

/* ============================================================
 * macOS AVF Build
 * ============================================================ */

bool wubu_deploy_macos(const wubu_macos_config_t* config) {
    printf("[deploy] Building macOS AVF app bundle...\n");

    /* This requires macOS with Xcode. Generate the project structure. */
    char app_path[512];
    snprintf(app_path, sizeof(app_path), "%s/Contents/MacOS", config->output_app);
    char resources_path[512];
    snprintf(resources_path, sizeof(resources_path), "%s/Contents/Resources", config->output_app);
    char frameworks_path[512];
    snprintf(frameworks_path, sizeof(frameworks_path), "%s/Contents/Frameworks", config->output_app);
    
    mkdir_p(app_path, 0755);
    mkdir_p(resources_path, 0755);
    mkdir_p(frameworks_path, 0755);

    /* Generate Info.plist */
    char info_plist[512];
    snprintf(info_plist, sizeof(info_plist), "%s/Contents/Info.plist", config->output_app);
    if (!wubu_deploy_generate_infoplist(config, info_plist)) {
        fprintf(stderr, "[deploy] Failed to generate Info.plist\n");
        return false;
    }

    /* Generate entitlements */
    char entitlements_path[512];
    snprintf(entitlements_path, sizeof(entitlements_path), "%s/%s", config->output_app, config->entitlements);
    if (!wubu_deploy_generate_entitlements(config, entitlements_path)) {
        fprintf(stderr, "[deploy] Failed to generate entitlements\n");
        return false;
    }

    /* Copy kernel and initramfs to Resources */
    if (config->kernel_path && access(config->kernel_path, F_OK) == 0) {
        char dst[512];
        snprintf(dst, sizeof(dst), "%s/vmlinuz", resources_path);
        copy_file(config->kernel_path, dst);
    }
    if (config->initramfs_path && access(config->initramfs_path, F_OK) == 0) {
        char dst[512];
        snprintf(dst, sizeof(dst), "%s/initramfs.img", resources_path);
        copy_file(config->initramfs_path, dst);
    }

    /* Generate macOS launcher (Swift) */
    char launcher_path[512];
    snprintf(launcher_path, sizeof(launcher_path), "%s/wubu_launcher.swift", config->output_app);
    char launcher_code[8192];
    snprintf(launcher_code, sizeof(launcher_code),
        "// WuBuOS macOS Launcher\n"
        "// Uses Virtualization.framework to run Linux VM\n"
        "\n"
        "import Foundation\n"
        "import Virtualization\n"
        "import OSLog\n"
        "\n"
        "@main\n"
        "struct WuBuOSLauncher {\n"
        "    static func main() async {\n"
        "        let logger = Logger(subsystem: \"%s\", category: \"launcher\")\n"
        "        \n"
        "        guard let bundle = Bundle.main.resourcePath else {\n"
        "            logger.error(\"Failed to get resource path\")\n"
        "            exit(1)\n"
        "        }\n"
        "        \n"
        "        let kernelURL = URL(fileURLWithPath: \"\\(bundle)/vmlinuz\")\n"
        "        let initrdURL = URL(fileURLWithPath: \"\\(bundle)/initramfs.img\")\n"
        "        \n"
        "        let configuration = VZVirtualMachineConfiguration()\n"
        "        configuration.cpuCount = %u\n"
        "        configuration.memorySize = %llu * 1024 * 1024\n"
        "        \n"
        "        // Boot loader\n"
        "        let bootLoader = VZLinuxBootLoader(kernelURL: kernelURL)\n"
        "        bootLoader.commandLine = \"quiet loglevel=3\"\n"
        "        bootLoader.initialRamdiskURL = initrdURL\n"
        "        configuration.bootLoader = bootLoader\n"
        "        \n"
        "        // Entropy\n"
        "        configuration.entropyDevices = [VZVirtioEntropyDeviceConfiguration()]\n"
        "        \n"
        "        // Memory balloon\n"
        "        configuration.memoryBalloonDevices = [VZVirtioTraditionalMemoryBalloonDeviceConfiguration()]\n"
        "        \n"
        "        // Serial console\n"
        "        let consoleConfig = VZVirtioConsoleDeviceSerialPortConfiguration()\n"
        "        let consoleAttachment = VZFileHandleSerialPortAttachment(\n"
        "            fileHandleForReading: FileHandle.standardInput,\n"
        "            fileHandleForWriting: FileHandle.standardOutput\n"
        "        )\n"
        "        consoleConfig.attachment = consoleAttachment\n"
        "        configuration.serialPorts = [VZVirtioConsoleDeviceConfiguration(ports: [consoleConfig])]\n"
        "        \n"
        "        // Network\n"
        "        let networkConfig = VZVirtioNetworkDeviceConfiguration()\n"
        "        networkConfig.attachment = VZNATNetworkDeviceAttachment()\n"
        "        configuration.networkDevices = [networkConfig]\n"
        "        \n"
        "        // Block device (optional)\n"
        "        // let blockConfig = VZVirtioBlockDeviceConfiguration(...)\n"
        "        // configuration.storageDevices = [blockConfig]\n"
        "        \n"
        "%s"
        "        // GPU\n"
        "        if #available(macOS 14.0, *) {\n"
        "            let gpuConfig = VZVirtioGPUDeviceConfiguration()\n"
        "            gpuConfig.scanouts = [VZVirtioGPUScanoutConfiguration()]\n"
        "            configuration.graphicsDevices = [gpuConfig]\n"
        "        }\n"
        "        \n"
        "        // Rosetta\n"
        "%s"
        "        do {\n"
        "            let vm = VZVirtualMachine(configuration: configuration)\n"
        "            try await vm.start()\n"
        "            logger.info(\"WuBuOS VM started\")\n"
        "            \n"
        "            // Wait for VM to exit\n"
        "            for await _ in vm.statePublisher {\n"
        "                if case .stopped = $0 { break }\n"
        "            }\n"
        "        } catch {\n"
        "            logger.error(\"VM error: \\(error)\")\n"
        "            exit(1)\n"
        "        }\n"
        "    }\n"
        "}\n",
        config->app_bundle_id ? config->app_bundle_id : "com.wubuos.app",
        config->vm_cpus,
        config->vm_memory_mb,
        config->gui_enabled ? "" : "        // GPU disabled\n",
        config->rosetta ? 
        "        if #available(macOS 13.0, *) {\n"
        "            let rosettaConfig = VZLinuxRosettaDirectoryShareConfiguration()\n"
        "            try configuration.setRosettaDirectoryShare(rosettaConfig)\n"
        "        }" : "");

    write_file(launcher_path, launcher_code);

    /* Generate build script */
    char build_script[512];
    snprintf(build_script, sizeof(build_script), "%s/build_macos.sh", config->output_app);
    char build_code[4096];
    snprintf(build_code, sizeof(build_code),
        "#!/bin/bash\n"
        "# WuBuOS macOS Build Script\n"
        "# Run on macOS with Xcode 15+\n"
        "\n"
        "set -e\n"
        "\n"
        "APP_NAME=\"%s\"\n"
        "BUNDLE_ID=\"%s\"\n"
        "OUTPUT=\"%s\"\n"
        "\n"
        "echo \"Building WuBuOS macOS launcher...\"\n"
        "\n"
        "# Compile Swift launcher\n"
        "swiftc \\\n"
        "    -target arm64-apple-macos13.0 \\\n"
        "    -framework Foundation \\\n"
        "    -framework Virtualization \\\n"
        "    -o \"\${OUTPUT}/Contents/MacOS/wubu-macos\" \\\n"
        "    \"wubu_launcher.swift\"\n"
        "\n"
        "# Codesign\n"
        "codesign --force --deep --sign - \\\n"
        "    --entitlements \"\${OUTPUT}/%s\" \\\n"
        "    \"\${OUTPUT}\"\n"
        "\n"
        "echo \"Build complete: \${OUTPUT}\"\n"
        "echo \"Run: open \${OUTPUT}\"\n",
        config->app_name ? config->app_name : "WuBuOS",
        config->app_bundle_id ? config->app_bundle_id : "com.wubuos.app",
        config->output_app,
        config->entitlements ? config->entitlements : "WuBuOS.entitlements");

    write_file(build_script, build_code);
    chmod(build_script, 0755);

    printf("[deploy] macOS app bundle structure: %s\n", config->output_app);
    printf("[deploy] Run build script on macOS: %s\n", build_script);
    return true;
}

/* ============================================================
 * Validation
 * ============================================================ */

bool wubu_deploy_validate_baremetal(const wubu_baremetal_config_t* config) {
    return config && config->kernel_path && config->wubu_binary && config->output_iso;
}

bool wubu_deploy_validate_wsl2(const wubu_wsl2_config_t* config) {
    return config && config->distro_name && config->wubu_binary && config->output_tar;
}

bool wubu_deploy_validate_oci(const wubu_oci_config_t* config) {
    return config && config->image_name && config->wubu_binary && config->context_dir;
}

bool wubu_deploy_validate_macos(const wubu_macos_config_t* config) {
    return config && config->app_bundle_id && config->output_app && config->wubu_binary;
}

/* ============================================================
 * Verification
 * ============================================================ */

bool wubu_deploy_verify_output(const char* output_path, const char* expected_sha256) {
    if (!expected_sha256) return true;
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "sha256sum %s | cut -d' ' -f1", output_path);
    char actual[65];
    if (!run_command_capture(cmd, actual, sizeof(actual))) return false;
    actual[strcspn(actual, "\n")] = '\0';
    return strcmp(actual, expected_sha256) == 0;
}
