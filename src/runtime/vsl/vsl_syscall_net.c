/*
 * vsl_syscall_net.c  --  VSL Socket, Signal, Timer, Namespace & Misc Syscalls
 * Socket family, rt_sigaction/procmask, futex, timers, eventfd, inotify,
 * signalfd, pselect/ppoll/select, pipe2, clone3, io_uring, *at family,
 * namespace/security, readlinkat, etc.
 */

#include "vsl_syscall_internal.h"

/* ====================================================================
 * SIGNAL SYSCALLS
 * ==================================================================== */

int64_t vsl_sys_sigaction(uint64_t signum, uint64_t act, uint64_t oldact,
                           uint64_t d, uint64_t e, uint64_t f) {
    (void)d; (void)e; (void)f;
    struct sigaction sa, old_sa;
    struct sigaction *sa_ptr = act ? &sa : NULL;
    struct sigaction *old_ptr = oldact ? &old_sa : NULL;
    if (act) memcpy(&sa, (void *)act, sizeof(struct sigaction));
    int rc = sigaction((int)signum, sa_ptr, old_ptr);
    if (rc < 0) return -errno;
    if (oldact) memcpy((void *)oldact, &old_sa, sizeof(struct sigaction));
    return 0;
}

int64_t vsl_sys_sigprocmask(uint64_t how, uint64_t set, uint64_t oldset,
                             uint64_t d, uint64_t e, uint64_t f) {
    (void)d; (void)e; (void)f;
    sigset_t ss, old_ss;
    sigset_t *ss_ptr = set ? &ss : NULL;
    sigset_t *old_ptr = oldset ? &old_ss : NULL;
    if (set) memcpy(&ss, (const void *)(uintptr_t)set, sizeof(sigset_t));
    int rc = sigprocmask((int)how, ss_ptr, old_ptr);
    if (rc < 0) return -errno;
    if (oldset) memcpy((void *)oldset, &old_ss, sizeof(sigset_t));
    return 0;
}

int64_t vsl_sys_sigreturn(uint64_t a, uint64_t b, uint64_t c,
                           uint64_t d, uint64_t e, uint64_t f) {
    (void)a; (void)b; (void)c; (void)d; (void)e; (void)f;
    long result = syscall(SYS_rt_sigreturn);
    return result < 0 ? -errno : result;
}

int64_t vsl_sys_rt_sigaction(uint64_t signum, uint64_t act, uint64_t oldact,
                              uint64_t d, uint64_t e, uint64_t f) {
    return vsl_sys_sigaction(signum, act, oldact, d, e, f);
}

int64_t vsl_sys_rt_sigprocmask(uint64_t how, uint64_t set, uint64_t oldset,
                                uint64_t d, uint64_t e, uint64_t f) {
    return vsl_sys_sigprocmask(how, set, oldset, d, e, f);
}

int64_t vsl_sys_futex(uint64_t uaddr, uint64_t op, uint64_t val,
                      uint64_t timeout, uint64_t uaddr2, uint64_t val3) {
    int *addr = (int *)uaddr;
    int rc = (int)syscall(SYS_futex, addr, (int)op, (int)val,
                          (struct timespec *)timeout, (int *)uaddr2, (int)val3);
    return rc < 0 ? -errno : (int64_t)rc;
}

int64_t vsl_sys_rt_sigsuspend(uint64_t mask, uint64_t d, uint64_t e,
                               uint64_t f, uint64_t g, uint64_t h) {
    (void)d; (void)e; (void)f; (void)g; (void)h;
    sigset_t ss;
    memcpy(&ss, (void *)mask, sizeof(sigset_t));
    int result = syscall(SYS_rt_sigsuspend, &ss, sizeof(sigset_t));
    return result < 0 ? -errno : (int64_t)result;
}

int64_t vsl_sys_rt_sigpending(uint64_t set, uint64_t sigsetsize,
                               uint64_t d, uint64_t e, uint64_t f, uint64_t g) {
    (void)d; (void)e; (void)f; (void)g;
    sigset_t ss;
    int result = syscall(SYS_rt_sigpending, &ss, (size_t)sigsetsize);
    if (result < 0) return -errno;
    if (set) memcpy((void *)set, &ss, sizeof(sigset_t));
    return 0;
}

