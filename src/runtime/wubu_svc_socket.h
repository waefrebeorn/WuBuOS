/*
 * wubu_svc_socket.h -- the socket-activation contract (Mint baseline §4.2).
 *
 * systemd socket activation: a `.socket` unit "encode[s] information about an
 * IPC or network socket ... controlled and supervised by systemd", and
 * "incoming traffic starts a matching `.service` on demand" — systemd hands
 * the accepted listening fd to the freshly-started service (LISTEN_FDS). This
 * is the P3 module that models that: a named listening socket that, on its
 * FIRST incoming connection, fires an ACTIVATION callback (the service-start
 * seam, normally wired to wubu_session_boot / wubu_bus), and subsequently lets
 * the activator claim the accepted fd.
 *
 * Scope: the SOCKET + ACTIVATION-TRIGGER contract over a Unix-domain socket
 * (host-testable, no privileges). A connection is queued; on the first one the
 * activator is called once (the service launches); afterwards connections are
 * accept()able AND passable to the activator so the started service can own
 * the fd (LISTEN_FDS delivery). Published under /n/background/sockets/<name>.
 *
 * C11, runtime host. Poll-based (no internal thread): call
 * wubu_svc_socket_poll() from your driver loop. Not thread-safe by design.
 */
#ifndef WUBU_SVC_SOCKET_H
#define WUBU_SVC_SOCKET_H

#include <stddef.h>
#include <stdint.h>

typedef struct wubu_svc_socket wubu_svc_socket_t;

/* Called on the FIRST connection for a socket (once). Return 0 = activation
 * accepted (service launching); nonzero = refuse (connections keep queuing).
 * When accepted, later connections may be accept()-ed and handed to the
 * service. */
typedef int (*wubu_svc_socket_activate_fn)(
    wubu_svc_socket_t *sock, const char *name, void *ud);

/* Lifecycle --------------------------------------------------------------- */
/* Create a socket that binds+listens on `path` (a Unix-domain socket file).
 * path is copied. Returns NULL on bind/listen failure. */
wubu_svc_socket_t *wubu_svc_socket_create(const char *name, const char *path);
void wubu_svc_socket_destroy(wubu_svc_socket_t *sock);

void wubu_svc_socket_set_activator(wubu_svc_socket_t *sock,
                                   wubu_svc_socket_activate_fn fn, void *ud);

/* Driver loop hook -------------------------------------------------------- */
/* Poll for a pending connection. Primes activation on the FIRST connection
 * (calls the activator exactly once if one is set); returns:
 *   1 if a connection is now pending/ready to accept,
 *   0 if none, -1 on error or a closed/unusable socket. Call repeatedly. */
int wubu_svc_socket_poll(wubu_svc_socket_t *sock);

/* accept() one queued connection and return its fd (the service can own it,
 * systemd LISTEN_FDS style). -1 if none pending. */
int wubu_svc_socket_accept(wubu_svc_socket_t *sock);

/* Inspection -------------------------------------------------------------- */
const char *wubu_svc_socket_name(const wubu_svc_socket_t *sock);
const char *wubu_svc_socket_path(const wubu_svc_socket_t *sock);
int wubu_svc_socket_was_activated(const wubu_svc_socket_t *sock);
int wubu_svc_socket_connections(const wubu_svc_socket_t *sock);

#endif /* WUBU_SVC_SOCKET_H */
