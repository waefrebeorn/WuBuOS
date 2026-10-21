/*
 * wubu_socket.c -- In-kernel BSD socket object layer.
 *
 * State-machine implementation of socket/bind/listen/accept/connect/
 * sendmsg/recvmsg on kernel-style file descriptors. NO host-socket
 * passthrough: the layer owns its socket pool, fd table, and port bitmap,
 * and runs the connection state machine entirely in-process. The live
 * kernel routes fd syscalls into these entry points via interrupt_syscall.c;
 * here it is self-contained so it compiles and tests as a standalone C11
 * object (-std=c11, no libc/klog/hardware dependency), mirroring how
 * txfs.c / wubu_kvfs.c self-test in src/kernel/test/wubu_socket_selftest.c.
 */
#include "wubu_socket.h"
#include <string.h>   /* memset / memcpy only */

/* ------------------------------------------------------------------ */
/* Connection states (internal). Header exposes the public integer set.*/
/* ------------------------------------------------------------------ */
enum {
    SK_FREE        = 0,
    SK_IDLE        = WUBU_SOCK_IDLE,
    SK_BOUND       = WUBU_SOCK_BOUND,
    SK_LISTEN      = WUBU_SOCK_LISTEN,
    SK_SYN_SENT    = WUBU_SOCK_SYN_SENT,
    SK_ESTABLISHED = WUBU_SOCK_ESTABLISHED,
};

/* ------------------------------------------------------------------ */
/* The socket object: an opaque, self-contained kernel-managed entity. */
/* The public handle is an `int fd`; this struct lives only in the .c. */
/* ------------------------------------------------------------------ */
struct wubu_socket {
    int                 fd;            /* owner fd (or -1 if free slot)  */
    int                 domain;        /* WUBU_AF_*                      */
    int                 type;          /* WUBU_SOCK_*                    */
    int                 proto;         /* 0 = default                     */
    int                 state;         /* enum SK_*                       */
    int                 reuseaddr;     /* SO_REUSEADDR latch              */
    int                 reuseport;     /* SO_REUSEPORT latch              */
    int                 backlog;       /* listen() backlog cap            */
    struct wubu_sockaddr local;        /* bound local address             */
    struct wubu_socket  *peer;         /* connected peer (stream/dgram)   */
    /* accept queue for LISTEN sockets (circular, of socket ptrs).    */
    struct wubu_socket  *accept_q[WUBU_BACKLOG_MAX];
    int                 accept_head;
    int                 accept_tail;
    int                 accept_len;
    /* receive ring (circular byte buffer).                          */
    unsigned char        ring[WUBU_RECV_RING_MAX];
    size_t               rp;           /* read  (consumer) index          */
    size_t               wp;           /* write (producer) index          */
};

/* ------------------------------------------------------------------ */
/* Static pools. The fd table maps fd -> socket; the pool holds socket */
/* storage. Zero/initialized at C runtime (SK_FREE == 0).             */
/* ------------------------------------------------------------------ */
static struct wubu_socket  g_pool[WUBU_SOCKET_MAX];
static struct wubu_socket *g_fds[WUBU_FD_MAX];
static unsigned char       g_port_busy[WUBU_PORT_MAX / 8];

/* ------------------------------------------------------------------ */
/* Port bitmap helpers. A port is reserved BEFORE assignment so two    */
/* concurrent bind() races cannot both win (n646 bind-race semantics).*/
/* ------------------------------------------------------------------ */
static int  port_test(int p) { return (g_port_busy[p >> 3] >> (p & 7)) & 1; }
static void port_set(int p)  { g_port_busy[p >> 3] |=  (unsigned char)(1u << (p & 7)); }
static void port_clr(int p)  { g_port_busy[p >> 3] &= (unsigned char)~(1u << (p & 7)); }

/* Allocate an ephemeral port atomically: scan + reserve in one step. */
static int port_alloc(void)
{
    for (int p = 1024; p < WUBU_PORT_MAX; p++) {
        if (!port_test(p)) { port_set(p); return p; }
    }
    return -1;
}