int64_t vsl_sys_rt_sigtimedwait(uint64_t uthese, uint64_t uinfo, uint64_t uts,
                                 uint64_t usize, uint64_t e, uint64_t f) {
    (void)e; (void)f;
    const sigset_t *uthese_ptr = uthese ? (const sigset_t *)uthese : NULL;
    siginfo_t *uinfo_ptr = uinfo ? (siginfo_t *)uinfo : NULL;
    const struct timespec *uts_ptr = uts ? (const struct timespec *)uts : NULL;
    int result = syscall(SYS_rt_sigtimedwait, uthese_ptr, uinfo_ptr, uts_ptr, (size_t)usize);
    return result < 0 ? -errno : (int64_t)result;
}

int64_t vsl_sys_rt_sigqueueinfo(uint64_t pid, uint64_t sig, uint64_t uinfo,
                                 uint64_t d, uint64_t e, uint64_t f) {
    (void)d; (void)e; (void)f;
    siginfo_t info;
    memcpy(&info, (void *)uinfo, sizeof(siginfo_t));
    int result = syscall(SYS_rt_sigqueueinfo, (int)pid, (int)sig, &info);
    return result < 0 ? -errno : (int64_t)result;
}

/* ====================================================================
 * SOCKET SYSCALLS
 * ==================================================================== */

int64_t vsl_sys_socket(uint64_t domain, uint64_t type, uint64_t protocol,
                        uint64_t d, uint64_t e, uint64_t f) {
    (void)d; (void)e; (void)f;
    int result = (int)socket((int)domain, (int)type, (int)protocol);
    return result < 0 ? -errno : (int64_t)result;
}

int64_t vsl_sys_socketpair(uint64_t domain, uint64_t type, uint64_t protocol,
                            uint64_t sv, uint64_t e, uint64_t f) {
    (void)e; (void)f;
    int result = socketpair((int)domain, (int)type, (int)protocol, (int *)sv);
    return result < 0 ? -errno : (int64_t)result;
}

int64_t vsl_sys_connect(uint64_t sockfd, uint64_t addr, uint64_t addrlen,
                         uint64_t d, uint64_t e, uint64_t f) {
    (void)d; (void)e; (void)f;
    int result = connect((int)sockfd, (const struct sockaddr *)addr, (socklen_t)addrlen);
    return result < 0 ? -errno : (int64_t)result;
}

int64_t vsl_sys_bind(uint64_t sockfd, uint64_t addr, uint64_t addrlen,
                      uint64_t d, uint64_t e, uint64_t f) {
    (void)d; (void)e; (void)f;
    int result = bind((int)sockfd, (const struct sockaddr *)addr, (socklen_t)addrlen);
    return result < 0 ? -errno : (int64_t)result;
}

int64_t vsl_sys_listen(uint64_t sockfd, uint64_t backlog, uint64_t c,
                        uint64_t d, uint64_t e, uint64_t f) {
    (void)c; (void)d; (void)e; (void)f;
    int result = listen((int)sockfd, (int)backlog);
    return result < 0 ? -errno : (int64_t)result;
}

int64_t vsl_sys_accept(uint64_t sockfd, uint64_t addr, uint64_t addrlen,
                        uint64_t d, uint64_t e, uint64_t f) {
    (void)d; (void)e; (void)f;
    socklen_t *len = addrlen ? (socklen_t *)addrlen : NULL;
    int result = accept((int)sockfd, (struct sockaddr *)addr, len);
    return result < 0 ? -errno : (int64_t)result;
}

int64_t vsl_sys_sendto(uint64_t sockfd, uint64_t buf, uint64_t len,
                        uint64_t flags, uint64_t dest_addr, uint64_t addrlen) {
    ssize_t result = sendto((int)sockfd, (const void *)buf, (size_t)len, (int)flags,
                            (const struct sockaddr *)dest_addr, (socklen_t)addrlen);
    return result < 0 ? -errno : (int64_t)result;
}

