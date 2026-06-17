/**
 * wubu_deploy.h - WuBuOS Multi-Target Deployment Subsystem
 * 
 * Build scripts and configuration for:
 *   - Bare metal initramfs (kernel + init + wubu binary)
 *   - WSL2 distro package (.tar.gz rootfs)
 *   - OCI container image (Dockerfile + build scripts)
 *   - macOS AVF port (virtualization.framework)
 * 
 * All targets produce a single binary: wubu (hosted) or wubu-baremetal
 */

#ifndef WUBU_DEPLOY_H
#define WUBU_DEPLOY_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * Deployment Target Types
 * ============================================================ */
typedef enum {
    WUBU_DEPLOY_BAREMETAL  = 0,  // Bare metal (limine bootloader + initramfs)
    WUBU_DEPLOY_WSL2       = 1,  // WSL2 distro package
    WUBU_DEPLOY_OCI        = 2,  // OCI/Docker container image
    WUBU_DEPLOY_MACOS      = 3,  // macOS AVF virtualization
    WUBU_DEPLOY_COUNT
} wubu_deploy_target_t;

/* ============================================================
 * Bare Metal Configuration
 * ============================================================ */
typedef struct {
    const char* kernel_path;       // Path to Linux kernel (bzImage)
    const char* initramfs_path;    // Output initramfs path
    const char* limine_cfg_path;   // Limine config path
    const char* output_iso;        // Output ISO path
    const char* wubu_binary;       // Path to wubu binary
    const char* rootfs_dir;        // Rootfs staging directory
    bool include_firmware;         // Include linux-firmware
    bool include_drivers;          // Include kernel modules
    const char* kernel_cmdline;    // Kernel command line
} wubu_baremetal_config_t;

/* ============================================================
 * WSL2 Configuration
 * ============================================================ */
typedef struct {
    const char* distro_name;       // Distro name (e.g., "WuBuOS")
    const char* rootfs_path;       // Rootfs staging directory
    const char* output_tar;        // Output .tar.gz path
    const char* wubu_binary;       // Path to wubu binary
    const char* default_user;      // Default username
    bool systemd;                  // Enable systemd (WSL2 feature)
    const char* wsl_conf;          // /etc/wsl.conf content
} wubu_wsl2_config_t;

/* ============================================================
 * OCI Container Configuration
 * ============================================================ */
typedef struct {
    const char* image_name;        // Image name (e.g., "wubuos:latest")
    const char* dockerfile_path;   // Dockerfile path
    const char* context_dir;       // Build context directory
    const char* wubu_binary;       // Path to wubu binary
    const char* base_image;        // Base image (scratch, alpine, debian)
    const char* entrypoint;        // Container entrypoint
    const char** env_vars;         // Environment variables (NULL-terminated)
    const char** ports;            // Exposed ports (NULL-terminated)
    const char** volumes;          // Volumes (NULL-terminated)
    bool multi_arch;               // Build multi-arch (amd64/arm64)
} wubu_oci_config_t;

/* ============================================================
 * macOS AVF Configuration
 * ============================================================ */
typedef struct {
    const char* app_bundle_id;     // Bundle identifier (e.g., "com.wubuos.app")
    const char* app_name;          // App name
    const char* kernel_path;       // Linux kernel for VM
    const char* initramfs_path;    // Initramfs for VM
    const char* output_app;        // Output .app bundle path
    const char* wubu_binary;       // Path to wubu binary (host)
    const char* entitlements;      // Entitlements plist path
    uint64_t vm_memory_mb;         // VM memory in MB
    uint32_t vm_cpus;              // VM CPU count
    bool gui_enabled;              // Enable GUI via virtio-gpu
    bool rosetta;                  // Enable Rosetta for x86_64 Linux
} wubu_macos_config_t;

/* ============================================================
 * Unified Deployment API
 * ============================================================ */

/* Initialize deployment subsystem */
bool wubu_deploy_init(void);

/* Cleanup deployment subsystem */
void wubu_deploy_shutdown(void);

/* Build bare metal initramfs + ISO */
bool wubu_deploy_baremetal(const wubu_baremetal_config_t* config);

/* Build WSL2 distro package */
bool wubu_deploy_wsl2(const wubu_wsl2_config_t* config);

/* Build OCI container image */
bool wubu_deploy_oci(const wubu_oci_config_t* config);

/* Build macOS AVF app bundle */
bool wubu_deploy_macos(const wubu_macos_config_t* config);

/* Get default config for target */
void wubu_deploy_get_default_baremetal(wubu_baremetal_config_t* config);
void wubu_deploy_get_default_wsl2(wubu_wsl2_config_t* config);
void wubu_deploy_get_default_oci(wubu_oci_config_t* config);
void wubu_deploy_get_default_macos(wubu_macos_config_t* config);

/* Validate configuration */
bool wubu_deploy_validate_baremetal(const wubu_baremetal_config_t* config);
bool wubu_deploy_validate_wsl2(const wubu_wsl2_config_t* config);
bool wubu_deploy_validate_oci(const wubu_oci_config_t* config);
bool wubu_deploy_validate_macos(const wubu_macos_config_t* config);

/* ============================================================
 * Helper Functions
 * ============================================================ */

/* Create minimal rootfs with busybox + wubu */
bool wubu_deploy_create_rootfs(const char* rootfs_dir, const char* wubu_binary);

/* Generate limine.conf for bare metal */
bool wubu_deploy_generate_limine_conf(const char* output_path, const char* kernel_cmdline);

/* Generate wsl.conf for WSL2 */
bool wubu_deploy_generate_wsl_conf(const char* output_path, bool systemd);

/* Generate Dockerfile for OCI */
bool wubu_deploy_generate_dockerfile(const wubu_oci_config_t* config, const char* output_path);

/* Generate macOS entitlements plist */
bool wubu_deploy_generate_entitlements(const wubu_macos_config_t* config, const char* output_path);

/* Generate macOS Info.plist */
bool wubu_deploy_generate_infoplist(const wubu_macos_config_t* config, const char* output_path);

/* Run checksum verification on output */
bool wubu_deploy_verify_output(const char* output_path, const char* expected_sha256);

#ifdef __cplusplus
}
#endif

#endif /* WUBU_DEPLOY_H */