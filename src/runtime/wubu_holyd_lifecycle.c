/*
 * wubu_holyd_lifecycle.c  --  WuBuOS HolyC DOS Daemon: Lifecycle
 */

#include "wubu_holyd_internal.h"

/* -- Daemon Lifecycle --------------------------------------------- */

int wubu_holyd_init(WubuHoly *d, const WubuHolyConfig *config) {
    if (!d || !config) return -1;
    memset(d, 0, sizeof(*d));
    d->config = *config;

    if (!d->config.sessions_path[0])
        strncpy(d->config.sessions_path, WUBU_HOLYD_SESSIONS_PATH, WUBU_HOLYD_MAX_PATH - 1);
    if (!d->config.socket_path[0])
        strncpy(d->config.socket_path, WUBU_HOLYD_SOCKET_PATH, WUBU_HOLYD_MAX_PATH - 1);
    if (!d->config.log_path[0])
        strncpy(d->config.log_path, WUBU_HOLYD_LOG_PATH, WUBU_HOLYD_MAX_PATH - 1);
    if (d->config.max_sessions <= 0)
        d->config.max_sessions = WUBU_HOLYD_MAX_SESSIONS;
    if (d->config.default_width <= 0) d->config.default_width = 800;
    if (d->config.default_height <= 0) d->config.default_height = 600;
    if (d->config.save_interval_sec <= 0) d->config.save_interval_sec = 300;

    mkdir(d->config.sessions_path, 0755);
    mkdir("/run/wubu", 0755);

    d->start_time = time(NULL);
    holyd_log(d, 2, "Holyd initialized: sessions_path=%s socket=%s",
              d->config.sessions_path, d->config.socket_path);
    return 0;
}

int wubu_holyd_start(WubuHoly *d) {
    if (!d) return -1;
    d->server_fd = holyd_socket_create(d);
    if (d->server_fd < 0) {
        holyd_log(d, 0, "Failed to create socket: %s", strerror(errno));
        return -1;
    }
    d->epoll_fd = epoll_create1(0);
    if (d->epoll_fd < 0) { close(d->server_fd); return -1; }

    struct epoll_event ev = { .events = EPOLLIN, .data.fd = d->server_fd };
    epoll_ctl(d->epoll_fd, EPOLL_CTL_ADD, d->server_fd, &ev);

    d->running = true;
    holyd_log(d, 2, "Holyd started on %s", d->config.socket_path);
    return 0;
}