int64_t vsl_sys_recvfrom(uint64_t sockfd, uint64_t buf, uint64_t len,
                          uint64_t flags, uint64_t src_addr, uint64_t addrlen) {
    ssize_t result = recvfrom((int)sockfd, (void *)buf, (size_t)len, (int)flags,
                              (struct sockaddr *)src_addr, (socklen_t *)addrlen);
    return result < 0 ? -errno : (int64_t)result;
}

int64_t vsl_sys_shutdown(uint64_t sockfd, uint64_t how,
                          uint64_t c, uint64_t d, uint64_t e, uint64_t f) {
    (void)c; (void)d; (void)e; (void)f;
    int result = shutdown((int)sockfd, (int)how);
    return result < 0 ? -errno : (int64_t)result;
}

int64_t vsl_sys_getsockname(uint64_t sockfd, uint64_t addr, uint64_t addrlen,
                             uint64_t d, uint64_t e, uint64_t f) {
    (void)d; (void)e; (void)f;
    socklen_t *len = addrlen ? (socklen_t *)addrlen : NULL;
    int result = getsockname((int)sockfd, (struct sockaddr *)addr, len);
    return result < 0 ? -errno : (int64_t)result;
}

int64_t vsl_sys_getpeername(uint64_t sockfd, uint64_t addr, uint64_t addrlen,
                             uint64_t d, uint64_t e, uint64_t f) {
    (void)d; (void)e; (void)f;
    socklen_t *len = addrlen ? (socklen_t *)addrlen : NULL;
    int result = getpeername((int)sockfd, (struct sockaddr *)addr, len);
    return result < 0 ? -errno : (int64_t)result;
}

int64_t vsl_sys_setsockopt(uint64_t sockfd, uint64_t level, uint64_t optname,
                            uint64_t optval, uint64_t optlen, uint64_t f) {
    (void)f;
    int result = setsockopt((int)sockfd, (int)level, (int)optname,
                            (const void *)optval, (socklen_t)optlen);
    return result < 0 ? -errno : (int64_t)result;
}

/* ====================================================================
 * TIMER SYSCALLS
 * ==================================================================== */

int64_t vsl_sys_timer_create(uint64_t clockid, uint64_t evp, uint64_t timerid,
                              uint64_t d, uint64_t e, uint64_t f) {
    (void)d; (void)e; (void)f;
    struct sigevent ev;
    if (evp) memcpy(&ev, (void *)evp, sizeof(struct sigevent));
    timer_t timer;
    int result = timer_create((clockid_t)clockid, evp ? &ev : NULL, &timer);
    if (result < 0) return -errno;
    if (timerid) memcpy((void *)timerid, &timer, sizeof(timer_t));
    return 0;
}

int64_t vsl_sys_timer_settime(uint64_t timerid, uint64_t flags, uint64_t newval,
                               uint64_t oldval, uint64_t e, uint64_t f) {
    (void)e; (void)f;
    struct itimerspec new_, old_;
    if (newval) memcpy(&new_, (void *)newval, sizeof(struct itimerspec));
    if (oldval) memset(&old_, 0, sizeof(struct itimerspec));
    int result = timer_settime((timer_t)timerid, (int)flags,
                               newval ? &new_ : NULL, oldval ? &old_ : NULL);
    if (result < 0) return -errno;
    if (oldval) memcpy((void *)oldval, &old_, sizeof(struct itimerspec));
    return 0;
}

int64_t vsl_sys_timer_gettime(uint64_t timerid, uint64_t value,
                               uint64_t d, uint64_t e, uint64_t f, uint64_t g) {
    (void)d; (void)e; (void)f; (void)g;
    struct itimerspec val;
    int result = timer_gettime((timer_t)timerid, &val);
    if (result < 0) return -errno;
    if (value) memcpy((void *)value, &val, sizeof(struct itimerspec));
    return 0;
}

int64_t vsl_sys_timer_delete(uint64_t timerid, uint64_t d, uint64_t e,
                              uint64_t f, uint64_t g, uint64_t h) {
    (void)d; (void)e; (void)f; (void)g; (void)h;
    int result = timer_delete((timer_t)timerid);
    return result < 0 ? -errno : (int64_t)result;
}

int64_t vsl_sys_timerfd_create(uint64_t clockid, uint64_t flags,
                                uint64_t d, uint64_t e, uint64_t f, uint64_t g) {
    (void)d; (void)e; (void)f; (void)g;
    int result = timerfd_create((int)clockid, (int)flags);
    return result < 0 ? -errno : (int64_t)result;
}

