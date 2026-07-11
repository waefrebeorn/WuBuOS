/* wubu_deploy_config.c -- WuBuOS deploy: default target config getters.
 * Extracted from wubu_deploy.c (separable leaf). Self-contained: pure config
 * data initialization. C11, minimal includes.
 */
#include "wubu_deploy.h"

#include <string.h>

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