void wubu_holyd_event_loop(WubuHoly *d) {
    if (!d || !d->running) return;

    struct epoll_event events[8];
    time_t last_autosave = 0;

    while (d->running) {
        int nfds = epoll_wait(d->epoll_fd, events, 8, 1000);
        time_t now = time(NULL);

        for (int i = 0; i < nfds; i++) {
            if (events[i].data.fd == d->server_fd) {
                int client = accept(d->server_fd, NULL, NULL);
                if (client >= 0) {
                    /* Set non-blocking mode via fcntl to avoid accept4/SOCK_NONBLOCK warning */
                    int flags = fcntl(client, F_GETFL, 0);
                    if (flags >= 0)
                        fcntl(client, F_SETFL, flags | O_NONBLOCK);
                    struct epoll_event cev = { .events = EPOLLIN | EPOLLERR | EPOLLHUP, .data.fd = client };
                    epoll_ctl(d->epoll_fd, EPOLL_CTL_ADD, client, &cev);
                }
            } else {
                int fd = events[i].data.fd;

                /* Check for error/hangup events first */
                if (events[i].events & (EPOLLERR | EPOLLHUP)) {
                    epoll_ctl(d->epoll_fd, EPOLL_CTL_DEL, fd, NULL);
                    close(fd);
                    continue;
                }

                char buf[8192];
                ssize_t n = read(fd, buf, sizeof(buf) - 1);
                if (n < 0) {
                    if (errno != EAGAIN && errno != EWOULDBLOCK) {
                        epoll_ctl(d->epoll_fd, EPOLL_CTL_DEL, fd, NULL);
                        close(fd);
                    }
                    continue;
                }
                if (n == 0) {
                    /* Client disconnected */
                    epoll_ctl(d->epoll_fd, EPOLL_CTL_DEL, fd, NULL);
                    close(fd);
                    continue;
                }

                buf[n] = '\0';
                /* Protocol: "CMD:session:data" */
                char cmd_str[64], sess[64], data[4096];
                cmd_str[0] = sess[0] = data[0] = '\0';
                sscanf(buf, "%63[^:]:%63[^:]:%4095[^\n]", cmd_str, sess, data);

                WubuHolyResponse resp = {0};
                d->requests_handled++;

                if (strcmp(cmd_str, "ping") == 0) {
                    resp.status = 0;
                    snprintf(resp.message, sizeof(resp.message), "pong");
                } else if (strcmp(cmd_str, "version") == 0) {
                    resp.status = 0;
                    snprintf(resp.data, sizeof(resp.data), "%s", WUBU_HOLYD_VERSION);
                } else if (strcmp(cmd_str, "stats") == 0) {
                    resp.status = 0;
                    snprintf(resp.data, sizeof(resp.data),
                             "{\"sessions\":%d,\"evals\":%lu,\"uptime\":%ld}",
                             d->session_count, d->evals_performed,
                             (long)(now - d->start_time));
                } else if (strcmp(cmd_str, "session_create") == 0) {
                    int w = 0, h = 0;
                    sscanf(data, "%d,%d", &w, &h);
                    resp.status = wubu_holyd_session_create(d, sess, w, h);
                } else if (strcmp(cmd_str, "session_destroy") == 0) {
                    resp.status = wubu_holyd_session_destroy(d, sess);
                } else if (strcmp(cmd_str, "session_list") == 0) {
                    resp.status = 0;
                    resp.data[0] = '\0';
                    for (int j = 0; j < d->session_count; j++) {
                        char entry[256];
                        int entry_len = snprintf(entry, sizeof(entry), "%s:%s:%d\n",
                                 d->sessions[j].name,
                                 wubu_holyd_session_state_str(d->sessions[j].state),
                                 d->sessions[j].window_count);
                        if (entry_len > 0 && (int)strlen(resp.data) + entry_len < (int)sizeof(resp.data) - 1)
                            strncat(resp.data, entry, sizeof(resp.data) - strlen(resp.data) - 1);
                    }
                } else if (strcmp(cmd_str, "session_focus") == 0) {
                    resp.status = wubu_holyd_session_focus(d, sess);
                } else if (strcmp(cmd_str, "eval") == 0) {
                    resp.status = wubu_holyd_eval(d, sess, data,
                                                   resp.output, sizeof(resp.output));
                } else if (strcmp(cmd_str, "window_create") == 0) {
                    int type = 0, x = 10, y = 10, w = 400, h = 300;
                    char title[128] = "HolyC Window";
                    sscanf(data, "%d,%d,%d,%d,%d,%127[^\n]", &type, &x, &y, &w, &h, title);
                    int wid = 0;
                    resp.status = wubu_holyd_window_create(d, sess, (WubuHolyWindowType)type,
                                                            x, y, w, h, title, &wid);
                    if (resp.status == 0) {
                        snprintf(resp.data, sizeof(resp.data), "%d", wid);
                    }
                } else if (strcmp(cmd_str, "window_destroy") == 0) {
                    resp.status = wubu_holyd_window_destroy(d, sess, atoi(data));
                } else if (strcmp(cmd_str, "window_show") == 0) {
                    resp.status = wubu_holyd_window_show(d, sess, atoi(data));
                } else if (strcmp(cmd_str, "window_hide") == 0) {
                    resp.status = wubu_holyd_window_hide(d, sess, atoi(data));
                } else if (strcmp(cmd_str, "window_focus") == 0) {
                    resp.status = wubu_holyd_window_focus(d, sess, atoi(data));
                } else if (strcmp(cmd_str, "input_key") == 0) {
                    int keycode = 0, modifiers = 0;
                    sscanf(data, "%d,%d", &keycode, &modifiers);
                    resp.status = wubu_holyd_input_key(d, sess, keycode, modifiers);
                } else if (strcmp(cmd_str, "session_save") == 0) {
                    resp.status = wubu_holyd_session_save(d, sess);
                } else if (strcmp(cmd_str, "mount") == 0) {
                    resp.status = wubu_holyd_mount(d, sess, data);
                } else if (strcmp(cmd_str, "unmount") == 0) {
                    resp.status = wubu_holyd_unmount(d, sess);
                } else if (strcmp(cmd_str, "shutdown") == 0) {
                    resp.status = 0;
                    d->running = false;
                } else {
                    resp.status = -1;
                    snprintf(resp.message, sizeof(resp.message),
                             "Unknown command: %s", cmd_str);
                    d->errors++;
                }

                /* Bounds-safe response: truncate to fit buffer */
                char resp_buf[WUBU_HOLYD_MAX_RESPONSE + 64];
                int resp_len = snprintf(resp_buf, sizeof(resp_buf), "%d:%s:%s:%s\n",
                         resp.status,
                         resp.message,
                         resp.output,
                         resp.data);
                if (resp_len > 0) {
                    ssize_t to_write = resp_len < (int)sizeof(resp_buf) ? resp_len : (int)sizeof(resp_buf) - 1;
                    write(fd, resp_buf, (size_t)to_write);
                }
                epoll_ctl(d->epoll_fd, EPOLL_CTL_DEL, fd, NULL);
                close(fd);
            }
        }

        /* Auto-save: use configured interval, default 60s if not set */
        int autosave_interval = d->config.save_interval_sec > 0 ? d->config.save_interval_sec : 60;
        if (now - last_autosave >= autosave_interval) {
            last_autosave = now;
            for (int j = 0; j < d->session_count; j++) {
                WubuHolySession *s = &d->sessions[j];
                if (s->state == SESSION_STATE_ACTIVE &&
                    s->save_interval_sec > 0 &&
                    (now - s->last_save) >= s->save_interval_sec) {
                    wubu_holyd_session_save(d, s->name);
                }
            }
        }
    }
}