int64_t vsl_sys_timerfd_settime(uint64_t fd, uint64_t flags, uint64_t newval,
                                 uint64_t oldval, uint64_t e, uint64_t f) {
    (void)e; (void)f;
    struct itimerspec new_, old_;
    if (newval) memcpy(&new_, (void *)newval, sizeof(struct itimerspec));
    if (oldval) memset(&old_, 0, sizeof(struct itimerspec));
    int host_fd = vsl_get_host_fd((int)fd);
    if (host_fd < 0) return -9;
    int result = timerfd_settime(host_fd, (int)flags,
                                 newval ? &new_ : NULL, oldval ? &old_ : NULL);
    if (result < 0) return -errno;
    if (oldval) memcpy((void *)oldval, &old_, sizeof(struct itimerspec));
    return 0;
}

int64_t vsl_sys_timerfd_gettime(uint64_t fd, uint64_t value,
                                 uint64_t d, uint64_t e, uint64_t f, uint64_t g) {
    (void)d; (void)e; (void)f; (void)g;
    struct itimerspec val;
    int host_fd = vsl_get_host_fd((int)fd);
    if (host_fd < 0) return -9;
    int result = timerfd_gettime(host_fd, &val);
    if (result < 0) return -errno;
    if (value) memcpy((void *)value, &val, sizeof(struct itimerspec));
    return 0;
}

/* ====================================================================
 * EVENTFD / INOTIFY / SIGNALFD SYSCALLS
 * ==================================================================== */

int64_t vsl_sys_eventfd(uint64_t count, uint64_t d, uint64_t e,
                         uint64_t f, uint64_t g, uint64_t h) {
    (void)d; (void)e; (void)f; (void)g; (void)h;
    int result = eventfd((unsigned int)count, 0);
    return result < 0 ? -errno : (int64_t)result;
}

int64_t vsl_sys_eventfd2(uint64_t count, uint64_t flags,
                          uint64_t d, uint64_t e, uint64_t f, uint64_t g) {
    (void)d; (void)e; (void)f; (void)g;
    long result = syscall(SYS_eventfd2, (unsigned int)count, (int)flags);
    return result < 0 ? -errno : (int64_t)result;
}

int64_t vsl_sys_inotify_init(uint64_t a, uint64_t b, uint64_t c,
                              uint64_t d, uint64_t e, uint64_t f) {
    (void)a; (void)b; (void)c; (void)d; (void)e; (void)f;
    int result = inotify_init();
    return result < 0 ? -errno : (int64_t)result;
}

int64_t vsl_sys_inotify_add_watch(uint64_t fd, uint64_t pathname, uint64_t mask,
                                   uint64_t d, uint64_t e, uint64_t f) {
    (void)d; (void)e; (void)f;
    int host_fd = vsl_get_host_fd((int)fd);
    if (host_fd < 0) return -9;
    int result = inotify_add_watch(host_fd, (const char *)pathname, (uint32_t)mask);
    return result < 0 ? -errno : (int64_t)result;
}

int64_t vsl_sys_inotify_rm_watch(uint64_t fd, uint64_t wd,
                                  uint64_t d, uint64_t e, uint64_t f, uint64_t g) {
    (void)d; (void)e; (void)f; (void)g;
    int host_fd = vsl_get_host_fd((int)fd);
    if (host_fd < 0) return -9;
    int result = inotify_rm_watch(host_fd, (int)wd);
    return result < 0 ? -errno : (int64_t)result;
}

int64_t vsl_sys_signalfd(uint64_t fd, uint64_t mask, uint64_t sizemask,
                          uint64_t d, uint64_t e, uint64_t f) {
    (void)d; (void)e; (void)f;
    sigset_t ss;
    memcpy(&ss, (void *)mask, sizeof(sigset_t));
    int result = syscall(SYS_signalfd, (int)fd, &ss, (size_t)sizemask);
    return result < 0 ? -errno : (int64_t)result;
}

