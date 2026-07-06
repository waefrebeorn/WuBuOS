/*
 * wubu_archd.c  --  WuBuOS Arch Linux Daemon Implementation
 *
 * Manages Arch Linux container roots: lifecycle, packages, services,
 * health monitoring, GPU passthrough, and event publishing.
 *
 * Design principles (from SteamOS + Ubuntu research):
 *   - Socket activation: can be started on-demand via Unix socket
 *   - Declarative state: roots described by config, daemon reconciles
 *   - Health monitoring: periodic checks, auto-heal, resource tracking
 *   - Event bus: publish state changes to WuBuOS desktop via 9P/socket
 */

#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE

#include "wubu_archd.h"
#include "wubu_arch.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/epoll.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <dirent.h>
#include <time.h>
#include <libgen.h>
#include <ftw.h>

/* -- Recursive directory removal (replaces system("rm -rf")) -------- */

static int unlink_cb(const char *fpath, const struct stat *sb, int typeflag, struct FTW *ftwbuf) {
    (void)sb; (void)typeflag; (void)ftwbuf;
    return unlink(fpath) == 0 ? 0 : -1;
}

static int rm_rf(const char *path) {
    if (!path) return -1;
    return nftw(path, unlink_cb, 64, FTW_DEPTH | FTW_PHYS);
}

/* -- Helper: run command via fork+exec (no system()) ---------------- */

static int run_cmd(const char *cmd) {
    pid_t pid = fork();
    if (pid < 0) return -1;

    if (pid == 0) {
        execl("/bin/sh", "sh", "-c", cmd, (char*)NULL);
        _exit(127);
    }

    int status;
    waitpid(pid, &status, 0);
    if (WIFEXITED(status))
        return WEXITSTATUS(status);
    return -1;
}

/* -- Helper: run command in chroot via fork+exec -------------------- */

static int run_chroot_cmd(const char *root, const char *fmt, ...) {
    char cmd[WUBU_ARCHD_MAX_CMD];
    va_list args;
    va_start(args, fmt);
    vsnprintf(cmd, sizeof(cmd), fmt, args);
    va_end(args);
    
    pid_t pid = fork();
    if (pid < 0) return -1;
    
    if (pid == 0) {
        execl("/bin/sh", "sh", "-c", cmd, (char*)NULL);
        _exit(127);
    }
    
    int status;
    waitpid(pid, &status, 0);
    if (WIFEXITED(status))
        return WEXITSTATUS(status);
    return -1;
}

/* -- Helper: write file --------------------------------------------- */

static bool write_file(const char *path, const char *content) {
    FILE *f = fopen(path, "w");
    if (!f) return false;
    fputs(content, f);
    fclose(f);
    return true;
}

/* -- Helper: mkdir -p ----------------------------------------------- */