/* ------------------------------------------------------------------ */
/* Socket pool + fd table allocation.                                 */
/* ------------------------------------------------------------------ */
static struct wubu_socket *sock_alloc(void)
{
    for (int i = 0; i < WUBU_SOCKET_MAX; i++) {
        if (g_pool[i].state == SK_FREE) {
            struct wubu_socket *s = &g_pool[i];
            memset(s, 0, sizeof(*s));
            s->fd    = -1;
            s->state = SK_IDLE;
            return s;
        }
    }
    return 0;
}

static int fd_alloc(struct wubu_socket *s)
{
    for (int fd = 0; fd < WUBU_FD_MAX; fd++) {
        if (!g_fds[fd]) {
            g_fds[fd] = s;
            s->fd = fd;
            return fd;
        }
    }
    return -1;
}

static struct wubu_socket *sock_from_fd(int fd)
{
    if (fd < 0 || fd >= WUBU_FD_MAX) return 0;
    return g_fds[fd];
}

static void sock_free(struct wubu_socket *s)
{
    if (!s || s->fd < 0) return;
    g_fds[s->fd] = 0;
    if (s->local.sa_family && s->state >= SK_BOUND && s->local.sa_port)
        port_clr(s->local.sa_port);
    s->fd         = -1;
    s->state      = SK_FREE;
    s->peer       = 0;
    s->accept_len = 0;
}

/* ------------------------------------------------------------------ */
/* Address helper.                                                    */
/* ------------------------------------------------------------------ */
static int addr_eq(const struct wubu_sockaddr *a, const struct wubu_sockaddr *b)
{
    return a->sa_family == b->sa_family &&
           a->sa_port   == b->sa_port   &&
           memcmp(a->sa_addr, b->sa_addr, sizeof(a->sa_addr)) == 0;
}

/* ------------------------------------------------------------------ */
/* wubu_socket: create a kernel fd carrying a new socket object.      */
/* ------------------------------------------------------------------ */
int wubu_socket(int domain, int type, int protocol)
{
    if (domain != WUBU_AF_INET && domain != WUBU_AF_INET6 && domain != WUBU_AF_UNIX)
        return WUBU_EINVAL;
    if (type != WUBU_SOCK_STREAM && type != WUBU_SOCK_DGRAM && type != WUBU_SOCK_RAW)
        return WUBU_EINVAL;
    if (type == WUBU_SOCK_RAW) return WUBU_EOPNOTSUPP; /* raw not yet modeled */

    struct wubu_socket *s = sock_alloc();
    if (!s) return WUBU_ENOMEM;
    s->domain = domain;
    s->type   = type;
    s->proto  = protocol;
    int fd = fd_alloc(s);
    if (fd < 0) { sock_free(s); return WUBU_ENOMEM; }
    return fd;
}

/* ------------------------------------------------------------------ */
/* wubu_bind: bind fd to local (addr,port). Port 0 => kernel allocates*/
/* an ephemeral port (n646). Bind races are prevented by reserving the */
/* port bit before committing the address. SO_REUSEADDR/SO_REUSEPORT  */
/* permits rebinding a port already held (n646).                      */
/* ------------------------------------------------------------------ */
int wubu_bind(int fd, const struct wubu_sockaddr *addr, size_t addrlen)
{
    struct wubu_socket *s = sock_from_fd(fd);
    if (!s) return WUBU_EBADF;
    if (s->state != SK_IDLE) return WUBU_EADDRINUSE; /* already bound / listening */
    if (!addr || addrlen < sizeof(struct wubu_sockaddr)) return WUBU_EINVAL;
    if (addr->sa_family != s->domain) return WUBU_EINVAL;

    int port = addr->sa_port;
    if (port == 0) {
        port = port_alloc();                 /* ephemeral auto-allocation */
        if (port < 0) return WUBU_EADDRNOTAVAIL;
    } else {
        if (port < 0 || port >= WUBU_PORT_MAX) return WUBU_EINVAL;
        /* Reserve first, assign second: closes the bind-race window.  */
        if (port_test(port)) {
            if (s->reuseaddr || s->reuseport) {
                /* permissive BSD reuse: rebind an in-use port */
            } else {
                return WUBU_EADDRINUSE;
            }
        }
        port_set(port);
    }
    s->local = *addr;
    s->local.sa_port = (uint16_t)port;
    s->state = SK_BOUND;
    return 0;
}

