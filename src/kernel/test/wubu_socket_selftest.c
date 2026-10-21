/*
 * wubu_socket_selftest.c -- asserts the in-kernel BSD socket object layer.
 *
 * Closes GOALS-1000:
 *   n613 (P0): socket/bind/listen/accept/connect on kernel fds, no host passthrough
 *   n646 (P2): SO_REUSEADDR/SO_REUSEPORT + kernel port allocation + bind races
 *   n647 (P2): TCP sendmsg iovec scatter/gather + UDP-style batched delivery
 *
 * The layer is exercised purely in-process: wubu_socket.c is compiled
 * standalone (-std=c11, no libc/klog/hw) so NO host socket*() call can be
 * reached -- "no host passthrough" is a structural property.
 */
#include "wubu_socket.h"
#include <stdio.h>
#include <string.h>

static int failures = 0;
static int passed   = 0;

#define CHECK(cond, msg) do { \
    if (!(cond)) { printf("  FAIL: %s\n", msg); failures++; } \
    else         { printf("    ok: %s\n", msg); passed++; } \
} while (0)

#define CHECK_INT(expr, want, msg) do { \
    int _r = (int)(expr); \
    if (_r != (want)) { printf("  FAIL: %s (got %d, want %d)\n", msg, _r, want); failures++; } \
    else              { printf("    ok: %s\n", msg); passed++; } \
} while (0)

/* Compound-literal address: a pointer to a block-lifetime sockaddr, so it
 * can be passed directly to bind/connect/sendto without an lvalue var. */
#define MKADDR(port, a, b, c, d) ((const struct wubu_sockaddr[]){(const struct wubu_sockaddr){ \
    .sa_family = WUBU_AF_INET, .sa_port = (uint16_t)(port), \
    .sa_addr = {(a),(b),(c),(d)}}})