void wubu_holyd_daemon_stop(WubuHoly *d) {
    if (!d) return;
    d->running = false;
    holyd_log(d, 2, "Holyd stopping");
}

void wubu_holyd_shutdown(WubuHoly *d) {
    if (!d) return;
    d->running = false;
    /* Save all active sessions, then free resources */
    for (int i = 0; i < d->session_count; i++) {
        if (d->sessions[i].state == SESSION_STATE_ACTIVE) {
            wubu_holyd_session_save(d, d->sessions[i].name);
        }
        /* Free all window framebuffers */
        for (int j = 0; j < d->sessions[i].window_count; j++) {
            if (d->sessions[i].windows[j].framebuffer) {
                free(d->sessions[i].windows[j].framebuffer);
                d->sessions[i].windows[j].framebuffer = NULL;
            }
        }
    }
    if (d->server_fd >= 0) close(d->server_fd);
    if (d->epoll_fd >= 0) close(d->epoll_fd);
    unlink(d->config.socket_path);
    holyd_log(d, 2, "Holyd shutdown complete");
}

/* -- Main Entry Point --------------------------------------------- */

#ifndef WUBD_TEST_MAIN
int main(int argc, char *argv[]) {
    WubuHolyConfig config = {0};
    strncpy(config.sessions_path, WUBU_HOLYD_SESSIONS_PATH, WUBU_HOLYD_MAX_PATH - 1);
    strncpy(config.socket_path, WUBU_HOLYD_SOCKET_PATH, WUBU_HOLYD_MAX_PATH - 1);
    strncpy(config.log_path, WUBU_HOLYD_LOG_PATH, WUBU_HOLYD_MAX_PATH - 1);
    config.max_sessions = WUBU_HOLYD_MAX_SESSIONS;
    config.default_width = 800;
    config.default_height = 600;
    config.save_interval_sec = 300;
    config.log_level = 2;
    config.daemonize = true;
    config.auto_mount_9p = true;
    config.debug_mode = false;

    int repl_mode = 0;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--no-daemon") == 0) config.daemonize = false;
        else if (strcmp(argv[i], "--repl") == 0) { repl_mode = 1; config.daemonize = false; }
        else if (strcmp(argv[i], "--debug") == 0) { config.log_level = 3; config.debug_mode = true; }
        else if (strcmp(argv[i], "--sessions") == 0 && i + 1 < argc) {
            strncpy(config.sessions_path, argv[++i], WUBU_HOLYD_MAX_PATH - 1);
        } else if (strcmp(argv[i], "--help") == 0) {
            printf("wubu_holyd -- WuBuOS TempleOS HolyC DOS Daemon\n");
            printf("  --no-daemon     Run in foreground\n");
            printf("  --repl          Run an interactive TTY HolyC REPL on stdin/stdout\n");
            printf("  --debug         Verbose logging + debug mode\n");
            printf("  --sessions PATH Sessions directory (default: %s)\n", WUBU_HOLYD_SESSIONS_PATH);
            return 0;
        }
    }

    signal(SIGPIPE, SIG_IGN);

    WubuHoly daemon;
    if (wubu_holyd_init(&daemon, &config) != 0) {
        fprintf(stderr, "Failed to initialize holyd\n");
        return 1;
    }

    /* Interactive TTY HolyC REPL: read lines from stdin, evaluate via the
     * real HolyC compiler (wubu_holyd_repl_eval), print results to stdout.
     * This is the REPL that the Desktop terminal embeds (E4). */
    if (repl_mode) {
        if (wubu_holyd_session_create(&daemon, "repl", 80, 24) != 0) {
            fprintf(stderr, "Failed to create REPL session\n");
            wubu_holyd_shutdown(&daemon);
            return 1;
        }
        if (wubu_holyd_repl_start(&daemon, "repl") != 0) {
            fprintf(stderr, "Failed to start REPL\n");
            wubu_holyd_shutdown(&daemon);
            return 1;
        }
        char line[4096];
        printf("$ "); fflush(stdout);
        while (fgets(line, sizeof(line), stdin)) {
            size_t n = strlen(line);
            while (n > 0 && (line[n-1] == '\n' || line[n-1] == '\r')) line[--n] = '\0';
            char out[8192];
            wubu_holyd_repl_eval(&daemon, "repl", line, out, sizeof(out));
            if (out[0]) { printf("%s\n", out); }
            printf("$ "); fflush(stdout);
        }
        wubu_holyd_shutdown(&daemon);
        return 0;
    }

    if (config.daemonize) {
        pid_t pid = fork();
        if (pid < 0) { perror("fork"); return 1; }
        if (pid > 0) { printf("wubu_holyd started (PID %d)\n", pid); return 0; }
        setsid();
        int devnull = open("/dev/null", O_RDWR);
        dup2(devnull, STDIN_FILENO);
        dup2(devnull, STDOUT_FILENO);
        dup2(devnull, STDERR_FILENO);
        close(devnull);
    }

    if (wubu_holyd_start(&daemon) != 0) {
        fprintf(stderr, "Failed to start holyd\n");
        return 1;
    }

    wubu_holyd_event_loop(&daemon);
    wubu_holyd_shutdown(&daemon);
    return 0;
}
#endif /* WUBD_TEST_MAIN */
