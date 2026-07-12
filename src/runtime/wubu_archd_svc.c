/*
 * wubu_archd_svc.c -- WuBuOS archd pkg/repo/svc/aur/hook/health/gpu mgmt.
 * Extracted from the monolithic wubu_archd.c. Depends on
 * wubu_archd_internal.h for shared statics. C11, no god headers.
 */

#include "wubu_archd.h"
#include "wubu_arch.h"
#include "wubu_archd_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <fcntl.h>
#include <dirent.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/epoll.h>
#include <sys/wait.h>

int wubu_archd_root_destroy(WubuArchd *d, const char *name) {
    if (!name || !d) return -1;
    for (int i = 0; i < d->root_count; i++) {
        if (strcmp(d->roots[i].name, name) == 0) {
            WubuArchdRoot *r = &d->roots[i];
            r->state = ROOT_STATE_DEACTIVATING;
            archd_log(d, 2, "Destroying root '%s' at %s", name, r->path);
            /* Remove directory tree */
            int ret = archd_rm_rf(r->path);
            /* Remove from array */
            memmove(&d->roots[i], &d->roots[i + 1],
                    (d->root_count - i - 1) * sizeof(WubuArchdRoot));
            d->root_count--;
            wubu_archd_publish_event(d, "root_destroyed", name, NULL);
            return ret;
        }
    }
    return -1;
}

int wubu_archd_root_list(WubuArchd *d, WubuArchdRoot *out, int max) {
    if (!d || !out) return -1;
    int count = d->root_count < max ? d->root_count : max;
    memcpy(out, d->roots, count * sizeof(WubuArchdRoot));
    return count;
}

int wubu_archd_root_info(WubuArchd *d, const char *name, WubuArchdRoot *out) {
    if (!d || !name || !out) return -1;
    for (int i = 0; i < d->root_count; i++) {
        if (strcmp(d->roots[i].name, name) == 0) {
            *out = d->roots[i];
            return 0;
        }
    }
    return -1;
}

/* -- Package Operations ------------------------------------------- */

int wubu_archd_pkg_install(WubuArchd *d, const char *root, const char *packages) {
    WubuArchdRoot r;
    if (wubu_archd_root_info(d, root, &r) != 0) return -1;
    archd_log(d, 2, "Installing packages in '%s': %s", root, packages);
    return wubu_arch_install(r.path, packages);
}

int wubu_archd_pkg_remove(WubuArchd *d, const char *root, const char *packages) {
    WubuArchdRoot r;
    if (wubu_archd_root_info(d, root, &r) != 0) return -1;
    archd_log(d, 2, "Removing packages from '%s': %s", root, packages);
    return run_chroot_cmd(r.path, "pacman -R --noconfirm %s", packages);
}

int wubu_archd_pkg_update(WubuArchd *d, const char *root) {
    WubuArchdRoot r;
    if (wubu_archd_root_info(d, root, &r) != 0) return -1;
    r.state = ROOT_STATE_MAINTENANCE;
    archd_log(d, 2, "Updating root '%s'", root);
    int ret = wubu_arch_update(r.path);
    r.state = ROOT_STATE_ACTIVE;
    r.last_updated = time(NULL);
    wubu_archd_publish_event(d, "root_updated", root, NULL);
    return ret;
}

int wubu_archd_pkg_list(WubuArchd *d, const char *root, char *out, size_t out_size) {
    WubuArchdRoot r;
    if (wubu_archd_root_info(d, root, &r) != 0) return -1;
    char cmd[WUBU_ARCHD_MAX_CMD];
    snprintf(cmd, sizeof(cmd), "arch-chroot %s pacman -Q 2>/dev/null", r.path);
    FILE *fp = popen(cmd, "r");
    if (!fp) return -1;
    size_t total = 0;
    while (fgets(out + total, out_size - total, fp) && total < out_size - 1)
        total += strlen(out + total);
    pclose(fp);
    return 0;
}

/* -- AUR Support -------------------------------------------------- */

int wubu_archd_aur_build(WubuArchd *d, const char *root, const char *pkg_name) {
    WubuArchdRoot r;
    if (wubu_archd_root_info(d, root, &r) != 0) return -1;
    archd_log(d, 2, "Building AUR package '%s' in root '%s'", pkg_name, root);
    
    char check_cmd[WUBU_ARCHD_MAX_CMD];
    snprintf(check_cmd, sizeof(check_cmd), "arch-chroot %s which git 2>/dev/null", r.path);
    FILE *fp = popen(check_cmd, "r");
    if (!fp || pclose(fp) != 0) {
        archd_log(d, 0, "git not available in root '%s'", root);
        return -1;
    }
    
    char build_cmd[WUBU_ARCHD_MAX_CMD];
    snprintf(build_cmd, sizeof(build_cmd),
             "arch-chroot %s /bin/bash -c 'cd /tmp && git clone https://aur.archlinux.org/%s.git 2>&1 && cd %s && makepkg -s --noconfirm 2>&1'",
             r.path, pkg_name, pkg_name);
    return run_cmd(build_cmd);
}

