/*
 * wubu_socket.h -- In-kernel BSD socket object layer.
 *
 * Pure state-machine implementation of the BSD socket API surface
 * (socket/bind/listen/accept/connect/sendmsg/recvmsg) on kernel-style file
 * descriptors, with NO host-socket passthrough. The layer owns its socket
 * pool, its fd table, and its port bitmap, and implements the connection
 * state machine entirely in-process. In the live kernel the syscall bridge
 * (see interrupt_syscall.c) routes fd syscalls into these entry points; here
 * it is self-contained so it links and tests as a standalone C11 object.
 *
 * Capability contract (GOALS-1000 n613/n646/n647):
 *   n613: socket/bind/listen/accept/connect on kernel fds, no host passthrough
 *   n646: SO_REUSEADDR/SO_REUSEPORT + kernel port allocation + bind races
 *   n647: TCP sendmsg iovec scatter/gather + UDP-style batched delivery
 */
#ifndef WUBU_SOCKET_H
#define WUBU_SOCKET_H

#include <stddef.h>
#include <stdint.h>

/* Domains (minimal BSD-style set) */
#define WUBU_AF_INET   1
#define WUBU_AF_INET6  2
#define WUBU_AF_UNIX   3

/* Socket types (map to BSD SOCK_) */
#define WUBU_SOCK_STREAM 1   /* connection oriented (TCP-style) */
#define WUBU_SOCK_DGRAM  2   /* connectionless (UDP-style)     */
#define WUBU_SOCK_RAW    3   /* raw                            */

/* getsockopt/setsockopt levels + options */
#define WUBU_SOL_SOCKET     1
#define WUBU_SO_REUSEADDR   1
#define WUBU_SO_REUSEPORT   2
#define WUBU_SO_RCVBUF      3
#define WUBU_SO_SNDBUF      4

/* Negative errno-style returns (derived from BSD errno numbering) */
#define WUBU_EINVAL           (-1)
#define WUBU_EBADF            (-2)
#define WUBU_EADDRINUSE       (-3)
#define WUBU_EADDRNOTAVAIL    (-4)
#define WUBU_EISCONN          (-6)
#define WUBU_ENOTCONN         (-7)
#define WUBU_EALREADY         (-8)
#define WUBU_EINPROGRESS      (-9)
#define WUBU_ENOMEM           (-10)
#define WUBU_EOPNOTSUPP       (-13)
#define WUBU_EAGAIN           (-11)
#define WUBU_ECONNREFUSED     (-12)

/* Shutdown how */
#define WUBU_SHUT_RD    1
#define WUBU_SHUT_WR    2
#define WUBU_SHUT_RDWR  3

/* Socket lifecycle states (returned by wubu_socket_state) */
#define WUBU_SOCK_IDLE        1   /* created, not bound       */
#define WUBU_SOCK_BOUND       2   /* bound to an address      */
#define WUBU_SOCK_LISTEN      3   /* listening for connects   */
#define WUBU_SOCK_SYN_SENT    4   /* connect() in flight      */
#define WUBU_SOCK_ESTABLISHED 5   /* connection ready         */

/* Static pool sizes (no malloc on the metal path).
 * FD cap 256, socket pool 256 (1:1 with fd table).
 * Port bitmap covers the full 16-bit port space.                */
#define WUBU_FD_MAX         256
#define WUBU_SOCKET_MAX     256
#define WUBU_PORT_MAX       65536
#define WUBU_BACKLOG_MAX    64
#define WUBU_RECV_RING_MAX  512   /* per-socket receive ring (bytes) */

/* Socket address (BSD generic + inet flavor).
 * sa_addr carries raw address bytes; IPv4 uses [0..3].          */
struct wubu_sockaddr {
    uint16_t sa_family;
    uint16_t sa_port;         /* host byte order (0..65535) */
    uint8_t  sa_addr[16];     /* raw address bytes          */
};

/* Scatter/gather vector */
struct wubu_iovec {
    void  *iov_base;
    size_t iov_len;
};

/* Message header (BSD msghdr) */
struct wubu_msghdr {
    struct wubu_sockaddr *msg_name;
    size_t                msg_namelen;
    struct wubu_iovec    *msg_iov;
    int                   msg_iovlen;
    const void           *msg_control;
    size_t                msg_controllen;
    int                   msg_flags;
};

/* BSD-style API. fd is a kernel-style descriptor (index into the layer's
 * fd table). Returns are 0/-1/errno or a byte count, as in BSD.            */
int     wubu_socket(int domain, int type, int protocol);
int     wubu_bind(int fd, const struct wubu_sockaddr *addr, size_t addrlen);
int     wubu_listen(int fd, int backlog);
int     wubu_accept(int fd, struct wubu_sockaddr *addr, size_t *addrlen);
int     wubu_connect(int fd, const struct wubu_sockaddr *addr, size_t addrlen);
long    wubu_sendmsg(int fd, const struct wubu_msghdr *msg, int flags);
long    wubu_recvmsg(int fd, struct wubu_msghdr *msg, int flags);
long    wubu_sendto(int fd, const void *buf, size_t len, int flags,
                    const struct wubu_sockaddr *dest_addr, size_t addrlen);
int     wubu_setsockopt(int fd, int level, int optname,
                        const void *optval, size_t optlen);
int     wubu_getsockname(int fd, struct wubu_sockaddr *addr, size_t *addrlen);
int     wubu_close(int fd);
int     wubu_shutdown(int fd, int how);

/* Introspection helpers (selftest-only). */
int wubu_socket_fd_count(void);   /* # of currently-open fds            */
int wubu_socket_state(int fd);    /* enum socket_state of fd (or -1)    */

#endif /* WUBU_SOCKET_H */