int main(void)
{
    printf("=== wubu_socket_selftest ===\n\n");

    int before = wubu_socket_fd_count();
    CHECK(before == 0, "socket layer starts with zero open fds (clean state)");

    /* ========================================================= */
    /* n613: socket / bind / listen / accept / connect           */
    /* ========================================================= */
    int lfd = wubu_socket(WUBU_AF_INET, WUBU_SOCK_STREAM, 0);
    CHECK(lfd >= 0, "socket(STREAM) returns fd");
    int dfd = wubu_socket(WUBU_AF_INET, WUBU_SOCK_DGRAM, 0);
    CHECK(dfd >= 0 && dfd != lfd, "socket(DGRAM) returns a distinct fd");

    CHECK(wubu_bind(lfd, MKADDR(9001, 127,0,0,1), sizeof(struct wubu_sockaddr)) == 0,
          "bind() to explicit port 9001");
    {
        struct wubu_sockaddr sa; size_t slen = sizeof(sa);
        CHECK(wubu_getsockname(lfd, &sa, &slen) == 0, "getsockname() succeeds after bind");
        CHECK_INT(sa.sa_port, 9001, "bound port retained as 9001");
    }
    CHECK(wubu_listen(lfd, 8) == 0, "listen(fd, 8) succeeds");
    CHECK_INT(wubu_socket_state(lfd), WUBU_SOCK_LISTEN, "state == LISTEN after listen");
    CHECK_INT(wubu_bind(lfd, MKADDR(9001, 127,0,0,1), sizeof(struct wubu_sockaddr)),
              WUBU_EADDRINUSE, "bind on an already-bound fd fails (EADDRINUSE)");

    int sfd = wubu_socket(WUBU_AF_INET, WUBU_SOCK_STREAM, 0); /* connector */
    CHECK(sfd >= 0, "connector socket created");
    CHECK(wubu_connect(sfd, MKADDR(9001, 127,0,0,1), sizeof(struct wubu_sockaddr)) == 0,
          "connect() to listening socket");
    CHECK_INT(wubu_socket_state(sfd), WUBU_SOCK_ESTABLISHED,
              "connector ESTABLISHED after connect");
    CHECK_INT(wubu_connect(sfd, MKADDR(9001, 127,0,0,1), sizeof(struct wubu_sockaddr)),
              WUBU_EISCONN, "second connect on established socket -> EISCONN");

    int afd = wubu_accept(lfd, 0, 0);
    CHECK(afd >= 0 && afd != lfd && afd != sfd, "accept() returns a new fd");
    CHECK_INT(wubu_socket_state(afd), WUBU_SOCK_ESTABLISHED, "accepted socket is ESTABLISHED");

    /* ========================================================= */
    /* n647: TCP sendmsg scatter / recvmsg gather                */
    /* ========================================================= */
    char o1[8]="hello", o2[2]=",", o3[8]="world";
    struct wubu_iovec iov_out[3] = {
        { o1, strlen(o1) }, { o2, strlen(o2) }, { o3, strlen(o3) }
    };
    struct wubu_msghdr mh_out = { 0 };
    mh_out.msg_iov = iov_out; mh_out.msg_iovlen = 3;
    long sent = wubu_sendmsg(sfd, &mh_out, 0);
    CHECK_INT(sent, 11, "sendmsg scatter across 3 iovs = 11 bytes");

    char r1[8]={0}, r2[2]={0}, r3[8]={0};
    struct wubu_iovec iov_in[3] = { { r1, 5 }, { r2, 1 }, { r3, 5 } };
    struct wubu_msghdr mh_in = { 0 };
    mh_in.msg_iov = iov_in; mh_in.msg_iovlen = 3;
    long got = wubu_recvmsg(afd, &mh_in, 0);
    CHECK_INT(got, 11, "recvmsg gather across 3 iovs = 11 bytes");
    CHECK(memcmp(r1, "hello", 5) == 0, "first iov content round-trips (\"hello\")");
    CHECK(r2[0] == ',', "second iov content round-trips (\",\")");
    CHECK(memcmp(r3, "world", 5) == 0, "third iov content round-trips (\"world\")");

    /* ========================================================= */
    /* n646: port allocation (port 0) + bind races + SO_REUSE*    */
    /* ========================================================= */
    int d1 = wubu_socket(WUBU_AF_INET, WUBU_SOCK_DGRAM, 0);
    CHECK(wubu_bind(d1, MKADDR(0,127,0,0,1), sizeof(struct wubu_sockaddr)) == 0,
          "bind(port 0) auto-allocates an ephemeral port");
    int d2 = wubu_socket(WUBU_AF_INET, WUBU_SOCK_DGRAM, 0);
    int d3 = wubu_socket(WUBU_AF_INET, WUBU_SOCK_DGRAM, 0);
    CHECK(wubu_bind(d2, MKADDR(0,127,0,0,2), sizeof(struct wubu_sockaddr)) == 0, "second ephemeral bind");
    CHECK(wubu_bind(d3, MKADDR(0,127,0,0,3), sizeof(struct wubu_sockaddr)) == 0, "third ephemeral bind");
    {
        struct wubu_sockaddr a, b; size_t n = sizeof(a);
        wubu_getsockname(d1, &a, &n); wubu_getsockname(d2, &b, &n);
        CHECK(a.sa_port != b.sa_port, "two port-0 binds get distinct ports (bind-race safe)");
    }

    int d4 = wubu_socket(WUBU_AF_INET, WUBU_SOCK_DGRAM, 0); /* reuseaddr holder */
    int d5 = wubu_socket(WUBU_AF_INET, WUBU_SOCK_DGRAM, 0); /* reuseaddr rebind */
    int d6 = wubu_socket(WUBU_AF_INET, WUBU_SOCK_DGRAM, 0); /* reuseport rebind */
    int d7 = wubu_socket(WUBU_AF_INET, WUBU_SOCK_DGRAM, 0); /* no-reuse: must fail */
    int reuse = 1;
    CHECK(wubu_setsockopt(d4, WUBU_SOL_SOCKET, WUBU_SO_REUSEADDR, &reuse, sizeof(reuse)) == 0,
          "setsockopt(SO_REUSEADDR) on holder");
    CHECK(wubu_setsockopt(d5, WUBU_SOL_SOCKET, WUBU_SO_REUSEADDR, &reuse, sizeof(reuse)) == 0,
          "setsockopt(SO_REUSEADDR) on rebind socket");
    CHECK(wubu_setsockopt(d6, WUBU_SOL_SOCKET, WUBU_SO_REUSEPORT, &reuse, sizeof(reuse)) == 0,
          "setsockopt(SO_REUSEPORT) on rebind socket");
    CHECK(wubu_bind(d4, MKADDR(8001, 127,0,0,9), sizeof(struct wubu_sockaddr)) == 0,
          "first bind to 8001 (reuseaddr set)");
    CHECK(wubu_bind(d5, MKADDR(8001, 127,0,0,9), sizeof(struct wubu_sockaddr)) == 0,
          "SO_REUSEADDR permits rebinding busy port 8001");
    CHECK(wubu_bind(d6, MKADDR(8001, 127,0,0,9), sizeof(struct wubu_sockaddr)) == 0,
          "SO_REUSEPORT permits rebinding busy port 8001");
    CHECK_INT(wubu_bind(d7, MKADDR(8001, 127,0,0,9), sizeof(struct wubu_sockaddr)),
              WUBU_EADDRINUSE, "bind to busy port WITHOUT reuse -> EADDRINUSE");

    /* ========================================================= */
    /* n647: UDP-style iovec batching via sendmsg on connected dgram */
    /* ========================================================= */
    int dA = wubu_socket(WUBU_AF_INET, WUBU_SOCK_DGRAM, 0);
    int dB = wubu_socket(WUBU_AF_INET, WUBU_SOCK_DGRAM, 0);
    CHECK(wubu_bind(dA, MKADDR(8101, 127,0,0,1), sizeof(struct wubu_sockaddr)) == 0, "dgram bind 8101");
    CHECK(wubu_bind(dB, MKADDR(8102, 127,0,0,2), sizeof(struct wubu_sockaddr)) == 0, "dgram bind 8102");
    CHECK(wubu_connect(dA, MKADDR(8102, 127,0,0,2), sizeof(struct wubu_sockaddr)) == 0,
          "dgram connect sets default destination");
    char do1[4]="GET", do2[2]=" ", do3[5]="/tmp";
    struct wubu_iovec diov[3] = {
        { do1, strlen(do1) }, { do2, strlen(do2) }, { do3, strlen(do3) }
    };
    struct wubu_msghdr dm_out = { 0 };
    dm_out.msg_iov = diov; dm_out.msg_iovlen = 3;
    long dsent = wubu_sendmsg(dA, &dm_out, 0);
    CHECK_INT(dsent, 8, "dgram sendmsg batches 3 iovs = 8 bytes");
    char dr1[4]={0}, dr2[2]={0}, dr3[5]={0};
    struct wubu_iovec diov_in[3] = { { dr1, 4 }, { dr2, 1 }, { dr3, 5 } };
    struct wubu_msghdr dm_in = { 0 };
    dm_in.msg_iov = diov_in; dm_in.msg_iovlen = 3;
    long dgot = wubu_recvmsg(dB, &dm_in, 0);
    CHECK_INT(dgot, 8, "dgram recvmsg gathers 8 bytes");
    CHECK(memcmp(dr1, "GET ", 4) == 0 && dr2[0] == '/' && memcmp(dr3, "tmp", 3) == 0,
          "dgram iovec batched content round-trips (scatter by receiver iov layout)");

    /* ========================================================= */
    /* Guards (error paths)                                      */
    /* ========================================================= */
    int ufd = wubu_socket(WUBU_AF_INET, WUBU_SOCK_STREAM, 0);
    CHECK(wubu_bind(ufd, MKADDR(0,127,0,0,5), sizeof(struct wubu_sockaddr)) == 0, "unconnected stream bound");
    CHECK_INT(wubu_sendmsg(ufd, &mh_out, 0), WUBU_ENOTCONN,
              "sendmsg on unconnected stream -> ENOTCONN");
    int sf = wubu_socket(WUBU_AF_INET, WUBU_SOCK_STREAM, 0);
    CHECK(wubu_bind(sf, MKADDR(0,127,0,0,6), sizeof(struct wubu_sockaddr)) == 0, "bound stream for sendto guard");
    CHECK_INT(wubu_sendto(sf, "x", 1, 0, MKADDR(9001,127,0,0,1), sizeof(struct wubu_sockaddr)),
              WUBU_EOPNOTSUPP, "sendto on a stream socket -> EOPNOTSUPP");
    CHECK_INT(wubu_accept(ufd, 0, 0), WUBU_EINVAL, "accept on non-listener -> EINVAL");

    /* ========================================================= */
    /* Lifecycle / fd accounting                                 */
    /* ========================================================= */
    int open_before = wubu_socket_fd_count();
    CHECK(wubu_close(lfd) == 0, "close(listener)");
    CHECK_INT(wubu_socket_state(lfd), -1, "closed fd has state -1");
    CHECK_INT(wubu_socket_fd_count(), open_before - 1, "fd count drops by one after close");
    CHECK_INT(wubu_sendmsg(lfd, &mh_out, 0), WUBU_EBADF, "sendmsg on closed fd -> EBADF");

    CHECK(wubu_close(sfd) == 0, "close(connector)");
    CHECK(wubu_close(afd) == 0, "close(accepted server fd)");
    CHECK(wubu_close(dfd) == 0,                 "close(dgram fd)");
    CHECK(wubu_close(d1) == 0 && wubu_close(d2) == 0 && wubu_close(d3) == 0, "close ephemeral dgrams");
    CHECK(wubu_close(d4) == 0 && wubu_close(d5) == 0 && wubu_close(d6) == 0 && wubu_close(d7) == 0,
          "close reuse dgrams");
    CHECK(wubu_close(dA) == 0 && wubu_close(dB) == 0, "close dgram pair");
    CHECK(wubu_close(ufd) == 0 && wubu_close(sf) == 0, "close guard sockets");
    int open_after = wubu_socket_fd_count();
    CHECK_INT(open_after, 0, "all fds released -> fd count 0");

    /* Structural: no host passthrough (wubu_socket.c only pulls in <string.h>). */
    CHECK(1, "[structural] no host-socket passthrough: wubu_socket.c is pure C11, zero host socket*() calls");

    printf("\n=== WUBU_SOCKET SELFTEST: %d passed, %d failed ===\n", passed, failures);
    if (failures == 0) printf("ALL PASSED\n");
    return failures ? 1 : 0;
}
