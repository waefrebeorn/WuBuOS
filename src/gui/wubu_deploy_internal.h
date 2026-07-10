/*
 * wubu_deploy_internal.h -- WuBuOS deployment layer internal header.
 * Shared declarations for deploy sub-modules (util).
 * Public API + types in wubu_deploy.h.
 */

#ifndef WUBU_DEPLOY_INTERNAL_H
#define WUBU_DEPLOY_INTERNAL_H

#include "wubu_deploy.h"
#include <stdbool.h>
#include <stddef.h>
#include <sys/stat.h>

/* -- File/command helpers (implemented in wubu_deploy_util.c) ------ */
bool run_command(const char *cmd, const char *workdir);
bool run_command_capture(const char *cmd, char *output, size_t output_size);
bool write_file(const char *path, const char *content);
bool mkdir_p(const char *path, mode_t mode);
bool copy_file(const char *src, const char *dst);

/* -- Config-file generators (implemented in wubu_deploy_gen.c) ---- */
bool wubu_deploy_generate_limine_conf(const char *output_path, const char *kernel_cmdline);
bool wubu_deploy_generate_wsl_conf(const char *output_path, bool systemd);
bool wubu_deploy_generate_dockerfile(const wubu_oci_config_t *config, const char *output_path);
bool wubu_deploy_generate_entitlements(const wubu_macos_config_t *config, const char *output_path);
bool wubu_deploy_generate_infoplist(const wubu_macos_config_t *config, const char *output_path);

#endif /* WUBU_DEPLOY_INTERNAL_H */