int wubu_archd_aur_search(WubuArchd *d, const char *query, char *out, size_t out_size) {
    if (!query || !out) return -1;
    
    char cmd[WUBU_ARCHD_MAX_CMD];
    // Simpler approach: use curl to get raw JSON, then just return it
    snprintf(cmd, sizeof(cmd),
             "curl -s \"https://aur.archlinux.org/rpc/?v=5&type=search&arg=%s\" 2>/dev/null",
             query);
    
    FILE *fp = popen(cmd, "r");
    if (!fp) return -1;
    
    size_t total = 0;
    char buf[1024];
    while (fgets(buf, sizeof(buf), fp) && total < out_size - 1) {
        size_t len = strlen(buf);
        if (total + len < out_size - 1) {
            memcpy(out + total, buf, len);
            total += len;
        }
    }
    pclose(fp);
    out[total] = '\0';
    return 0;
}

/* -- Package Signing & Repo Management ----------------------------- */

int wubu_archd_repo_init(WubuArchd *d, const char *repo_name, const char *repo_path) {
    if (!d || !repo_name || !repo_path) return -1;
    archd_log(d, 2, "Initializing repo '%s' at '%s'", repo_name, repo_path);
    
    archd_mkdir_p(repo_path, 0755);
    
    char cmd[WUBU_ARCHD_MAX_CMD];
    snprintf(cmd, sizeof(cmd), "repo-add %s/%s.db.tar.gz", repo_path, repo_name);
    return run_cmd(cmd);
}

int wubu_archd_repo_add(WubuArchd *d, const char *repo_name, const char *repo_path, const char *pkg_file) {
    if (!d || !repo_name || !repo_path || !pkg_file) return -1;
    archd_log(d, 2, "Adding package '%s' to repo '%s'", pkg_file, repo_name);
    
    char cmd[WUBU_ARCHD_MAX_CMD];
    snprintf(cmd, sizeof(cmd), "repo-add %s/%s.db.tar.gz %s", repo_path, repo_name, pkg_file);
    return run_cmd(cmd);
}

int wubu_archd_repo_remove(WubuArchd *d, const char *repo_name, const char *repo_path, const char *pkg_name) {
    if (!d || !repo_name || !repo_path || !pkg_name) return -1;
    archd_log(d, 2, "Removing package '%s' from repo '%s'", pkg_name, repo_name);
    
    char cmd[WUBU_ARCHD_MAX_CMD];
    snprintf(cmd, sizeof(cmd), "repo-remove %s/%s.db.tar.gz %s", repo_path, repo_name, pkg_name);
    return run_cmd(cmd);
}

int wubu_archd_sign_package(WubuArchd *d, const char *pkg_file, const char *key_id) {
    if (!d || !pkg_file) return -1;
    archd_log(d, 2, "Signing package '%s'", pkg_file);
    
    char cmd[WUBU_ARCHD_MAX_CMD];
    if (key_id) {
        snprintf(cmd, sizeof(cmd), "gpg --detach-sign --default-key %s %s", key_id, pkg_file);
    } else {
        snprintf(cmd, sizeof(cmd), "gpg --detach-sign %s", pkg_file);
    }
    return run_cmd(cmd);
}

/* -- Pacman Hooks ------------------------------------------------- */

int wubu_archd_hook_install(WubuArchd *d, const char *root, const char *hook_name,
                            const char *trigger, const char *action) {
    if (!d || !root || !hook_name || !trigger || !action) return -1;
    archd_log(d, 2, "Installing pacman hook '%s' in root '%s'", hook_name, root);
    
    WubuArchdRoot r;
    if (wubu_archd_root_info(d, root, &r) != 0) return -1;
    
    char hook_path[WUBU_ARCHD_MAX_PATH];
    snprintf(hook_path, sizeof(hook_path), "%s/etc/pacman.d/hooks/%s.hook", r.path, hook_name);
    
    char hook_content[4096];
    snprintf(hook_content, sizeof(hook_content),
             "[Trigger]\n"
             "Type = %s\n"
             "Operation = Install\n"
             "Operation = Upgrade\n"
             "Target = %s\n"
             "\n"
             "[Action]\n"
             "Description = %s\n"
             "When = PostTransaction\n"
             "Exec = /bin/sh -c \"%s\"\n",
             (strstr(trigger, "file") != NULL) ? "File" : "Package",
             trigger,
             action,
             action);
    
    return archd_write_file(hook_path, hook_content);
}
int wubu_archd_hook_remove(WubuArchd *d, const char *root, const char *hook_name) {
    if (!d || !root || !hook_name) return -1;
    
    WubuArchdRoot r;
    if (wubu_archd_root_info(d, root, &r) != 0) return -1;
    
    char hook_path[WUBU_ARCHD_MAX_PATH];
    snprintf(hook_path, sizeof(hook_path), "%s/etc/pacman.d/hooks/%s.hook", r.path, hook_name);
    return unlink(hook_path) == 0 ? 0 : -1;
}

