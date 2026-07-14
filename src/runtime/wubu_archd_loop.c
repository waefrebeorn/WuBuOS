/*
 * wubu_archd_loop.c -- WuBuOS archd daemon: main event loop
 *
 * Self-contained request-dispatch concern split out of wubu_archd_daemon.c.
 * Owns the epoll event loop, client request parsing/dispatch, and periodic
 * health/update polling. Calls only already-declared archd API surfaces
 * (declared in wubu_archd.h / wubu_archd_internal.h). C11, opaque-safe,
 * no god headers.
 */

#include "wubu_archd_internal.h"

#include <string.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <sys/socket.h>

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
