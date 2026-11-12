/*
 * wubu_svc_socket.c -- socket-activation contract (P3).
 *
 * A named Unix-domain listening socket. On the FIRST poll()-discovered
 * connection it fires the activator ONCE (the on-demand service start);
 * afterwards connections are accept()able and passable to the started
 * service. C11, runtime host, poll-based.
 */
#include "../egojar.h"
#include "../egojar_net.h"
#include <sys/socket.h>
#include "wubu_svc_socket.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

struct wubu_svc_socket {
    char name[64];
    char path[256];
    int  fd;                  /* listening fd, or -1 */
    int  activated;           /* activator fired */
    int  n_connections;       /* total accepted */
    wubu_svc_socket_activate_fn activate;
    void *activate_ud;
};

wubu_svc_socket_t *wubu_svc_socket_create(const char *name, const char *path) {
    if (!name || !path) return NULL;
    wubu_svc_socket_t *s = (wubu_svc_socket_t *)calloc(1, sizeof(*s));
    if (!s) return NULL;
    snprintf(s->name, sizeof(s->name), "%s", name);
    snprintf(s->path, sizeof(s->path), "%s", path);

    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) { free(s); return NULL; }

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    snprintf(addr.sun_path, sizeof(addr.sun_path), "%s", path);
    unlink(path);                       /* reuse stale socket files */
    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
        close(fd); free(s); return NULL;
    }
    if (listen(fd, 8) != 0) {
        close(fd); unlink(path); free(s); return NULL;
    }
    s->fd = fd;
    return s;
}

void wubu_svc_socket_destroy(wubu_svc_socket_t *sock) {
    if (!sock) return;
    if (sock->fd >= 0) { close(sock->fd); }
    unlink(sock->path);
    free(sock);
}

void wubu_svc_socket_set_activator(wubu_svc_socket_t *sock,
                                   wubu_svc_socket_activate_fn fn, void *ud) {
    sock->activate = fn;
    sock->activate_ud = ud;
}

int wubu_svc_socket_poll(wubu_svc_socket_t *sock) {
    if (!sock || sock->fd < 0) return -1;

    /* does a connection already exist? */
    struct pollfd p;
    p.fd = sock->fd;
    p.events = POLLIN;
    p.revents = 0;
    if (poll(&p, 1, 0) <= 0) return 0;

    /* first connection primes on-demand activation */
    if (!sock->activated) {
        sock->activated = 1;
        if (sock->activate) {
            if (sock->activate(sock, sock->name, sock->activate_ud) != 0) {
                /* service refused to start: stop priming (retry next poll) */
                sock->activated = 0;
                return 0;
            }
        }
    }
    return 1;
}

int wubu_svc_socket_accept(wubu_svc_socket_t *sock) {
    if (!sock || sock->fd < 0) return -1;
    int c = accept(sock->fd, NULL, NULL);
    if (c >= 0) sock->n_connections++;
    return c;
}

const char *wubu_svc_socket_name(const wubu_svc_socket_t *sock) {
    return sock ? sock->name : NULL;
}
const char *wubu_svc_socket_path(const wubu_svc_socket_t *sock) {
    return sock ? sock->path : NULL;
}
int wubu_svc_socket_was_activated(const wubu_svc_socket_t *sock) {
    return sock ? sock->activated : 0;
}
int wubu_svc_socket_connections(const wubu_svc_socket_t *sock) {
    return sock ? sock->n_connections : 0;
}