/* -- ABS (Arch Build System) -------------------------------------- */

int wubu_archd_abs_update(WubuArchd *d, const char *root) {
    if (!d || !root) return -1;
    archd_log(d, 2, "Updating ABS tree in root '%s'", root);
    
    WubuArchdRoot r;
    if (wubu_archd_root_info(d, root, &r) != 0) return -1;
    
    char cmd[WUBU_ARCHD_MAX_CMD];
    snprintf(cmd, sizeof(cmd), "arch-chroot %s abs 2>&1", r.path);
    return run_cmd(cmd);
}

int wubu_archd_abs_build(WubuArchd *d, const char *root, const char *pkg_name, const char *build_dir) {
    if (!d || !root || !pkg_name) return -1;
    archd_log(d, 2, "Building ABS package '%s' in root '%s'", pkg_name, root);
    
    WubuArchdRoot r;
    if (wubu_archd_root_info(d, root, &r) != 0) return -1;
    
    const char *abs_path = build_dir ? build_dir : "/var/abs";
    char cmd[WUBU_ARCHD_MAX_CMD];
    snprintf(cmd, sizeof(cmd), 
             "arch-chroot %s /bin/bash -c 'cd %s/%s && makepkg -s --noconfirm 2>&1'",
             r.path, abs_path, pkg_name);
    return run_cmd(cmd);
}

/* -- Service Operations ------------------------------------------- */

int wubu_archd_svc_enable(WubuArchd *d, const char *root, const char *svc) {
    WubuArchdRoot r;
    if (wubu_archd_root_info(d, root, &r) != 0) return -1;
    archd_log(d, 2, "Enabling service '%s' in root '%s'", svc, root);
    return wubu_arch_enable_service(r.path, svc);
}

int wubu_archd_svc_disable(WubuArchd *d, const char *root, const char *svc) {
    WubuArchdRoot r;
    if (wubu_archd_root_info(d, root, &r) != 0) return -1;
    return run_chroot_cmd(r.path, "arch-chroot %s systemctl disable %s 2>/dev/null", r.path, svc);
}

int wubu_archd_svc_start(WubuArchd *d, const char *root, const char *svc) {
    WubuArchdRoot r;
    if (wubu_archd_root_info(d, root, &r) != 0) return -1;
    return run_chroot_cmd(r.path, "arch-chroot %s systemctl start %s 2>/dev/null", r.path, svc);
}

int wubu_archd_svc_stop(WubuArchd *d, const char *root, const char *svc) {
    WubuArchdRoot r;
    if (wubu_archd_root_info(d, root, &r) != 0) return -1;
    return run_chroot_cmd(r.path, "arch-chroot %s systemctl stop %s 2>/dev/null", r.path, svc);
}

int wubu_archd_svc_restart(WubuArchd *d, const char *root, const char *svc) {
    WubuArchdRoot r;
    if (wubu_archd_root_info(d, root, &r) != 0) return -1;
    return run_chroot_cmd(r.path, "arch-chroot %s systemctl restart %s 2>/dev/null", r.path, svc);
}

int wubu_archd_svc_status(WubuArchd *d, const char *root, const char *svc,
                            WubuArchService *out) {
    WubuArchdRoot r;
    if (wubu_archd_root_info(d, root, &r) != 0) return -1;
    memset(out, 0, sizeof(*out));
    strncpy(out->name, svc, WUBU_ARCHD_MAX_PACKAGE_NAME - 1);
    strncpy(out->root_name, root, WUBU_ARCHD_MAX_ROOT_NAME - 1);

    char cmd[WUBU_ARCHD_MAX_CMD];
    snprintf(cmd, sizeof(cmd),
             "arch-chroot %s systemctl is-active %s 2>/dev/null", r.path, svc);
    FILE *fp = popen(cmd, "r");
    if (!fp) { out->state = SERVICE_STATE_FAILED; return -1; }
    char result[64] = {0};
    if (fgets(result, sizeof(result), fp)) {
        if (strncmp(result, "active", 6) == 0)
            out->state = SERVICE_STATE_RUNNING;
        else if (strncmp(result, "inactive", 8) == 0)
            out->state = SERVICE_STATE_DISABLED;
        else
            out->state = SERVICE_STATE_FAILED;
    }
    pclose(fp);
    return 0;
}