int64_t vsl_sys_signalfd4(uint64_t fd, uint64_t mask, uint64_t sizemask,
                           uint64_t flags, uint64_t e, uint64_t f) {
    (void)e; (void)f;
    sigset_t ss;
    memcpy(&ss, (void *)mask, sizeof(sigset_t));
    long result = syscall(SYS_signalfd4, (int)fd, &ss, (size_t)sizemask, (int)flags);
    return result < 0 ? -errno : (int64_t)result;
}

/* ====================================================================
 * SELECT / PPOLL SYSCALLS
 * ==================================================================== */

int64_t vsl_sys_pselect6(uint64_t nfds, uint64_t readfds, uint64_t writefds,
                          uint64_t exceptfds, uint64_t timeout, uint64_t sigmask) {
    (void)sigmask;
    fd_set *rfds = readfds ? (fd_set *)readfds : NULL;
    fd_set *wfds = writefds ? (fd_set *)writefds : NULL;
    fd_set *efds = exceptfds ? (fd_set *)exceptfds : NULL;
    struct timespec *ts = timeout ? (struct timespec *)timeout : NULL;
    const sigset_t *ss = NULL;
    int result = pselect((int)nfds, rfds, wfds, efds, ts, ss);
    return result < 0 ? -errno : (int64_t)result;
}

int64_t vsl_sys_ppoll(uint64_t fds, uint64_t nfds, uint64_t timeout,
                       uint64_t sigmask, uint64_t sigsetsize, uint64_t f) {
    (void)sigmask; (void)sigsetsize; (void)f;
    struct pollfd *pfds = (struct pollfd *)fds;
    struct timespec *ts = timeout ? (struct timespec *)timeout : NULL;
    const sigset_t *ss = NULL;
    long result = syscall(SYS_ppoll, pfds, (nfds_t)nfds, ts, ss);
    return result < 0 ? -errno : (int64_t)result;
}

int64_t vsl_sys_select(uint64_t nfds, uint64_t readfds, uint64_t writefds,
                        uint64_t exceptfds, uint64_t timeout, uint64_t f) {
    (void)f;
    fd_set *rfds = readfds ? (fd_set *)readfds : NULL;
    fd_set *wfds = writefds ? (fd_set *)writefds : NULL;
    fd_set *efds = exceptfds ? (fd_set *)exceptfds : NULL;
    struct timeval *tv = timeout ? (struct timeval *)timeout : NULL;
    int result = select((int)nfds, rfds, wfds, efds, tv);
    return result < 0 ? -errno : (int64_t)result;
}

/* ====================================================================
 * PIPE2 / CLONE3 / IO_URING SYSCALLS
 * ==================================================================== */

int64_t vsl_sys_pipe2(uint64_t pipefd, uint64_t flags, uint64_t c,
                       uint64_t d, uint64_t e, uint64_t f) {
    (void)c; (void)d; (void)e; (void)f;
    if (pipefd) {
        int *fds = (int *)pipefd;
        int host_fds[2];
        int rc = pipe2(host_fds, (int)flags);
        if (rc < 0) return -errno;
        fds[0] = host_fds[0];
        fds[1] = host_fds[1];
    }
    return 0;
}

int64_t vsl_sys_clone3(uint64_t cl_args, uint64_t size, uint64_t c,
                        uint64_t d, uint64_t e, uint64_t f) {
    (void)c; (void)d; (void)e; (void)f;
    int result = (int)syscall(SYS_clone3, (void *)cl_args, (size_t)size);
    return result < 0 ? -errno : (int64_t)result;
}

int64_t vsl_sys_io_uring_setup(uint64_t entries, uint64_t params,
                                uint64_t c, uint64_t d, uint64_t e, uint64_t f) {
    (void)c; (void)d; (void)e; (void)f;
    long result = syscall(SYS_io_uring_setup, (unsigned int)entries, (void *)params);
    return result < 0 ? -errno : (int64_t)result;
}

int64_t vsl_sys_io_uring_enter(uint64_t fd, uint64_t to_submit,
                                uint64_t min_complete, uint64_t flags,
                                uint64_t arg, uint64_t sz) {
    (void)arg; (void)sz;
    long result = syscall(SYS_io_uring_enter, (int)fd, (unsigned int)to_submit,
                         (unsigned int)min_complete, (unsigned int)flags,
                         (void *)arg, (size_t)sz);
    return result < 0 ? -errno : (int64_t)result;
}