/* ------------------------------------------------------------------ */
/* wubu_listen: transition BOUND -> LISTEN with a backlog cap.        */
/* ------------------------------------------------------------------ */
int wubu_listen(int fd, int backlog)
{
    struct wubu_socket *s = sock_from_fd(fd);
    if (!s) return WUBU_EBADF;
    if (s->state != SK_BOUND) return WUBU_EINVAL;
    s->backlog = backlog < 0 ? 0
               : (backlog > WUBU_BACKLOG_MAX ? WUBU_BACKLOG_MAX : backlog);
    s->state = SK_LISTEN;
    s->accept_head = s->accept_tail = s->accept_len = 0;
    return 0;
}

/* ------------------------------------------------------------------ */
/* Accept-queue ops for LISTEN sockets.                               */
/* ------------------------------------------------------------------ */
static int accept_enqueue(struct wubu_socket *l, struct wubu_socket *c)
{
    if (l->accept_len >= WUBU_BACKLOG_MAX) return -1;
    int pos = (l->accept_head + l->accept_len) % WUBU_BACKLOG_MAX;
    l->accept_q[pos] = c;
    l->accept_len++;
    return 0;
}

static struct wubu_socket *accept_dequeue(struct wubu_socket *l)
{
    if (l->accept_len == 0) return 0;
    struct wubu_socket *c = l->accept_q[l->accept_head];
    l->accept_q[l->accept_head] = 0;
    l->accept_head = (l->accept_head + 1) % WUBU_BACKLOG_MAX;
    l->accept_len--;
    return c;
}

/* ------------------------------------------------------------------ */
/* wubu_connect: pair this socket with a destination at (addr,port).  */
/* STREAM: must be a LISTEN socket; a pending server-side socket is  */
/*   created, linked both ways, and handed to the listener's accept   */
/*   queue (no host handshake).                                       */
/* DGRAM:  any bound datagram socket acts as the connected peer.      */
/* ------------------------------------------------------------------ */
int wubu_connect(int fd, const struct wubu_sockaddr *addr, size_t addrlen)
{
    struct wubu_socket *s = sock_from_fd(fd);
    if (!s) return WUBU_EBADF;
    if (!addr || addrlen < sizeof(struct wubu_sockaddr)) return WUBU_EINVAL;
    if (s->state == SK_ESTABLISHED) return WUBU_EISCONN;

    struct wubu_socket *dst = 0;
    for (int i = 0; i < WUBU_SOCKET_MAX; i++) {
        struct wubu_socket *c = &g_pool[i];
        if (!addr_eq(&c->local, addr)) continue;
        if (s->type == WUBU_SOCK_STREAM && c->state == SK_LISTEN) { dst = c; break; }
        if (s->type == WUBU_SOCK_DGRAM  && c->state >= SK_BOUND &&
            c->type == WUBU_SOCK_DGRAM) { dst = c; break; }
    }
    if (!dst) return WUBU_ECONNREFUSED; /* no route to a peer */

    s->state = SK_SYN_SENT;

    if (s->type == WUBU_SOCK_STREAM) {
        /* Build the server-side half of the connection and enqueue it on
         * the listener so accept() yields a fresh fd for it. */
        struct wubu_socket *p = sock_alloc();
        if (!p) return WUBU_ENOMEM;
        p->domain = s->domain;
        p->type   = s->type;
        p->proto  = s->proto;
        p->local  = dst->local;   /* server's bound address            */
        p->peer   = s;            /* P.peer = connector                */
        p->state  = SK_ESTABLISHED;
        (void)accept_enqueue(dst, p);
        s->peer   = p;            /* connector's peer = server half    */
        s->state  = SK_ESTABLISHED;
    } else {
        /* DGRAM connect sets a default destination (peer both ways). */
        dst->peer = s;
        s->peer   = dst;
        s->state  = SK_ESTABLISHED;
    }
    return 0;
}