static int mkdir_p(const char *path, mode_t mode) {
    char tmp[1024];
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

/* -- String Tables ----------------------------------------------- */

const char *wubu_archd_root_state_str(WubuArchRootState state) {
    switch (state) {
        case ROOT_STATE_INACTIVE:    return "inactive";
        case ROOT_STATE_ACTIVATING:  return "activating";
        case ROOT_STATE_ACTIVE:      return "active";
        case ROOT_STATE_DEACTIVATING:return "deactivating";
        case ROOT_STATE_FAILED:      return "failed";
        case ROOT_STATE_MAINTENANCE: return "maintenance";
        case ROOT_STATE_SNAPSHOT:    return "snapshot";
        default:                     return "unknown";
    }
}

const char *wubu_archd_root_type_str(WubuArchRootType type) {
    switch (type) {
        case ROOT_TYPE_BASE:     return "base";
        case ROOT_TYPE_GUI:      return "gui";
        case ROOT_TYPE_STEAM:    return "steam";
        case ROOT_TYPE_GAMING:   return "gaming";
        case ROOT_TYPE_PROTON:   return "proton";
        case ROOT_TYPE_DEVELOP:  return "develop";
        case ROOT_TYPE_CUSTOM:   return "custom";
        default:                 return "unknown";
    }
}

const char *wubu_archd_svc_state_str(WubuArchServiceState state) {
    switch (state) {
        case SERVICE_STATE_DISABLED:  return "disabled";
        case SERVICE_STATE_ENABLED:   return "enabled";
        case SERVICE_STATE_RUNNING:   return "running";
        case SERVICE_STATE_FAILED:    return "failed";
        case SERVICE_STATE_RESTARTING:return "restarting";
        default:                      return "unknown";
    }
}

const char *wubu_archd_cmd_str(WubuArchdCmd cmd) {
    switch (cmd) {
        case ARCHD_CMD_ROOT_CREATE:  return "root_create";
        case ARCHD_CMD_ROOT_DESTROY: return "root_destroy";
        case ARCHD_CMD_ROOT_LIST:    return "root_list";
        case ARCHD_CMD_ROOT_INFO:    return "root_info";
        case ARCHD_CMD_ROOT_CLONE:   return "root_clone";
        case ARCHD_CMD_ROOT_SNAPSHOT:return "root_snapshot";
        case ARCHD_CMD_ROOT_ROLLBACK:return "root_rollback";
        case ARCHD_CMD_PKG_INSTALL:  return "pkg_install";
        case ARCHD_CMD_PKG_REMOVE:   return "pkg_remove";
        case ARCHD_CMD_PKG_UPDATE:   return "pkg_update";
        case ARCHD_CMD_SVC_ENABLE:   return "svc_enable";
        case ARCHD_CMD_SVC_DISABLE:  return "svc_disable";
        case ARCHD_CMD_SVC_START:    return "svc_start";
        case ARCHD_CMD_SVC_STOP:     return "svc_stop";
        case ARCHD_CMD_SVC_RESTART:  return "svc_restart";
        case ARCHD_CMD_SVC_STATUS:   return "svc_status";
        case ARCHD_CMD_SVC_LIST:     return "svc_list";
        case ARCHD_CMD_PING:         return "ping";
        case ARCHD_CMD_STATS:        return "stats";
        case ARCHD_CMD_HEALTH:       return "health";
        case ARCHD_CMD_GPU_DETECT:   return "gpu_detect";
        case ARCHD_CMD_GPU_LIST:     return "gpu_list";
        case ARCHD_CMD_GPU_ASSIGN:   return "gpu_assign";
        case ARCHD_CMD_SHUTDOWN:     return "shutdown";
        case ARCHD_CMD_RELOAD:       return "reload";
        case ARCHD_CMD_VERSION:      return "version";
        default:                     return "unknown";
    }
}

const char *wubu_archd_version(void) {
    return WUBU_ARCHD_VERSION;
}

/* -- Logging ------------------------------------------------------ */

static void archd_log(WubuArchd *d, int level, const char *fmt, ...) {
    if (level > d->config.log_level) return;
    FILE *f = fopen(d->config.log_path, "a");
    if (!f) f = stderr;
    time_t now = time(NULL);
    struct tm *tm = localtime(&now);
    char ts[64];
    strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", tm);
    const char *lvl[] = {"ERR", "WRN", "INF", "DBG"};
    fprintf(f, "[%s] %s ", ts, lvl[level < 4 ? level : 3]);
    va_list ap;
    va_start(ap, fmt);
    vfprintf(f, fmt, ap);
    va_end(ap);
    fprintf(f, "\n");
    if (f != stderr) fclose(f);
}

/* -- PID File ----------------------------------------------------- */

static int archd_write_pid(WubuArchd *d) {
    FILE *f = fopen(d->config.socket_path, "w");
    /* Actually write to PID path */
    char pid_path[WUBU_ARCHD_MAX_PATH];
    snprintf(pid_path, sizeof(pid_path), "%s", WUBU_ARCHD_PID_PATH);
    f = fopen(pid_path, "w");
    if (!f) return -1;
    fprintf(f, "%d\n", getpid());
    fclose(f);
    return 0;
}

static void archd_remove_pid(void) {
    unlink(WUBU_ARCHD_PID_PATH);
}

/* -- Socket Server ------------------------------------------------ */

static int archd_socket_create(WubuArchd *d) {
    struct sockaddr_un addr;
    int fd;

    unlink(d->config.socket_path);
    fd = socket(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK, 0);
    if (fd < 0) return -1;

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, d->config.socket_path, sizeof(addr.sun_path) - 1);

    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(fd);
        return -1;
    }

    if (listen(fd, 8) < 0) {
        close(fd);
        return -1;
    }

    chmod(d->config.socket_path, 0666);
    return fd;
}