int64_t vsl_sys_io_uring_register(uint64_t fd, uint64_t opcode,
                                   uint64_t arg, uint64_t nr_args,
                                   uint64_t e, uint64_t f) {
    (void)e; (void)f;
    long result = syscall(SYS_io_uring_register, (int)fd, (unsigned int)opcode,
                         (void *)arg, (unsigned int)nr_args);
    return result < 0 ? -errno : (int64_t)result;
}

/* ====================================================================
 * *AT FAMILY SYSCALLS
 * ==================================================================== */

int64_t vsl_sys_readlinkat(uint64_t dirfd, uint64_t pathname, uint64_t buf,
                            uint64_t bufsiz, uint64_t e, uint64_t f) {
    (void)e; (void)f;
    ssize_t result = readlinkat((int)dirfd, (const char *)pathname,
                                (char *)buf, (size_t)bufsiz);
    return result < 0 ? -errno : (int64_t)result;
}

int64_t vsl_sys_fchmodat(uint64_t dirfd, uint64_t pathname, uint64_t mode,
                          uint64_t flags, uint64_t e, uint64_t f) {
    (void)e; (void)f;
    int result = fchmodat((int)dirfd, (const char *)pathname,
                          (mode_t)mode, (int)flags);
    return result < 0 ? -errno : (int64_t)result;
}

int64_t vsl_sys_fchownat(uint64_t dirfd, uint64_t pathname, uint64_t owner,
                          uint64_t group, uint64_t flags, uint64_t f) {
    (void)f;
    int result = fchownat((int)dirfd, (const char *)pathname,
                          (uid_t)owner, (gid_t)group, (int)flags);
    return result < 0 ? -errno : (int64_t)result;
}

int64_t vsl_sys_utimensat(uint64_t dirfd, uint64_t pathname, uint64_t times,
                           uint64_t flags, uint64_t e, uint64_t f) {
    (void)e; (void)f;
    const struct timespec *ts = times ? (const struct timespec *)times : NULL;
    int result = utimensat((int)dirfd, (const char *)pathname, ts, (int)flags);
    return result < 0 ? -errno : (int64_t)result;
}

int64_t vsl_sys_futimesat(uint64_t dirfd, uint64_t pathname, uint64_t times,
                           uint64_t e, uint64_t f, uint64_t g) {
    (void)e; (void)f; (void)g;
    const struct timeval *tv = times ? (const struct timeval *)times : NULL;
    struct timespec ts[2];
    if (tv) {
        ts[0].tv_sec = tv[0].tv_sec;
        ts[0].tv_nsec = tv[0].tv_usec * 1000;
        ts[1].tv_sec = tv[1].tv_sec;
        ts[1].tv_nsec = tv[1].tv_usec * 1000;
    }
    int result = utimensat((int)dirfd, (const char *)pathname, tv ? ts : NULL, 0);
    return result < 0 ? -errno : (int64_t)result;
}

int64_t vsl_sys_renameat(uint64_t olddirfd, uint64_t oldpath, uint64_t newdirfd,
                          uint64_t newpath, uint64_t e, uint64_t f) {
    (void)e; (void)f;
    int result = renameat((int)olddirfd, (const char *)oldpath,
                          (int)newdirfd, (const char *)newpath);
    return result < 0 ? -errno : (int64_t)result;
}

int64_t vsl_sys_mkdirat(uint64_t dirfd, uint64_t pathname, uint64_t mode,
                         uint64_t d, uint64_t e, uint64_t f) {
    (void)d; (void)e; (void)f;
    int result = mkdirat((int)dirfd, (const char *)pathname, (mode_t)mode);
    return result < 0 ? -errno : (int64_t)result;
}

int64_t vsl_sys_symlinkat(uint64_t target, uint64_t newdirfd,
                           uint64_t linkpath, uint64_t d, uint64_t e, uint64_t f) {
    (void)d; (void)e; (void)f;
    int result = symlinkat((const char *)target, (int)newdirfd,
                           (const char *)linkpath);
    return result < 0 ? -errno : (int64_t)result;
}

