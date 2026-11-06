/* wubu_svc_socket_test.c -- P3 socket-activation contract (§4.2).
 * A real Unix-domain client connects; the socket fires its activator ONCE on
 * the first connection (on-demand service start), then the accepted fd is
 * available to the started service (systemd LISTEN_FDS delivery). Asserts:
 *   - activator fires exactly once across multiple connections
 *   - each pending connection is accept()able and yields a usable fd
 *   - a refused default causes poll to re-prime instead of dead-firing
 *   - the /n-visible inspection (name/path/activated/connections) is sane
 */
#include "wubu_svc_socket.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <errno.h>

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#define SOCKPATH "/tmp/wubu_svc_socket_test.sock"

static int g_pass = 0, g_fail = 0;
#define CHECK(cond, msg) do { if (cond) { g_pass++; } \
    else { g_fail++; printf("  FAIL: %s\n", msg); } } while (0)

static int g_activate_calls = 0;
static char g_last[64];
static int accept_gate = 1;   /* when 0, the activator REFUSES (re-prime) */
static int on_activate(wubu_svc_socket_t *sock, const char *name, void *ud) {
    (void)ud;
    g_activate_calls++;
    snprintf(g_last, sizeof(g_last), "%s:%.*s,%d", name,
             (int)strlen(wubu_svc_socket_path(sock)),
             wubu_svc_socket_path(sock), accept_gate);
    return accept_gate ? 0 : -1;
}

/* open a client connection to the Unix socket */
static int connect_client(void) {
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) return -1;
    struct sockaddr_un a;
    memset(&a, 0, sizeof(a));
    a.sun_family = AF_UNIX;
    snprintf(a.sun_path, sizeof(a.sun_path), "%s", SOCKPATH);
    if (connect(fd, (struct sockaddr *)&a, sizeof(a)) != 0) { close(fd); return -1; }
    return fd;
}

int main(void) {
    unlink(SOCKPATH);

    /* activator REFUSES first: poll must re-prime, not dead-fire */
    accept_gate = 0;
    wubu_svc_socket_t *s = wubu_svc_socket_create("cups", SOCKPATH);
    CHECK(s != NULL, "socket created (bind+listen)");
    wubu_svc_socket_set_activator(s, on_activate, NULL);

    int c1 = connect_client();
    CHECK(c1 >= 0, "first client connects to the Unix socket");

    int r = wubu_svc_socket_poll(s);
    CHECK(r == 0, "poll with a REFUSING activator returns 0 (not fired/primed)");
    CHECK(g_activate_calls == 1, "activator still consulted once");
    CHECK(!wubu_svc_socket_was_activated(s), "not marked activated on refusal");
    close(c1);
    /* accept the refused-but-present connection to drain it */
    int drained = wubu_svc_socket_accept(s);
    CHECK(drained >= 0, "connection drained after activation refusal");

    /* now the activator ACCEPTS: second connection fires activation once */
    accept_gate = 1;
    int c2 = connect_client();
    int c3 = connect_client();
    CHECK(c2 >= 0 && c3 >= 0, "two more clients connect");

    r = wubu_svc_socket_poll(s);
    CHECK(r == 1, "poll reports a pending connection");
    CHECK(g_activate_calls == 2, "activation fired again (fresh slot)");
    CHECK(wubu_svc_socket_was_activated(s), "socket now marked activated");
    CHECK(strncmp(g_last, "cups:", 5) == 0, "activator saw the socket name");

    /* both connections are accept()able; fds are real (writable byte) */
    int a1 = wubu_svc_socket_accept(s);
    int a2 = wubu_svc_socket_accept(s);
    CHECK(a1 >= 0 && a2 >= 0, "both pending connections accepted (LISTEN_FDS)");
    CHECK(wubu_svc_socket_connections(s) >= 2, "connection counter reflects accepted fds (>=2)");

    /* the accepted fd is genuinely usable: write a byte, client reads it */
    char sent = 'A';
    CHECK(write(a1, &sent, 1) == 1, "first accepted fd accepts a write (service owns it)");
    char got = 0;
    CHECK(read(c2, &got, 1) == 1 && got == 'A',
          "client c2 received the byte the service wrote over accepted fd");
    close(a1); close(a2); close(c2); close(c3);

    /* inspection */
    CHECK(strcmp(wubu_svc_socket_name(s), "cups") == 0, "name inspectable");
    CHECK(strcmp(wubu_svc_socket_path(s), SOCKPATH) == 0, "path inspectable");

    wubu_svc_socket_destroy(s);
    unlink(SOCKPATH);

    printf("\n=== Results: %d passed, %d failed ===\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