/* -- Root Index (scan /var/wubu/roots) --------------------------- */

static int archd_scan_roots(WubuArchd *d) {
    DIR *dir = opendir(d->config.roots_path);
    if (!dir) {
        mkdir(d->config.roots_path, 0755);
        return 0;
    }

    d->root_count = 0;
    struct dirent *ent;
    while ((ent = readdir(dir)) != NULL && d->root_count < WUBU_ARCHD_MAX_ROOTS) {
        if (ent->d_name[0] == '.') continue;

        WubuArchdRoot *r = &d->roots[d->root_count];
        memset(r, 0, sizeof(*r));
        strncpy(r->name, ent->d_name, WUBU_ARCHD_MAX_ROOT_NAME - 1);
        snprintf(r->path, sizeof(r->path), "%s/%s", d->config.roots_path, ent->d_name);

        /* Check if it's a valid Arch root */
        char test_path[WUBU_ARCHD_MAX_PATH];
        snprintf(test_path, sizeof(test_path), "%s/etc/arch-release", r->path);
        if (access(test_path, F_OK) == 0) {
            r->state = ROOT_STATE_ACTIVE;
            r->type = ROOT_TYPE_BASE; /* Default, can be overridden by config */
        } else {
            r->state = ROOT_STATE_INACTIVE;
        }

        /* Get disk usage */
        struct stat st;
        if (stat(r->path, &st) == 0) {
            r->created = st.st_ctime;
            r->last_used = st.st_atime;
        }

        d->root_count++;
    }
    closedir(dir);
    return d->root_count;
}

/* -- Root Operations ---------------------------------------------- */

int wubu_archd_root_create(WubuArchd *d, const char *name,
                             WubuArchRootType type, const char *mirror) {
    if (!name || !d) return -1;
    if (d->root_count >= WUBU_ARCHD_MAX_ROOTS) return -1;

    /* Check if root already exists */
    for (int i = 0; i < d->root_count; i++) {
        if (strcmp(d->roots[i].name, name) == 0) return -1;
    }

    WubuArchdRoot *r = &d->roots[d->root_count];
    memset(r, 0, sizeof(*r));
    strncpy(r->name, name, WUBU_ARCHD_MAX_ROOT_NAME - 1);
    snprintf(r->path, sizeof(r->path), "%s/%s", d->config.roots_path, name);
    r->type = type;
    r->state = ROOT_STATE_ACTIVATING;
    r->created = time(NULL);
    r->auto_update = d->config.auto_update;

    archd_log(d, 2, "Creating root '%s' type=%s path=%s",
              name, wubu_archd_root_type_str(type), r->path);

    /* Bootstrap based on type */
    int ret = -1;
    switch (type) {
        case ROOT_TYPE_BASE:
            ret = wubu_arch_bootstrap(r->path, mirror ? mirror : d->config.mirror, NULL);
            break;
        case ROOT_TYPE_GUI:
            ret = wubu_arch_bootstrap_gui(r->path, mirror ? mirror : d->config.mirror);
            break;
        case ROOT_TYPE_STEAM:
            ret = wubu_arch_bootstrap_steam(r->path, mirror ? mirror : d->config.mirror);
            break;
        case ROOT_TYPE_GAMING:
            ret = wubu_arch_bootstrap_gaming(r->path, mirror ? mirror : d->config.mirror);
            break;
        case ROOT_TYPE_PROTON:
            ret = wubu_arch_bootstrap_steam_runtime2(r->path, mirror ? mirror : d->config.mirror);
            break;
        case ROOT_TYPE_DEVELOP:
            ret = wubu_arch_bootstrap(r->path, mirror ? mirror : d->config.mirror, NULL);
            if (ret == 0) {
                ret = wubu_arch_install(r->path,
                    "base-devel gcc clang cmake ninja git vim nano");
            }
            break;
        default:
            ret = wubu_arch_bootstrap(r->path, mirror ? mirror : d->config.mirror, NULL);
            break;
    }

    if (ret == 0) {
        r->state = ROOT_STATE_ACTIVE;
        d->root_count++;
        archd_log(d, 2, "Root '%s' created successfully", name);
        wubu_archd_publish_event(d, "root_created", name, NULL);
    } else {
        r->state = ROOT_STATE_FAILED;
        archd_log(d, 0, "Root '%s' creation failed: %d", name, ret);
    }

    return ret;
}

