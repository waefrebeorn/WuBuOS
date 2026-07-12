/*
 * wubu_archd_daemon.c -- WuBuOS archd daemon lifecycle + main entry.
 * Extracted from the monolithic wubu_archd.c. Depends on
 * wubu_archd_internal.h for shared statics (archd_log/pid/socket/scan_roots).
 * C11, no god headers.
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
#include "wubu_archd_internal.h"
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

void archd_log(WubuArchd *d, int level, const char *fmt, ...) {
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

int archd_write_pid(WubuArchd *d) {
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

void archd_remove_pid(void) {
    unlink(WUBU_ARCHD_PID_PATH);
}

/* -- Socket Server ------------------------------------------------ */

int archd_socket_create(WubuArchd *d) {
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

int archd_scan_roots(WubuArchd *d) {
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


/* -- Daemon Lifecycle (continued from wubu_archd.c) --------- */

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