/* -- Health & Monitoring ------------------------------------------ */

int wubu_archd_health_check(WubuArchd *d, const char *root) {
    WubuArchdRoot r;
    if (wubu_archd_root_info(d, root, &r) != 0) return -1;

    /* Check root is valid */
    if (!wubu_arch_root_valid(r.path)) {
        archd_log(d, 1, "Health check FAILED for root '%s': invalid rootfs", root);
        return -1;
    }

    /* Check pacman database */
    char cmd[WUBU_ARCHD_MAX_CMD];
    snprintf(cmd, sizeof(cmd), "arch-chroot %s pacman -Qq 2>/dev/null | head -1", r.path);
    FILE *fp = popen(cmd, "r");
    if (!fp) return -1;
    char buf[256];
    int healthy = (fgets(buf, sizeof(buf), fp) != NULL);
    pclose(fp);

    if (healthy) {
        archd_log(d, 3, "Health check OK for root '%s'", root);
    } else {
        archd_log(d, 1, "Health check WARNING for root '%s': pacman db empty", root);
    }
    return healthy ? 0 : -1;
}

int wubu_archd_stats(WubuArchd *d, char *out, size_t out_size) {
    if (!d || !out) return -1;
    int n = snprintf(out, out_size,
        "{"
        "\"version\":\"%s\","
        "\"uptime\":%ld,"
        "\"roots\":%d,"
        "\"requests\":%lu,"
        "\"errors\":%lu"
        "}",
        WUBU_ARCHD_VERSION,
        (long)(time(NULL) - d->start_time),
        d->root_count,
        d->requests_handled,
        d->errors
    );
    return (n < (int)out_size) ? 0 : -1;
}

/* -- GPU Detection ------------------------------------------------ */

int wubu_archd_gpu_detect(WubuArchd *d, char *out, size_t out_size) {
    if (!d || !out) return -1;
    /* Reuse wubu_gpu_detect from wubu_proton2.c */
    char name[256] = {0}, pci[64] = {0};
    /* Simple detection via /sys/class/drm */
    DIR *dir = opendir("/sys/class/drm");
    if (!dir) { snprintf(out, out_size, "[]"); return 0; }

    int count = 0;
    struct dirent *ent;
    while ((ent = readdir(dir)) != NULL) {
        if (strncmp(ent->d_name, "card", 4) != 0) continue;
        char path[256], buf[256];
        snprintf(path, sizeof(path), "/sys/class/drm/%s/device/vendor", ent->d_name);
        FILE *f = fopen(path, "r");
        if (!f) continue;
        if (fgets(buf, sizeof(buf), f)) {
            int vendor = (int)strtol(buf, NULL, 0);
            const char *vendor_name = "Unknown";
            if (vendor == 0x10de) vendor_name = "NVIDIA";
            else if (vendor == 0x1002) vendor_name = "AMD";
            else if (vendor == 0x8086) vendor_name = "Intel";
            if (count > 0) strncat(out, ",", out_size - strlen(out) - 1);
            int n = strlen(out);
            snprintf(out + n, out_size - n,
                     "{\"card\":\"%s\",\"vendor\":\"%s\",\"pci\":\"%s\"}",
                     ent->d_name, vendor_name, pci);
            count++;
        }
        fclose(f);
    }
    closedir(dir);
    if (count == 0) snprintf(out, out_size, "[]");
    return 0;
}

/* -- Event Bus ---------------------------------------------------- */

int wubu_archd_publish_event(WubuArchd *d, const char *event_type,
                               const char *root_name, const char *data) {
    if (!d || !event_type) return -1;
    archd_log(d, 2, "EVENT: %s root=%s", event_type, root_name ? root_name : "*");
    /* Write to event file for WuBuOS desktop to read */
    char event_path[WUBU_ARCHD_MAX_PATH];
    snprintf(event_path, sizeof(event_path), "%s/events", d->config.roots_path);
    FILE *f = fopen(event_path, "a");
    if (!f) return -1;
    time_t now = time(NULL);
    fprintf(f, "{\"time\":%ld,\"event\":\"%s\",\"root\":\"%s\",\"data\":\"%s\"}\n",
            (long)now, event_type, root_name ? root_name : "", data ? data : "");
    fclose(f);
    return 0;
}