int wubu_archd_root_destroy(WubuArchd *d, const char *name) {
    if (!name || !d) return -1;
    for (int i = 0; i < d->root_count; i++) {
        if (strcmp(d->roots[i].name, name) == 0) {
            WubuArchdRoot *r = &d->roots[i];
            r->state = ROOT_STATE_DEACTIVATING;
            archd_log(d, 2, "Destroying root '%s' at %s", name, r->path);
            /* Remove directory tree */
            int ret = rm_rf(r->path);
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
    
    mkdir_p(repo_path, 0755);
    
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
    
    return write_file(hook_path, hook_content);
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

/* -- Daemon Lifecycle --------------------------------------------- */

int wubu_archd_init(WubuArchd *d, const WubuArchdConfig *config) {
    if (!d || !config) return -1;
    memset(d, 0, sizeof(*d));
    d->config = *config;

    /* Set defaults */
    if (!d->config.roots_path[0])
        strncpy(d->config.roots_path, WUBU_ARCHD_ROOTS_PATH, WUBU_ARCHD_MAX_PATH - 1);
    if (!d->config.socket_path[0])
        strncpy(d->config.socket_path, WUBU_ARCHD_SOCKET_PATH, WUBU_ARCHD_MAX_PATH - 1);
    if (!d->config.log_path[0])
        strncpy(d->config.log_path, WUBU_ARCHD_LOG_PATH, WUBU_ARCHD_MAX_PATH - 1);
    if (d->config.max_roots <= 0)
        d->config.max_roots = WUBU_ARCHD_MAX_ROOTS;
    if (d->config.health_check_interval_sec <= 0)
        d->config.health_check_interval_sec = 60;
    if (d->config.update_check_interval_sec <= 0)
        d->config.update_check_interval_sec = 3600;

    /* Create directories */
    mkdir(d->config.roots_path, 0755);
    mkdir("/run/wubu", 0755);

    /* Scan existing roots */
    archd_scan_roots(d);

    d->start_time = time(NULL);
    archd_log(d, 2, "Archd initialized: roots=%d socket=%s",
              d->root_count, d->config.socket_path);
    return 0;
}

int wubu_archd_start(WubuArchd *d) {
    if (!d) return -1;

    d->server_fd = archd_socket_create(d);
    if (d->server_fd < 0) {
        archd_log(d, 0, "Failed to create socket: %s", strerror(errno));
        return -1;
    }

    d->epoll_fd = epoll_create1(0);
    if (d->epoll_fd < 0) {
        close(d->server_fd);
        return -1;
    }

    struct epoll_event ev = { .events = EPOLLIN, .data.fd = d->server_fd };
    epoll_ctl(d->epoll_fd, EPOLL_CTL_ADD, d->server_fd, &ev);

    archd_write_pid(d);
    d->running = true;

    archd_log(d, 2, "Archd started on %s", d->config.socket_path);
    return 0;
}

void wubu_archd_event_loop(WubuArchd *d) {
    if (!d || !d->running) return;

    struct epoll_event events[8];
    time_t last_health = 0;
    time_t last_update = 0;

    while (d->running) {
        int nfds = epoll_wait(d->epoll_fd, events, 8, 1000);
        time_t now = time(NULL);

        for (int i = 0; i < nfds; i++) {
            if (events[i].data.fd == d->server_fd) {
                /* Accept new connection */
                int client = accept4(d->server_fd, NULL, NULL, SOCK_NONBLOCK);
                if (client >= 0) {
                    struct epoll_event cev = { .events = EPOLLIN, .data.fd = client };
                    epoll_ctl(d->epoll_fd, EPOLL_CTL_ADD, client, &cev);
                }
            } else {
                /* Handle client request */
                int fd = events[i].data.fd;
                char buf[4096];
                ssize_t n = read(fd, buf, sizeof(buf) - 1);
                if (n > 0) {
                    buf[n] = '\0';
                    /* Simple protocol: "CMD:root_name:data" */
                    char cmd_str[64], root[64], data[2048];
                    cmd_str[0] = root[0] = data[0] = '\0';
                    sscanf(buf, "%63[^:]:%63[^:]:%2047[^\n]", cmd_str, root, data);

                    WubuArchdResponse resp = {0};
                    d->requests_handled++;

                    /* Dispatch */
                    if (strcmp(cmd_str, "ping") == 0) {
                        resp.status = 0;
                        snprintf(resp.message, sizeof(resp.message), "pong");
                    } else if (strcmp(cmd_str, "version") == 0) {
                        resp.status = 0;
                        snprintf(resp.data, sizeof(resp.data), "%s", WUBU_ARCHD_VERSION);
                    } else if (strcmp(cmd_str, "stats") == 0) {
                        resp.status = 0;
                        wubu_archd_stats(d, resp.data, sizeof(resp.data));
                    } else if (strcmp(cmd_str, "root_list") == 0) {
                        resp.status = 0;
                        for (int j = 0; j < d->root_count; j++) {
                            char entry[256];
                            snprintf(entry, sizeof(entry), "%s:%s:%s\n",
                                     d->roots[j].name,
                                     wubu_archd_root_type_str(d->roots[j].type),
                                     wubu_archd_root_state_str(d->roots[j].state));
                            strncat(resp.data, entry, sizeof(resp.data) - strlen(resp.data) - 1);
                        }
                    } else if (strcmp(cmd_str, "root_create") == 0) {
                        /* data = "type_name" */
                        int type = 0;
                        char type_name[32];
                        if (sscanf(data, "%31[^:]", type_name) == 1) {
                            WubuArchRootType rt = ROOT_TYPE_BASE;
                            if (strcmp(type_name, "gui") == 0) rt = ROOT_TYPE_GUI;
                            else if (strcmp(type_name, "gaming") == 0) rt = ROOT_TYPE_GAMING;
                            else if (strcmp(type_name, "proton") == 0) rt = ROOT_TYPE_PROTON;
                            resp.status = wubu_archd_root_create(d, root, rt, NULL);
                        }
                    } else if (strcmp(cmd_str, "root_destroy") == 0) {
                        resp.status = wubu_archd_root_destroy(d, root);
                    } else if (strcmp(cmd_str, "pkg_install") == 0) {
                        resp.status = wubu_archd_pkg_install(d, root, data);
                    } else if (strcmp(cmd_str, "pkg_update") == 0) {
                        resp.status = wubu_archd_pkg_update(d, root);
                    } else if (strcmp(cmd_str, "svc_enable") == 0) {
                        resp.status = wubu_archd_svc_enable(d, root, data);
                    } else if (strcmp(cmd_str, "svc_start") == 0) {
                        resp.status = wubu_archd_svc_start(d, root, data);
                    } else if (strcmp(cmd_str, "svc_stop") == 0) {
                        resp.status = wubu_archd_svc_stop(d, root, data);
                    } else if (strcmp(cmd_str, "svc_restart") == 0) {
                        resp.status = wubu_archd_svc_restart(d, root, data);
                    } else if (strcmp(cmd_str, "health") == 0) {
                        resp.status = wubu_archd_health_check(d, root);
                    } else if (strcmp(cmd_str, "gpu_detect") == 0) {
                        resp.status = 0;
                        wubu_archd_gpu_detect(d, resp.data, sizeof(resp.data));
                    } else if (strcmp(cmd_str, "shutdown") == 0) {
                        resp.status = 0;
                        d->running = false;
                    } else {
                        resp.status = -1;
                        snprintf(resp.message, sizeof(resp.message),
                                 "Unknown command: %s", cmd_str);
                        d->errors++;
                    }

                    /* Send response */
                    char resp_buf[WUBU_ARCHD_MAX_RESPONSE + 64];
                    snprintf(resp_buf, sizeof(resp_buf), "%d:%s:%s\n",
                             resp.status, resp.message, resp.data);
                    write(fd, resp_buf, strlen(resp_buf));
                }
                epoll_ctl(d->epoll_fd, EPOLL_CTL_DEL, fd, NULL);
                close(fd);
            }
        }

        /* Periodic health checks */
        if (now - last_health >= d->config.health_check_interval_sec) {
            last_health = now;
            for (int j = 0; j < d->root_count; j++) {
                if (d->roots[j].state == ROOT_STATE_ACTIVE) {
                    wubu_archd_health_check(d, d->roots[j].name);
                }
            }
        }

        /* Periodic update checks */
        if (d->config.auto_update && now - last_update >= d->config.update_check_interval_sec) {
            last_update = now;
            for (int j = 0; j < d->root_count; j++) {
                if (d->roots[j].auto_update && d->roots[j].state == ROOT_STATE_ACTIVE) {
                    archd_log(d, 2, "Auto-updating root '%s'", d->roots[j].name);
                    wubu_archd_pkg_update(d, d->roots[j].name);
                }
            }
        }
    }
}

void wubu_archd_stop(WubuArchd *d) {
    if (!d) return;
    d->running = false;
    archd_log(d, 2, "Archd stopping");
}

void wubu_archd_shutdown(WubuArchd *d) {
    if (!d) return;
    d->running = false;
    if (d->server_fd >= 0) close(d->server_fd);
    if (d->epoll_fd >= 0) close(d->epoll_fd);
    unlink(d->config.socket_path);
    archd_remove_pid();
    archd_log(d, 2, "Archd shutdown complete");
}

/* -- Main Entry Point --------------------------------------------- */

#ifndef WUBD_TEST_MAIN
int main(int argc, char *argv[]) {
    WubuArchdConfig config = {0};
    strncpy(config.roots_path, WUBU_ARCHD_ROOTS_PATH, WUBU_ARCHD_MAX_PATH - 1);
    strncpy(config.socket_path, WUBU_ARCHD_SOCKET_PATH, WUBU_ARCHD_MAX_PATH - 1);
    strncpy(config.log_path, WUBU_ARCHD_LOG_PATH, WUBU_ARCHD_MAX_PATH - 1);
    config.auto_update = false;
    config.health_check_interval_sec = 60;
    config.update_check_interval_sec = 3600;
    config.log_level = 2;
    config.daemonize = true;
    config.gpu_detect = true;

    /* Parse args */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--no-daemon") == 0) config.daemonize = false;
        else if (strcmp(argv[i], "--auto-update") == 0) config.auto_update = true;
        else if (strcmp(argv[i], "--debug") == 0) config.log_level = 3;
        else if (strcmp(argv[i], "--roots") == 0 && i + 1 < argc) {
            strncpy(config.roots_path, argv[++i], WUBU_ARCHD_MAX_PATH - 1);
        } else if (strcmp(argv[i], "--mirror") == 0 && i + 1 < argc) {
            strncpy(config.mirror, argv[++i], sizeof(config.mirror) - 1);
        } else if (strcmp(argv[i], "--help") == 0) {
            printf("wubu_archd -- WuBuOS Arch Linux Daemon\n");
            printf("  --no-daemon     Run in foreground\n");
            printf("  --auto-update   Enable automatic root updates\n");
            printf("  --debug         Verbose logging\n");
            printf("  --roots PATH    Roots directory (default: %s)\n", WUBU_ARCHD_ROOTS_PATH);
            printf("  --mirror URL    Pacman mirror URL\n");
            return 0;
        }
    }

    signal(SIGPIPE, SIG_IGN);
    signal(SIGTERM, SIG_DFL);
    signal(SIGINT, SIG_DFL);

    WubuArchd daemon;
    if (wubu_archd_init(&daemon, &config) != 0) {
        fprintf(stderr, "Failed to initialize archd\n");
        return 1;
    }

    if (config.daemonize) {
        pid_t pid = fork();
        if (pid < 0) { perror("fork"); return 1; }
        if (pid > 0) { printf("wubu_archd started (PID %d)\n", pid); return 0; }
        setsid();
        /* Redirect stdio */
        int devnull = open("/dev/null", O_RDWR);
        dup2(devnull, STDIN_FILENO);
        dup2(devnull, STDOUT_FILENO);
        dup2(devnull, STDERR_FILENO);
        close(devnull);
    }

    if (wubu_archd_start(&daemon) != 0) {
        fprintf(stderr, "Failed to start archd\n");
        return 1;
    }

    wubu_archd_event_loop(&daemon);
    wubu_archd_shutdown(&daemon);
    return 0;
}
#endif /* WUBD_TEST_MAIN */