/* ------------------------------------------------------------------ */
/* wubu_accept: dequeue a pending connection from a LISTEN socket and */
/* hand the caller a new fd for the server side of the connection.    */
/* ------------------------------------------------------------------ */
int wubu_accept(int fd, struct wubu_sockaddr *addr, size_t *addrlen)
{
    struct wubu_socket *s = sock_from_fd(fd);
    if (!s) return WUBU_EBADF;
    if (s->state != SK_LISTEN) return WUBU_EINVAL;

    struct wubu_socket *c = accept_dequeue(s);
    if (!c) return WUBU_EAGAIN; /* no pending connections */

    int newfd = fd_alloc(c);             /* give the pending socket an fd */
    if (newfd < 0) return WUBU_ENOMEM;
    if (addr) *addr = s->local;          /* server's bound address      */
    if (addrlen) *addrlen = sizeof(struct wubu_sockaddr);
    return newfd;
}

/* ------------------------------------------------------------------ */
/* Receive-ring primitives (the "wire" between two connected sockets). */
/* ring_put returns bytes written (may be partial if the ring is full).*/
/* ------------------------------------------------------------------ */
static size_t ring_put(struct wubu_socket *s, const unsigned char *data, size_t n)
{
    size_t used = (s->wp - s->rp) % WUBU_RECV_RING_MAX;
    size_t free_ = WUBU_RECV_RING_MAX - used;
    if (free_ == 0) return 0;
    if (n > free_) n = free_;
    for (size_t i = 0; i < n; i++)
        s->ring[(s->wp + i) % WUBU_RECV_RING_MAX] = data[i];
    s->wp += n;
    return n;
}

static size_t ring_get(struct wubu_socket *s, unsigned char *out, size_t n)
{
    size_t avail = (s->wp - s->rp) % WUBU_RECV_RING_MAX;
    if (avail == 0) return 0;
    if (n > avail) n = avail;
    for (size_t i = 0; i < n; i++)
        out[i] = s->ring[(s->rp + i) % WUBU_RECV_RING_MAX];
    s->rp += n;
    return n;
}

/* ------------------------------------------------------------------ */
/* wubu_sendmsg / wubu_recvmsg: scatter/gather over the peer ring.    */
/* Stream (ESTABLISHED) sockets scatter caller iovecs into the peer's */
/* receive ring; recvmsg gathers from this socket's ring. This is the */
/* TCP iovec path (n647).                                             */
/* ------------------------------------------------------------------ */
long wubu_sendmsg(int fd, const struct wubu_msghdr *msg, int flags)
{
    (void)flags;
    struct wubu_socket *s = sock_from_fd(fd);
    if (!s) return WUBU_EBADF;
    if (s->state != SK_ESTABLISHED || !s->peer) return WUBU_ENOTCONN;
    if (!msg || !msg->msg_iov) return WUBU_EINVAL;

    long total = 0;
    for (int i = 0; i < msg->msg_iovlen; i++) {
        const struct wubu_iovec *io = &msg->msg_iov[i];
        size_t written = ring_put(s->peer, (const unsigned char *)io->iov_base, io->iov_len);
        total += (long)written;
        if (written < io->iov_len) break; /* peer ring full: partial, stop */
    }
    return total;
}

long wubu_recvmsg(int fd, struct wubu_msghdr *msg, int flags)
{
    (void)flags;
    struct wubu_socket *s = sock_from_fd(fd);
    if (!s) return WUBU_EBADF;
    if (!msg || !msg->msg_iov) return WUBU_EINVAL;

    long total = 0;
    for (int i = 0; i < msg->msg_iovlen; i++) {
        struct wubu_iovec *io = &msg->msg_iov[i];
        size_t cap = io->iov_len;
        size_t got = ring_get(s, (unsigned char *)io->iov_base, cap);
        total += (long)got;
        io->iov_len = got;                 /* reflects bytes actually filled */
        if (got < cap) break;              /* ring drained: stop */
    }
    return total;
}