int64_t vsl_sys_linkat(uint64_t olddirfd, uint64_t oldpath, uint64_t newdirfd,
                        uint64_t newpath, uint64_t flags, uint64_t f) {
    (void)f;
    int result = linkat((int)olddirfd, (const char *)oldpath,
                        (int)newdirfd, (const char *)newpath, (int)flags);
    return result < 0 ? -errno : (int64_t)result;
}

int64_t vsl_sys_mknodat(uint64_t dirfd, uint64_t pathname, uint64_t mode,
                        uint64_t dev, uint64_t e, uint64_t f) {
    (void)e; (void)f;
    int result = mknodat((int)dirfd, (const char *)pathname, (mode_t)mode, (dev_t)dev);
    return result < 0 ? -errno : (int64_t)result;
}

int64_t vsl_sys_getwd(uint64_t buf, uint64_t size, uint64_t c,
                       uint64_t d, uint64_t e, uint64_t f) {
    (void)c; (void)d; (void)e; (void)f;
    char *result = getcwd((char *)buf, (size_t)size);
    return result ? (int64_t)buf : -errno;
}

int64_t vsl_sys_fchdir(uint64_t fd, uint64_t b, uint64_t c,
                        uint64_t d, uint64_t e, uint64_t f) {
    (void)b; (void)c; (void)d; (void)e; (void)f;
    int result = fchdir((int)fd);
    return result < 0 ? -errno : (int64_t)result;
}

/* ====================================================================
 * NAMESPACE & SECURITY SYSCALLS
 * ==================================================================== */

int64_t vsl_sys_unshare(uint64_t flags, uint64_t b, uint64_t c,
                         uint64_t d, uint64_t e, uint64_t f) {
    (void)b; (void)c; (void)d; (void)e; (void)f;
    int result = unshare((int)flags);
    return result < 0 ? -errno : (int64_t)result;
}

int64_t vsl_sys_setns(uint64_t fd, uint64_t nstype, uint64_t c,
                       uint64_t d, uint64_t e, uint64_t f) {
    (void)c; (void)d; (void)e; (void)f;
    int result = setns((int)fd, (int)nstype);
    return result < 0 ? -errno : (int64_t)result;
}

int64_t vsl_sys_fanotify_init(uint64_t flags, uint64_t event_f_flags,
                               uint64_t c, uint64_t d, uint64_t e, uint64_t f) {
    (void)c; (void)d; (void)e; (void)f;
    int result = syscall(SYS_fanotify_init, (unsigned int)flags, (unsigned int)event_f_flags);
    return result < 0 ? -errno : (int64_t)result;
}

int64_t vsl_sys_fanotify_mark(uint64_t fanotify_fd, uint64_t flags, uint64_t mask,
                               uint64_t dirfd, uint64_t pathname, uint64_t f) {
    (void)f;
    int result = syscall(SYS_fanotify_mark, (int)fanotify_fd, (unsigned int)flags,
                         (uint64_t)mask, (int)dirfd, (const char *)pathname);
    return result < 0 ? -errno : (int64_t)result;
}

int64_t vsl_sys_landlock(uint64_t cmd, uint64_t attr, uint64_t flags,
                          uint64_t c, uint64_t d, uint64_t e) {
    (void)c; (void)d; (void)e;
    int result = syscall(444, (int)cmd, (void *)attr, (size_t)flags);
    return result < 0 ? -errno : (int64_t)result;
}

int64_t vsl_sys_bpf(uint64_t cmd, uint64_t attr, uint64_t size,
                    uint64_t c, uint64_t d, uint64_t e) {
    (void)c; (void)d; (void)e;
    int result = syscall(321, (int)cmd, (void *)attr, (size_t)size);
    return result < 0 ? -errno : (int64_t)result;
}

int64_t vsl_sys_perf_event_open(uint64_t attr, uint64_t pid, uint64_t cpu,
                                 uint64_t group_fd, uint64_t flags, uint64_t f) {
    (void)f;
    struct perf_event_attr *pe = (struct perf_event_attr *)attr;
    int result = syscall(SYS_perf_event_open, pe, (pid_t)pid, (int)cpu,
                         (int)group_fd, (unsigned long)flags);
    return result < 0 ? -errno : (int64_t)result;
}