/* ------------------------------------------------------------------ */
/* wubu_sendto: UDP-style delivery to a bound datagram peer. Delivers */
/* caller bytes into the matching socket's receive ring. sendto on a  */
/* stream socket is unsupported (returns EOPNOTSUPP). The iovec form of*/
/* batched delivery on connected dgrams is exercised via wubu_sendmsg.*/
/* ------------------------------------------------------------------ */
long wubu_sendto(int fd, const void *buf, size_t len, int flags,
                 const struct wubu_sockaddr *dest_addr, size_t addrlen)
{
    (void)flags; (void)addrlen;
    struct wubu_socket *s = sock_from_fd(fd);
    if (!s) return WUBU_EBADF;
    if (s->type == WUBU_SOCK_STREAM) return WUBU_EOPNOTSUPP; /* stream uses sendmsg */
    if (!buf || !dest_addr) return WUBU_EINVAL;

    struct wubu_socket *peer = 0;
    for (int i = 0; i < WUBU_SOCKET_MAX; i++) {
        struct wubu_socket *c = &g_pool[i];
        if (c->state >= SK_BOUND && c->type == WUBU_SOCK_DGRAM &&
            addr_eq(&c->local, dest_addr)) { peer = c; break; }
    }
    if (!peer) return WUBU_ECONNREFUSED;

    size_t written = ring_put(peer, (const unsigned char *)buf, len);
    return (long)written;
}

/* ------------------------------------------------------------------ */
/* wubu_setsockopt: SO_REUSEADDR / SO_REUSEPORT (n646).               */
/* ------------------------------------------------------------------ */
int wubu_setsockopt(int fd, int level, int optname, const void *optval, size_t optlen)
{
    struct wubu_socket *s = sock_from_fd(fd);
    if (!s) return WUBU_EBADF;
    if (!optval || optlen == 0) return WUBU_EINVAL;
    if (level != WUBU_SOL_SOCKET) return WUBU_EINVAL;

    switch (optname) {
    case WUBU_SO_REUSEADDR: s->reuseaddr = *(const int *)optval ? 1 : 0; return 0;
    case WUBU_SO_REUSEPORT: s->reuseport = *(const int *)optval ? 1 : 0; return 0;
    case WUBU_SO_RCVBUF:  return 0; /* ring sized at compile time */
    case WUBU_SO_SNDBUF:  return 0; /* ring sized at compile time */
    default: return WUBU_EINVAL;
    }
}

/* ------------------------------------------------------------------ */
/* wubu_getsockname: return the bound local address.                  */
/* ------------------------------------------------------------------ */
int wubu_getsockname(int fd, struct wubu_sockaddr *addr, size_t *addrlen)
{
    struct wubu_socket *s = sock_from_fd(fd);
    if (!s) return WUBU_EBADF;
    if (!addr) return WUBU_EINVAL;
    *addr = s->local;
    if (addrlen) *addrlen = sizeof(struct wubu_sockaddr);
    return 0;
}

/* ------------------------------------------------------------------ */
/* wubu_shutdown / wubu_close.                                        */
/* ------------------------------------------------------------------ */
int wubu_shutdown(int fd, int how)
{
    struct wubu_socket *s = sock_from_fd(fd);
    if (!s) return WUBU_EBADF;
    if (s->state < SK_BOUND) return WUBU_ENOTCONN;
    if (how == WUBU_SHUT_RD || how == WUBU_SHUT_RDWR) {
        s->rp = s->wp = 0;                 /* drain the receive ring */
    }
    if (how == WUBU_SHUT_WR || how == WUBU_SHUT_RDWR) {
        if (s->peer) { s->peer->peer = 0; s->peer = 0; }
        if (s->type == WUBU_SOCK_STREAM) s->state = SK_FREE;
    }
    return 0;
}

int wubu_close(int fd)
{
    struct wubu_socket *s = sock_from_fd(fd);
    if (!s) return WUBU_EBADF;
    if (s->peer) s->peer->peer = 0;        /* detach peer linkage     */
    sock_free(s);
    return 0;
}

/* ------------------------------------------------------------------ */
/* Introspection (selftest-only).                                     */
/* ------------------------------------------------------------------ */
int wubu_socket_fd_count(void)
{
    int n = 0;
    for (int i = 0; i < WUBU_FD_MAX; i++) if (g_fds[i]) n++;
    return n;
}

int wubu_socket_state(int fd)
{
    struct wubu_socket *s = sock_from_fd(fd);
    return s ? s->state : -1;
}
