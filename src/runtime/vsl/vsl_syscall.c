/*
 * vsl_syscall.c  --  VSL Syscall Bridge Implementation
 * All syscall handlers in one self-contained module (C11, opaque-friendly)
 */

#define _GNU_SOURCE
#include "wubu_vsl.h"
#include "wubu_container.h"
#include "vsl/vsl_syscall.h"
#include "vsl/vsl_internal.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <signal.h>
#include <time.h>
#include <sys/ioctl.h>
#include <sys/resource.h>
#include <sys/syscall.h>
#include <sys/uio.h>
#include <sys/epoll.h>
#include <poll.h>
#include <sys/select.h>
#include <stddef.h>
#include <sys/inotify.h>
#include <sys/mman.h>
#include <sys/timerfd.h>
#include <sys/eventfd.h>
#include <sys/signalfd.h>
#include <dlfcn.h>
#include <linux/if.h>
#include <linux/if_tun.h>

/* For new namespace/security syscalls */
#include <linux/landlock.h>
#include <linux/bpf.h>
#include <linux/perf_event.h>
#include <sys/fanotify.h>
#include <sched.h>

/* Forward declarations */
static int vsl_get_host_fd(int vsl_fd);

/* -- Syscall Handlers ---------------------------------------------- */

static int64_t vsl_sys_nosys(uint64_t a, uint64_t b, uint64_t c,
                              uint64_t d, uint64_t e, uint64_t f) {
    (void)a; (void)b; (void)c; (void)d; (void)e; (void)f;
    return -38; /* ENOSYS */
}

static int64_t vsl_sys_exit(uint64_t code, uint64_t b, uint64_t c,
                             uint64_t d, uint64_t e, uint64_t f) {
    (void)b; (void)c; (void)d; (void)e; (void)f;
    VSL_PROC *p = vsl_get_process(g_vsl.current_pid);
    if (p) {
        p->state = VSL_PROC_ZOMBIE;
        p->exit_code = (int)(code & 0xFF);
    }
    return 0;
}

static int64_t vsl_sys_getpid(uint64_t a, uint64_t b, uint64_t c,
                               uint64_t d, uint64_t e, uint64_t f) {
    (void)b; (void)c; (void)d; (void)e; (void)f;
    return (int64_t)g_vsl.current_pid;
}

static int64_t vsl_sys_getppid(uint64_t a, uint64_t b, uint64_t c,
                                uint64_t d, uint64_t e, uint64_t f) {
    (void)a; (void)b; (void)c; (void)d; (void)e; (void)f;
    VSL_PROC *p = vsl_get_process(g_vsl.current_pid);
    return p ? (int64_t)p->ppid : 0;
}

static int64_t vsl_sys_getuid(uint64_t a, uint64_t b, uint64_t c,
                               uint64_t d, uint64_t e, uint64_t f) {
    (void)a; (void)b; (void)c; (void)d; (void)e; (void)f;
    return 0; /* root */
}

static int64_t vsl_sys_getgid(uint64_t a, uint64_t b, uint64_t c,
                               uint64_t d, uint64_t e, uint64_t f) {
    (void)a; (void)b; (void)c; (void)d; (void)e; (void)f;
    return 0; /* root */
}

static int64_t vsl_sys_sched_yield(uint64_t a, uint64_t b, uint64_t c,
                                    uint64_t d, uint64_t e, uint64_t f) {
    (void)a; (void)b; (void)c; (void)d; (void)e; (void)f;
    return 0;
}

static int64_t vsl_sys_fork(uint64_t a, uint64_t b, uint64_t c,
                             uint64_t d, uint64_t e, uint64_t f) {
    (void)b; (void)c; (void)d; (void)e; (void)f;
    pid_t host_pid = fork();
    if (host_pid < 0) return -errno;
    if (host_pid == 0) {
        g_vsl.current_pid = (uint32_t)host_pid;
        return 0;
    }
    int vsl_child = register_child_pid(host_pid, g_vsl.current_pid);
    if (vsl_child < 0) {
        kill(host_pid, SIGKILL);
        waitpid(host_pid, NULL, 0);
        return -1;
    }
    return (int64_t)vsl_child;
}

static int64_t vsl_sys_clone(uint64_t flags, uint64_t stack, uint64_t ptid,
                              uint64_t ctid, uint64_t tls, uint64_t f) {
    (void)stack; (void)ptid; (void)ctid; (void)tls; (void)f;
    return vsl_sys_fork(flags, 0, 0, 0, 0, 0);
}

static int64_t vsl_sys_vfork(uint64_t a, uint64_t b, uint64_t c,
                              uint64_t d, uint64_t e, uint64_t f) {
    return vsl_sys_fork(a, b, c, d, e, f);
}

static int64_t vsl_sys_execve(uint64_t path, uint64_t argv, uint64_t envp,
                               uint64_t d, uint64_t e, uint64_t f) {
    (void)d; (void)e; (void)f;
    const char *pathname = (const char *)path;
    if (!pathname) return -2;
    char **host_argv = NULL;
    int argc = 0;
    if (argv) {
        uint64_t *vsl_argv = (uint64_t *)argv;
        while (vsl_argv[argc]) argc++;
        host_argv = (char **)calloc((size_t)argc + 1, sizeof(char *));
        if (!host_argv) return -12;
        for (int i = 0; i < argc; i++)
            host_argv[i] = (char *)(uintptr_t)vsl_argv[i];
    }
    execve(pathname, host_argv, (char *const *)(uintptr_t)envp);
    free(host_argv);
    return -errno;
}

static int64_t vsl_sys_wait4(uint64_t pid, uint64_t status, uint64_t options,
                              uint64_t rusage, uint64_t e, uint64_t f) {
    (void)e; (void)f;
    int host_status = 0;
    pid_t host_pid = (pid == (uint64_t)(-1)) ? -1 : (pid_t)(int)pid;
    pid_t result = waitpid(host_pid, &host_status, (int)options);
    if (result < 0) return -errno;
    if (status && result > 0) {
        int *out = (int *)status;
        *out = host_status;
    }
    if (rusage) {
        memset((void *)rusage, 0, sizeof(struct rusage));
    }
    return (int64_t)result;
}

static int64_t vsl_sys_waitpid(uint64_t pid, uint64_t status, uint64_t options,
                                uint64_t d, uint64_t e, uint64_t f) {
    return vsl_sys_wait4(pid, status, options, 0, e, f);
}

static int64_t vsl_sys_kill(uint64_t pid, uint64_t sig, uint64_t c,
                             uint64_t d, uint64_t e, uint64_t f) {
    (void)c; (void)d; (void)e; (void)f;
    int rc = kill((pid_t)pid, (int)sig);
    return rc < 0 ? -errno : (int64_t)rc;
}

static int64_t vsl_sys_read(uint64_t fd, uint64_t buf, uint64_t count,
                             uint64_t d, uint64_t e, uint64_t f) {
    (void)d; (void)e; (void)f;
    ssize_t result = read((int)fd, (void *)buf, (size_t)count);
    return result < 0 ? -errno : (int64_t)result;
}

static int64_t vsl_sys_write(uint64_t fd, uint64_t buf, uint64_t count,
                              uint64_t d, uint64_t e, uint64_t f) {
    (void)d; (void)e; (void)f;
    ssize_t result = write((int)fd, (const void *)buf, (size_t)count);
    return result < 0 ? -errno : (int64_t)result;
}

static int64_t vsl_sys_open(uint64_t path, uint64_t flags, uint64_t mode,
                             uint64_t d, uint64_t e, uint64_t f) {
    (void)d; (void)e; (void)f;
    return vsl_open((const char *)path, (int)flags, (int)mode);
}

static int64_t vsl_sys_close(uint64_t fd, uint64_t b, uint64_t c,
                              uint64_t d, uint64_t e, uint64_t f) {
    (void)b; (void)c; (void)d; (void)e; (void)f;
    return vsl_close((int)fd);
}

static int64_t vsl_sys_lseek(uint64_t fd, uint64_t offset, uint64_t whence,
                              uint64_t d, uint64_t e, uint64_t f) {
    (void)d; (void)e; (void)f;
    return vsl_lseek((int)fd, (int64_t)offset, (int)whence);
}

static int64_t vsl_sys_fstat(uint64_t fd, uint64_t buf, uint64_t c,
                              uint64_t d, uint64_t e, uint64_t f) {
    (void)c; (void)d; (void)e; (void)f;
    struct stat st;
    int rc = fstat((int)fd, &st);
    if (rc < 0) return -errno;
    if (buf) {
        memset((void *)buf, 0, sizeof(struct stat));
        memcpy((void *)buf, &st, sizeof(struct stat));
    }
    return 0;
}

static int64_t vsl_sys_stat(uint64_t path, uint64_t buf, uint64_t c,
                             uint64_t d, uint64_t e, uint64_t f) {
    (void)c; (void)d; (void)e; (void)f;
    struct stat st;
    int rc = stat((const char *)path, &st);
    if (rc < 0) return -errno;
    if (buf) {
        memset((void *)buf, 0, sizeof(struct stat));
        memcpy((void *)buf, &st, sizeof(struct stat));
    }
    return 0;
}

static int64_t vsl_sys_ioctl(uint64_t fd, uint64_t req, uint64_t arg,
                              uint64_t d, uint64_t e, uint64_t f) {
    (void)d; (void)e; (void)f;
    int result = ioctl((int)fd, (unsigned long)req, (void *)arg);
    return result < 0 ? -errno : (int64_t)result;
}

static int64_t vsl_sys_access(uint64_t path, uint64_t mode, uint64_t c,
                               uint64_t d, uint64_t e, uint64_t f) {
    (void)c; (void)d; (void)e; (void)f;
    int rc = access((const char *)path, (int)mode);
    return rc < 0 ? -errno : (int64_t)rc;
}

static int64_t vsl_sys_pipe(uint64_t pipefd, uint64_t b, uint64_t c,
                             uint64_t d, uint64_t e, uint64_t f) {
    (void)b; (void)c; (void)d; (void)e; (void)f;
    if (pipefd) {
        int *fds = (int *)pipefd;
        int host_fds[2];
        int rc = pipe(host_fds);
        if (rc < 0) return -errno;
        fds[0] = host_fds[0];
        fds[1] = host_fds[1];
    }
    return 0;
}

static int64_t vsl_sys_dup(uint64_t fd, uint64_t b, uint64_t c,
                            uint64_t d, uint64_t e, uint64_t f) {
    (void)b; (void)c; (void)d; (void)e; (void)f;
    int result = dup((int)fd);
    return result < 0 ? -errno : (int64_t)result;
}

static int64_t vsl_sys_dup2(uint64_t oldfd, uint64_t newfd, uint64_t c,
                             uint64_t d, uint64_t e, uint64_t f) {
    (void)c; (void)d; (void)e; (void)f;
    int result = dup2((int)oldfd, (int)newfd);
    return result < 0 ? -errno : (int64_t)result;
}

static int64_t vsl_sys_fcntl(uint64_t fd, uint64_t cmd, uint64_t arg,
                              uint64_t d, uint64_t e, uint64_t f) {
    (void)d; (void)e; (void)f;
    int rc = fcntl((int)fd, (int)cmd, (int)arg);
    return rc < 0 ? -errno : (int64_t)rc;
}

static int64_t vsl_sys_fsync(uint64_t fd, uint64_t b, uint64_t c,
                              uint64_t d, uint64_t e, uint64_t f) {
    (void)b; (void)c; (void)d; (void)e; (void)f;
    int rc = fsync((int)fd);
    return rc < 0 ? -errno : (int64_t)rc;
}

static int64_t vsl_sys_unlink(uint64_t path, uint64_t b, uint64_t c,
                               uint64_t d, uint64_t e, uint64_t f) {
    (void)b; (void)c; (void)d; (void)e; (void)f;
    int rc = unlink((const char *)path);
    return rc < 0 ? -errno : (int64_t)rc;
}

static int64_t vsl_sys_mkdir(uint64_t path, uint64_t mode, uint64_t c,
                              uint64_t d, uint64_t e, uint64_t f) {
    (void)c; (void)d; (void)e; (void)f;
    int rc = mkdir((const char *)path, (mode_t)mode);
    return rc < 0 ? -errno : (int64_t)rc;
}

static int64_t vsl_sys_rmdir(uint64_t path, uint64_t b, uint64_t c,
                              uint64_t d, uint64_t e, uint64_t f) {
    (void)b; (void)c; (void)d; (void)e; (void)f;
    int rc = rmdir((const char *)path);
    return rc < 0 ? -errno : (int64_t)rc;
}

static int64_t vsl_sys_rename(uint64_t oldpath, uint64_t newpath, uint64_t c,
                               uint64_t d, uint64_t e, uint64_t f) {
    (void)c; (void)d; (void)e; (void)f;
    int rc = rename((const char *)oldpath, (const char *)newpath);
    return rc < 0 ? -errno : (int64_t)rc;
}

static int64_t vsl_sys_getcwd(uint64_t buf, uint64_t size, uint64_t c,
                               uint64_t d, uint64_t e, uint64_t f) {
    (void)c; (void)d; (void)e; (void)f;
    char *result = getcwd((char *)buf, (size_t)size);
    return result ? (int64_t)buf : -errno;
}

static int64_t vsl_sys_chdir(uint64_t path, uint64_t b, uint64_t c,
                              uint64_t d, uint64_t e, uint64_t f) {
    (void)b; (void)c; (void)d; (void)e; (void)f;
    int rc = chdir((const char *)path);
    return rc < 0 ? -errno : (int64_t)rc;
}

static int64_t vsl_sys_clock_gettime(uint64_t clk_id, uint64_t tp, uint64_t c,
                                      uint64_t d, uint64_t e, uint64_t f) {
    (void)c; (void)d; (void)e; (void)f;
    struct timespec ts;
    int rc = clock_gettime((clockid_t)clk_id, &ts);
    if (rc < 0) return -errno;
    if (tp) memcpy((void *)tp, &ts, sizeof(struct timespec));
    return 0;
}

static int64_t vsl_sys_sigaction(uint64_t signum, uint64_t act, uint64_t oldact,
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

static int64_t vsl_sys_sigprocmask(uint64_t how, uint64_t set, uint64_t oldset,
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

static int64_t vsl_sys_sigreturn(uint64_t a, uint64_t b, uint64_t c,
                                  uint64_t d, uint64_t e, uint64_t f) {
    (void)a; (void)b; (void)c; (void)d; (void)e; (void)f;
    return -ENOSYS;
}

static int64_t vsl_sys_rt_sigaction(uint64_t signum, uint64_t act, uint64_t oldact,
                                     uint64_t d, uint64_t e, uint64_t f) {
    return vsl_sys_sigaction(signum, act, oldact, d, e, f);
}

static int64_t vsl_sys_rt_sigprocmask(uint64_t how, uint64_t set, uint64_t oldset,
                                       uint64_t d, uint64_t e, uint64_t f) {
    return vsl_sys_sigprocmask(how, set, oldset, d, e, f);
}

static int64_t vsl_sys_futex(uint64_t uaddr, uint64_t op, uint64_t val,
                              uint64_t timeout, uint64_t uaddr2, uint64_t val3) {
    int *addr = (int *)uaddr;
    int rc = (int)syscall(SYS_futex, addr, (int)op, (int)val,
                          (struct timespec *)timeout, (int *)uaddr2, (int)val3);
    return rc < 0 ? -errno : (int64_t)rc;
}

static int64_t vsl_sys_exit_group(uint64_t code, uint64_t b, uint64_t c,
                                   uint64_t d, uint64_t e, uint64_t f) {
    return vsl_sys_exit(code, b, c, d, e, f);
}

static int64_t vsl_sys_socket(uint64_t domain, uint64_t type, uint64_t protocol,
                               uint64_t d, uint64_t e, uint64_t f) {
    (void)d; (void)e; (void)f;
    int result = (int)socket((int)domain, (int)type, (int)protocol);
    return result < 0 ? -errno : (int64_t)result;
}

static int64_t vsl_sys_socketpair(uint64_t domain, uint64_t type, uint64_t protocol,
                                   uint64_t sv, uint64_t e, uint64_t f) {
    (void)e; (void)f;
    int result = socketpair((int)domain, (int)type, (int)protocol, (int *)sv);
    return result < 0 ? -errno : (int64_t)result;
}

static int64_t vsl_sys_connect(uint64_t sockfd, uint64_t addr, uint64_t addrlen,
                                uint64_t d, uint64_t e, uint64_t f) {
    (void)d; (void)e; (void)f;
    int result = connect((int)sockfd, (const struct sockaddr *)addr, (socklen_t)addrlen);
    return result < 0 ? -errno : (int64_t)result;
}

static int64_t vsl_sys_bind(uint64_t sockfd, uint64_t addr, uint64_t addrlen,
                             uint64_t d, uint64_t e, uint64_t f) {
    (void)d; (void)e; (void)f;
    int result = bind((int)sockfd, (const struct sockaddr *)addr, (socklen_t)addrlen);
    return result < 0 ? -errno : (int64_t)result;
}

static int64_t vsl_sys_listen(uint64_t sockfd, uint64_t backlog, uint64_t c,
                               uint64_t d, uint64_t e, uint64_t f) {
    (void)c; (void)d; (void)e; (void)f;
    int result = listen((int)sockfd, (int)backlog);
    return result < 0 ? -errno : (int64_t)result;
}

static int64_t vsl_sys_accept(uint64_t sockfd, uint64_t addr, uint64_t addrlen,
                               uint64_t d, uint64_t e, uint64_t f) {
    (void)d; (void)e; (void)f;
    socklen_t *len = addrlen ? (socklen_t *)addrlen : NULL;
    int result = accept((int)sockfd, (struct sockaddr *)addr, len);
    return result < 0 ? -errno : (int64_t)result;
}

static int64_t vsl_sys_sendto(uint64_t sockfd, uint64_t buf, uint64_t len,
                               uint64_t flags, uint64_t dest_addr, uint64_t addrlen) {
    ssize_t result = sendto((int)sockfd, (const void *)buf, (size_t)len, (int)flags,
                            (const struct sockaddr *)dest_addr, (socklen_t)addrlen);
    return result < 0 ? -errno : (int64_t)result;
}

static int64_t vsl_sys_recvfrom(uint64_t sockfd, uint64_t buf, uint64_t len,
                                 uint64_t flags, uint64_t src_addr, uint64_t addrlen) {
    ssize_t result = recvfrom((int)sockfd, (void *)buf, (size_t)len, (int)flags,
                               (struct sockaddr *)src_addr, (socklen_t *)addrlen);
    return result < 0 ? -errno : (int64_t)result;
}

static int64_t vsl_sys_shutdown(uint64_t sockfd, uint64_t how,
                                 uint64_t c, uint64_t d, uint64_t e, uint64_t f) {
    (void)c; (void)d; (void)e; (void)f;
    int result = shutdown((int)sockfd, (int)how);
    return result < 0 ? -errno : (int64_t)result;
}

static int64_t vsl_sys_getsockname(uint64_t sockfd, uint64_t addr, uint64_t addrlen,
                                    uint64_t d, uint64_t e, uint64_t f) {
    (void)d; (void)e; (void)f;
    socklen_t *len = addrlen ? (socklen_t *)addrlen : NULL;
    int result = getsockname((int)sockfd, (struct sockaddr *)addr, len);
    return result < 0 ? -errno : (int64_t)result;
}

static int64_t vsl_sys_getpeername(uint64_t sockfd, uint64_t addr, uint64_t addrlen,
                                    uint64_t d, uint64_t e, uint64_t f) {
    (void)d; (void)e; (void)f;
    socklen_t *len = addrlen ? (socklen_t *)addrlen : NULL;
    int result = getpeername((int)sockfd, (struct sockaddr *)addr, len);
    return result < 0 ? -errno : (int64_t)result;
}

static int64_t vsl_sys_setsockopt(uint64_t sockfd, uint64_t level, uint64_t optname,
                                   uint64_t optval, uint64_t optlen, uint64_t f) {
    (void)f;
    int result = setsockopt((int)sockfd, (int)level, (int)optname,
                             (const void *)optval, (socklen_t)optlen);
    return result < 0 ? -errno : (int64_t)result;
}

static int64_t vsl_sys_mmap(uint64_t addr, uint64_t size, uint64_t prot,
                             uint64_t flags, uint64_t fd, uint64_t offset) {
    return (int64_t)vsl_mmap(addr, (size_t)size, (int)prot, (int)flags, (int)fd, offset);
}

static int64_t vsl_sys_munmap(uint64_t addr, uint64_t size, uint64_t c,
                               uint64_t d, uint64_t e, uint64_t f) {
    (void)c; (void)d; (void)e; (void)f;
    return vsl_munmap(addr, (size_t)size);
}

static int64_t vsl_sys_brk(uint64_t new_brk, uint64_t b, uint64_t c,
                            uint64_t d, uint64_t e, uint64_t f) {
    (void)b; (void)c; (void)d; (void)e; (void)f;
    return vsl_brk(new_brk);
}

/* Cell 360-370: Additional syscalls */

static int64_t vsl_sys_pread64(uint64_t fd, uint64_t buf, uint64_t count,
                                uint64_t pos_low, uint64_t pos_high, uint64_t f) {
    (void)f;
    int64_t offset = ((int64_t)pos_high << 32) | (int64_t)pos_low;
    ssize_t result = pread64((int)fd, (void *)buf, (size_t)count, offset);
    return result < 0 ? -errno : (int64_t)result;
}

static int64_t vsl_sys_pwrite64(uint64_t fd, uint64_t buf, uint64_t count,
                                 uint64_t pos_low, uint64_t pos_high, uint64_t f) {
    (void)f;
    int64_t offset = ((int64_t)pos_high << 32) | (int64_t)pos_low;
    ssize_t result = pwrite64((int)fd, (const void *)buf, (size_t)count, offset);
    return result < 0 ? -errno : (int64_t)result;
}

static int64_t vsl_sys_readv(uint64_t fd, uint64_t iov, uint64_t iovcnt,
                              uint64_t d, uint64_t e, uint64_t f) {
    (void)d; (void)e; (void)f;
    struct iovec *vec = (struct iovec *)iov;
    ssize_t result = readv((int)fd, vec, (int)iovcnt);
    return result < 0 ? -errno : (int64_t)result;
}

static int64_t vsl_sys_writev(uint64_t fd, uint64_t iov, uint64_t iovcnt,
                               uint64_t d, uint64_t e, uint64_t f) {
    (void)d; (void)e; (void)f;
    const struct iovec *vec = (const struct iovec *)iov;
    ssize_t result = writev((int)fd, vec, (int)iovcnt);
    return result < 0 ? -errno : (int64_t)result;
}

static int64_t vsl_sys_truncate(uint64_t path, uint64_t length,
                                 uint64_t c, uint64_t d, uint64_t e, uint64_t f) {
    (void)c; (void)d; (void)e; (void)f;
    int result = truncate((const char *)path, (off_t)length);
    return result < 0 ? -errno : (int64_t)result;
}

static int64_t vsl_sys_ftruncate(uint64_t fd, uint64_t length,
                                  uint64_t c, uint64_t d, uint64_t e, uint64_t f) {
    (void)c; (void)d; (void)e; (void)f;
    int result = ftruncate((int)fd, (off_t)length);
    return result < 0 ? -errno : (int64_t)result;
}

static int64_t vsl_sys_creat(uint64_t pathname, uint64_t mode,
                              uint64_t c, uint64_t d, uint64_t e, uint64_t f) {
    (void)c; (void)d; (void)e; (void)f;
    int fd = open((const char *)pathname, O_CREAT | O_WRONLY | O_TRUNC, (mode_t)mode);
    return fd < 0 ? -errno : (int64_t)fd;
}

static int64_t vsl_sys_symlink(uint64_t target, uint64_t linkpath,
                                uint64_t c, uint64_t d, uint64_t e, uint64_t f) {
    (void)c; (void)d; (void)e; (void)f;
    int result = symlink((const char *)target, (const char *)linkpath);
    return result < 0 ? -errno : (int64_t)result;
}

static int64_t vsl_sys_readlink(uint64_t path, uint64_t buf, uint64_t bufsiz,
                                 uint64_t d, uint64_t e, uint64_t f) {
    (void)d; (void)e; (void)f;
    ssize_t result = readlink((const char *)path, (char *)buf, (size_t)bufsiz);
    return result < 0 ? -errno : (int64_t)result;
}

static int64_t vsl_sys_chmod(uint64_t path, uint64_t mode,
                              uint64_t c, uint64_t d, uint64_t e, uint64_t f) {
    (void)c; (void)d; (void)e; (void)f;
    int result = chmod((const char *)path, (mode_t)mode);
    return result < 0 ? -errno : (int64_t)result;
}

static int64_t vsl_sys_fchmod(uint64_t fd, uint64_t mode,
                               uint64_t c, uint64_t d, uint64_t e, uint64_t f) {
    (void)c; (void)d; (void)e; (void)f;
    int result = fchmod((int)fd, (mode_t)mode);
    return result < 0 ? -errno : (int64_t)result;
}

static int64_t vsl_sys_chown(uint64_t path, uint64_t owner, uint64_t group,
                              uint64_t d, uint64_t e, uint64_t f) {
    (void)d; (void)e; (void)f;
    int result = chown((const char *)path, (uid_t)owner, (gid_t)group);
    return result < 0 ? -errno : (int64_t)result;
}

static int64_t vsl_sys_lchown(uint64_t path, uint64_t owner, uint64_t group,
                               uint64_t d, uint64_t e, uint64_t f) {
    (void)d; (void)e; (void)f;
    int result = lchown((const char *)path, (uid_t)owner, (gid_t)group);
    return result < 0 ? -errno : (int64_t)result;
}

static int64_t vsl_sys_fchown(uint64_t fd, uint64_t owner, uint64_t group,
                               uint64_t d, uint64_t e, uint64_t f) {
    (void)d; (void)e; (void)f;
    int result = fchown((int)fd, (uid_t)owner, (gid_t)group);
    return result < 0 ? -errno : (int64_t)result;
}

static int64_t vsl_sys_lstat(uint64_t path, uint64_t buf, uint64_t c,
                              uint64_t d, uint64_t e, uint64_t f) {
    (void)c; (void)d; (void)e; (void)f;
    struct stat st;
    int rc = lstat((const char *)path, &st);
    if (rc < 0) return -errno;
    if (buf) {
        memset((void *)buf, 0, sizeof(struct stat));
        memcpy((void *)buf, &st, sizeof(struct stat));
    }
    return 0;
}

static int64_t vsl_sys_openat(uint64_t dirfd, uint64_t pathname, uint64_t flags,
                               uint64_t mode, uint64_t e, uint64_t f) {
    (void)e; (void)f;
    return vsl_openat((int)dirfd, (const char *)pathname, (int)flags, (mode_t)mode);
}

static int64_t vsl_sys_newfstatat(uint64_t dirfd, uint64_t pathname, uint64_t buf,
                                   uint64_t flags, uint64_t e, uint64_t f) {
    (void)e; (void)f;
    struct stat st;
    int rc = fstatat((int)dirfd, (const char *)pathname, &st, (int)flags);
    if (rc < 0) return -errno;
    if (buf) {
        memset((void *)buf, 0, sizeof(struct stat));
        memcpy((void *)buf, &st, sizeof(struct stat));
    }
    return 0;
}

static int64_t vsl_sys_unlinkat(uint64_t dirfd, uint64_t pathname, uint64_t flags,
                                 uint64_t d, uint64_t e, uint64_t f) {
    (void)d; (void)e; (void)f;
    int result = unlinkat((int)dirfd, (const char *)pathname, (int)flags);
    return result < 0 ? -errno : (int64_t)result;
}

static int64_t vsl_sys_faccessat(uint64_t dirfd, uint64_t pathname, uint64_t mode,
                                  uint64_t flags, uint64_t e, uint64_t f) {
    (void)e; (void)f;
    int result = faccessat((int)dirfd, (const char *)pathname, (int)mode, (int)flags);
    return result < 0 ? -errno : (int64_t)result;
}

static int64_t vsl_sys_poll(uint64_t ufds, uint64_t nfds, uint64_t timeout,
                             uint64_t d, uint64_t e, uint64_t f) {
    (void)d; (void)e; (void)f;
    struct pollfd *fds = (struct pollfd *)ufds;
    int result = poll(fds, (nfds_t)nfds, (int)timeout);
    return result < 0 ? -errno : (int64_t)result;
}

static int64_t vsl_sys_epoll_create(uint64_t size, uint64_t b, uint64_t c,
                                     uint64_t d, uint64_t e, uint64_t f) {
    (void)b; (void)c; (void)d; (void)e; (void)f;
    int result = epoll_create1(0);
    return result < 0 ? -errno : (int64_t)result;
}

static int64_t vsl_sys_epoll_ctl(uint64_t epfd, uint64_t op, uint64_t fd,
                                  uint64_t event, uint64_t e, uint64_t f) {
    (void)e; (void)f;
    struct epoll_event ev;
    memcpy(&ev, (void *)event, sizeof(struct epoll_event));
    int result = epoll_ctl((int)epfd, (int)op, (int)fd, &ev);
    return result < 0 ? -errno : (int64_t)result;
}

static int64_t vsl_sys_epoll_wait(uint64_t epfd, uint64_t events, uint64_t maxevents,
                                   uint64_t timeout, uint64_t e, uint64_t f) {
    (void)e; (void)f;
    struct epoll_event *ev = (struct epoll_event *)events;
    int result = epoll_wait((int)epfd, ev, (int)maxevents, (int)timeout);
    return result < 0 ? -errno : (int64_t)result;
}

static int64_t vsl_sys_getdents64(uint64_t fd, uint64_t dirp, uint64_t count,
                                   uint64_t d, uint64_t e, uint64_t f) {
    (void)d; (void)e; (void)f;
    int host_fd = vsl_get_host_fd((int)fd);
    if (host_fd < 0) return -9;
    ssize_t result = syscall(SYS_getdents64, host_fd, (void *)dirp, (size_t)count);
    return result < 0 ? -errno : (int64_t)result;
}

static int64_t vsl_sys_statx(uint64_t dirfd, uint64_t pathname, uint64_t flags,
                              uint64_t mask, uint64_t statxbuf, uint64_t f) {
    (void)f; (void)mask;
    if (statxbuf) {
        struct stat st;
        int rc = fstatat((int)dirfd, (const char *)pathname, &st, (int)flags);
        if (rc < 0) return -errno;
        memset((void *)statxbuf, 0, sizeof(struct stat));
        memcpy((void *)statxbuf, &st, sizeof(struct stat));
    }
    return 0;
}

static int64_t vsl_sys_mremap(uint64_t old_addr, uint64_t old_size, uint64_t new_size,
                               uint64_t flags, uint64_t new_addr, uint64_t f) {
    (void)f;
    long result = syscall(SYS_mremap, (void *)old_addr, (size_t)old_size, (size_t)new_size,
                           (unsigned long)flags, (void *)new_addr);
    return result < 0 ? -errno : (int64_t)result;
}

static int64_t vsl_sys_mprotect(uint64_t addr, uint64_t len, uint64_t prot,
                                 uint64_t d, uint64_t e, uint64_t f) {
    (void)d; (void)e; (void)f;
    int result = mprotect((void *)addr, (size_t)len, (int)prot);
    return result < 0 ? -errno : (int64_t)result;
}

static int64_t vsl_sys_msync(uint64_t addr, uint64_t len, uint64_t flags,
                              uint64_t d, uint64_t e, uint64_t f) {
    (void)d; (void)e; (void)f;
    int result = msync((void *)addr, (size_t)len, (int)flags);
    return result < 0 ? -errno : (int64_t)result;
}

static int64_t vsl_sys_rt_sigsuspend(uint64_t mask, uint64_t d, uint64_t e,
                                      uint64_t f, uint64_t g, uint64_t h) {
    (void)d; (void)e; (void)f; (void)g; (void)h;
    sigset_t ss;
    memcpy(&ss, (void *)mask, sizeof(sigset_t));
    int result = syscall(SYS_rt_sigsuspend, &ss, sizeof(sigset_t));
    return result < 0 ? -errno : (int64_t)result;
}

static int64_t vsl_sys_rt_sigpending(uint64_t set, uint64_t sigsetsize,
                                      uint64_t d, uint64_t e, uint64_t f, uint64_t g) {
    (void)d; (void)e; (void)f; (void)g;
    sigset_t ss;
    int result = syscall(SYS_rt_sigpending, &ss, (size_t)sigsetsize);
    if (result < 0) return -errno;
    if (set) memcpy((void *)set, &ss, sizeof(sigset_t));
    return 0;
}

static int64_t vsl_sys_rt_sigtimedwait(uint64_t uthese, uint64_t uinfo, uint64_t uts,
                                        uint64_t usize, uint64_t e, uint64_t f) {
    (void)e; (void)f;
    const sigset_t *uthese_ptr = uthese ? (const sigset_t *)uthese : NULL;
    siginfo_t *uinfo_ptr = uinfo ? (siginfo_t *)uinfo : NULL;
    const struct timespec *uts_ptr = uts ? (const struct timespec *)uts : NULL;
    int result = syscall(SYS_rt_sigtimedwait, uthese_ptr, uinfo_ptr, uts_ptr, (size_t)usize);
    return result < 0 ? -errno : (int64_t)result;
}

static int64_t vsl_sys_rt_sigqueueinfo(uint64_t pid, uint64_t sig, uint64_t uinfo,
                                        uint64_t d, uint64_t e, uint64_t f) {
    (void)d; (void)e; (void)f;
    siginfo_t info;
    memcpy(&info, (void *)uinfo, sizeof(siginfo_t));
    int result = syscall(SYS_rt_sigqueueinfo, (int)pid, (int)sig, &info);
    return result < 0 ? -errno : (int64_t)result;
}

static int64_t vsl_sys_timer_create(uint64_t clockid, uint64_t evp, uint64_t timerid,
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

static int64_t vsl_sys_timer_settime(uint64_t timerid, uint64_t flags, uint64_t newval,
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

static int64_t vsl_sys_timer_gettime(uint64_t timerid, uint64_t value,
                                      uint64_t d, uint64_t e, uint64_t f, uint64_t g) {
    (void)d; (void)e; (void)f; (void)g;
    struct itimerspec val;
    int result = timer_gettime((timer_t)timerid, &val);
    if (result < 0) return -errno;
    if (value) memcpy((void *)value, &val, sizeof(struct itimerspec));
    return 0;
}

static int64_t vsl_sys_timer_delete(uint64_t timerid, uint64_t d, uint64_t e,
                                     uint64_t f, uint64_t g, uint64_t h) {
    (void)d; (void)e; (void)f; (void)g; (void)h;
    int result = timer_delete((timer_t)timerid);
    return result < 0 ? -errno : (int64_t)result;
}

static int64_t vsl_sys_timerfd_create(uint64_t clockid, uint64_t flags,
                                       uint64_t d, uint64_t e, uint64_t f, uint64_t g) {
    (void)d; (void)e; (void)f; (void)g;
    int result = timerfd_create((int)clockid, (int)flags);
    return result < 0 ? -errno : (int64_t)result;
}

static int64_t vsl_sys_timerfd_settime(uint64_t fd, uint64_t flags, uint64_t newval,
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

static int64_t vsl_sys_timerfd_gettime(uint64_t fd, uint64_t value,
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

static int64_t vsl_sys_eventfd(uint64_t count, uint64_t d, uint64_t e,
                                uint64_t f, uint64_t g, uint64_t h) {
    (void)d; (void)e; (void)f; (void)g; (void)h;
    int result = eventfd((unsigned int)count, 0);
    return result < 0 ? -errno : (int64_t)result;
}

static int64_t vsl_sys_eventfd2(uint64_t count, uint64_t flags,
                                 uint64_t d, uint64_t e, uint64_t f, uint64_t g) {
    (void)d; (void)e; (void)f; (void)g;
    long result = syscall(SYS_eventfd2, (unsigned int)count, (int)flags);
    return result < 0 ? -errno : (int64_t)result;
}

static int64_t vsl_sys_inotify_init(uint64_t a, uint64_t b, uint64_t c,
                                     uint64_t d, uint64_t e, uint64_t f) {
    (void)a; (void)b; (void)c; (void)d; (void)e; (void)f;
    int result = inotify_init();
    return result < 0 ? -errno : (int64_t)result;
}

static int64_t vsl_sys_inotify_add_watch(uint64_t fd, uint64_t pathname, uint64_t mask,
                                          uint64_t d, uint64_t e, uint64_t f) {
    (void)d; (void)e; (void)f;
    int host_fd = vsl_get_host_fd((int)fd);
    if (host_fd < 0) return -9;
    int result = inotify_add_watch(host_fd, (const char *)pathname, (uint32_t)mask);
    return result < 0 ? -errno : (int64_t)result;
}

static int64_t vsl_sys_inotify_rm_watch(uint64_t fd, uint64_t wd,
                                         uint64_t d, uint64_t e, uint64_t f, uint64_t g) {
    (void)d; (void)e; (void)f; (void)g;
    int host_fd = vsl_get_host_fd((int)fd);
    if (host_fd < 0) return -9;
    int result = inotify_rm_watch(host_fd, (int)wd);
    return result < 0 ? -errno : (int64_t)result;
}

static int64_t vsl_sys_signalfd(uint64_t fd, uint64_t mask, uint64_t sizemask,
                                 uint64_t d, uint64_t e, uint64_t f) {
    (void)d; (void)e; (void)f;
    sigset_t ss;
    memcpy(&ss, (void *)mask, sizeof(sigset_t));
    int result = syscall(SYS_signalfd, (int)fd, &ss, (size_t)sizemask);
    return result < 0 ? -errno : (int64_t)result;
}

static int64_t vsl_sys_signalfd4(uint64_t fd, uint64_t mask, uint64_t sizemask,
                                  uint64_t flags, uint64_t e, uint64_t f) {
    (void)e; (void)f;
    sigset_t ss;
    memcpy(&ss, (void *)mask, sizeof(sigset_t));
    long result = syscall(SYS_signalfd4, (int)fd, &ss, (size_t)sizemask, (int)flags);
    return result < 0 ? -errno : (int64_t)result;
}

static int64_t vsl_sys_pselect6(uint64_t nfds, uint64_t readfds, uint64_t writefds,
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

static int64_t vsl_sys_ppoll(uint64_t fds, uint64_t nfds, uint64_t timeout,
                              uint64_t sigmask, uint64_t sigsetsize, uint64_t f) {
    (void)sigmask; (void)sigsetsize; (void)f;
    struct pollfd *pfds = (struct pollfd *)fds;
    struct timespec *ts = timeout ? (struct timespec *)timeout : NULL;
    const sigset_t *ss = NULL;
    long result = syscall(SYS_ppoll, pfds, (nfds_t)nfds, ts, ss);
    return result < 0 ? -errno : (int64_t)result;
}

/* Cell 360-370: Missing syscalls */

static int64_t vsl_sys_select(uint64_t nfds, uint64_t readfds, uint64_t writefds,
                               uint64_t exceptfds, uint64_t timeout, uint64_t f) {
    (void)f;
    fd_set *rfds = readfds ? (fd_set *)readfds : NULL;
    fd_set *wfds = writefds ? (fd_set *)writefds : NULL;
    fd_set *efds = exceptfds ? (fd_set *)exceptfds : NULL;
    struct timeval *tv = timeout ? (struct timeval *)timeout : NULL;
    int result = select((int)nfds, rfds, wfds, efds, tv);
    return result < 0 ? -errno : (int64_t)result;
}

static int64_t vsl_sys_pipe2(uint64_t pipefd, uint64_t flags, uint64_t c,
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

static int64_t vsl_sys_clone3(uint64_t cl_args, uint64_t size, uint64_t c,
                              uint64_t d, uint64_t e, uint64_t f) {
    int result = (int)syscall(SYS_clone3, (void *)cl_args, (size_t)size);
    return result < 0 ? -errno : (int64_t)result;
}

static int64_t vsl_sys_io_uring_setup(uint64_t entries, uint64_t params,
                                      uint64_t c, uint64_t d, uint64_t e, uint64_t f) {
    (void)c; (void)d; (void)e; (void)f;
    long result = syscall(SYS_io_uring_setup, (unsigned int)entries, (void *)params);
    return result < 0 ? -errno : (int64_t)result;
}

static int64_t vsl_sys_io_uring_enter(uint64_t fd, uint64_t to_submit,
                                       uint64_t min_complete, uint64_t flags,
                                       uint64_t arg, uint64_t sz) {
    (void)arg; (void)sz;
    long result = syscall(SYS_io_uring_enter, (int)fd, (unsigned int)to_submit,
                          (unsigned int)min_complete, (unsigned int)flags,
                          (void *)arg, (size_t)sz);
    return result < 0 ? -errno : (int64_t)result;
}

static int64_t vsl_sys_io_uring_register(uint64_t fd, uint64_t opcode,
                                          uint64_t arg, uint64_t nr_args,
                                          uint64_t e, uint64_t f) {
    (void)e; (void)f;
    long result = syscall(SYS_io_uring_register, (int)fd, (unsigned int)opcode,
                          (void *)arg, (unsigned int)nr_args);
    return result < 0 ? -errno : (int64_t)result;
}

static int64_t vsl_sys_readlinkat(uint64_t dirfd, uint64_t pathname, uint64_t buf,
                                   uint64_t bufsiz, uint64_t e, uint64_t f) {
    (void)e; (void)f;
    ssize_t result = readlinkat((int)dirfd, (const char *)pathname,
                                (char *)buf, (size_t)bufsiz);
    return result < 0 ? -errno : (int64_t)result;
}

static int64_t vsl_sys_fchmodat(uint64_t dirfd, uint64_t pathname, uint64_t mode,
                                 uint64_t flags, uint64_t e, uint64_t f) {
    (void)e; (void)f;
    int result = fchmodat((int)dirfd, (const char *)pathname,
                          (mode_t)mode, (int)flags);
    return result < 0 ? -errno : (int64_t)result;
}

static int64_t vsl_sys_fchownat(uint64_t dirfd, uint64_t pathname, uint64_t owner,
                                 uint64_t group, uint64_t flags, uint64_t f) {
    (void)f;
    int result = fchownat((int)dirfd, (const char *)pathname,
                          (uid_t)owner, (gid_t)group, (int)flags);
    return result < 0 ? -errno : (int64_t)result;
}

static int64_t vsl_sys_utimensat(uint64_t dirfd, uint64_t pathname, uint64_t times,
                                  uint64_t flags, uint64_t e, uint64_t f) {
    (void)e; (void)f;
    const struct timespec *ts = times ? (const struct timespec *)times : NULL;
    int result = utimensat((int)dirfd, (const char *)pathname, ts, (int)flags);
    return result < 0 ? -errno : (int64_t)result;
}

static int64_t vsl_sys_futimesat(uint64_t dirfd, uint64_t pathname, uint64_t times,
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

static int64_t vsl_sys_renameat(uint64_t olddirfd, uint64_t oldpath, uint64_t newdirfd,
                                 uint64_t newpath, uint64_t e, uint64_t f) {
    (void)e; (void)f;
    int result = renameat((int)olddirfd, (const char *)oldpath,
                          (int)newdirfd, (const char *)newpath);
    return result < 0 ? -errno : (int64_t)result;
}

static int64_t vsl_sys_mkdirat(uint64_t dirfd, uint64_t pathname, uint64_t mode,
                                uint64_t d, uint64_t e, uint64_t f) {
    (void)d; (void)e; (void)f;
    int result = mkdirat((int)dirfd, (const char *)pathname, (mode_t)mode);
    return result < 0 ? -errno : (int64_t)result;
}

static int64_t vsl_sys_symlinkat(uint64_t target, uint64_t newdirfd,
                                  uint64_t linkpath, uint64_t d, uint64_t e, uint64_t f) {
    (void)d; (void)e; (void)f;
    int result = symlinkat((const char *)target, (int)newdirfd,
                           (const char *)linkpath);
    return result < 0 ? -errno : (int64_t)result;
}

static int64_t vsl_sys_linkat(uint64_t olddirfd, uint64_t oldpath, uint64_t newdirfd,
                               uint64_t newpath, uint64_t flags, uint64_t f) {
    (void)f;
    int result = linkat((int)olddirfd, (const char *)oldpath,
                        (int)newdirfd, (const char *)newpath, (int)flags);
    return result < 0 ? -errno : (int64_t)result;
}

static int64_t vsl_sys_mknodat(uint64_t dirfd, uint64_t pathname, uint64_t mode,
                                uint64_t dev, uint64_t e, uint64_t f) {
    (void)e; (void)f;
    int result = mknodat((int)dirfd, (const char *)pathname, (mode_t)mode, (dev_t)dev);
    return result < 0 ? -errno : (int64_t)result;
}

static int64_t vsl_sys_getwd(uint64_t buf, uint64_t size, uint64_t c,
                              uint64_t d, uint64_t e, uint64_t f) {
    (void)c; (void)d; (void)e; (void)f;
    char *result = getcwd((char *)buf, (size_t)size);
    return result ? (int64_t)buf : -errno;
}

static int64_t vsl_sys_fchdir(uint64_t fd, uint64_t b, uint64_t c,
                               uint64_t d, uint64_t e, uint64_t f) {
    (void)b; (void)c; (void)d; (void)e; (void)f;
    int result = fchdir((int)fd);
    return result < 0 ? -errno : (int64_t)result;
}

/* Cell 360-370: Namespace & Security Syscalls */

static int64_t vsl_sys_unshare(uint64_t flags, uint64_t b, uint64_t c,
                                uint64_t d, uint64_t e, uint64_t f) {
    (void)b; (void)c; (void)d; (void)e; (void)f;
    int result = unshare((int)flags);
    return result < 0 ? -errno : (int64_t)result;
}

static int64_t vsl_sys_setns(uint64_t fd, uint64_t nstype, uint64_t c,
                              uint64_t d, uint64_t e, uint64_t f) {
    (void)c; (void)d; (void)e; (void)f;
    int result = setns((int)fd, (int)nstype);
    return result < 0 ? -errno : (int64_t)result;
}

static int64_t vsl_sys_fanotify_init(uint64_t flags, uint64_t event_f_flags,
                                      uint64_t c, uint64_t d, uint64_t e, uint64_t f) {
    (void)c; (void)d; (void)e; (void)f;
    int result = syscall(SYS_fanotify_init, (unsigned int)flags, (unsigned int)event_f_flags);
    return result < 0 ? -errno : (int64_t)result;
}

static int64_t vsl_sys_fanotify_mark(uint64_t fanotify_fd, uint64_t flags, uint64_t mask,
                                      uint64_t dirfd, uint64_t pathname, uint64_t f) {
    (void)f;
    int result = syscall(SYS_fanotify_mark, (int)fanotify_fd, (unsigned int)flags,
                         (uint64_t)mask, (int)dirfd, (const char *)pathname);
    return result < 0 ? -errno : (int64_t)result;
}

static int64_t vsl_sys_landlock(uint64_t cmd, uint64_t attr, uint64_t flags,
                                 uint64_t c, uint64_t d, uint64_t e, uint64_t f) {
    (void)c; (void)d; (void)e; (void)f;
    int result = syscall(444, (int)cmd, (void *)attr, (size_t)flags);
    return result < 0 ? -errno : (int64_t)result;
}

static int64_t vsl_sys_bpf(uint64_t cmd, uint64_t attr, uint64_t size,
                            uint64_t c, uint64_t d, uint64_t e, uint64_t f) {
    (void)c; (void)d; (void)e; (void)f;
    int result = syscall(321, (int)cmd, (void *)attr, (size_t)size);
    return result < 0 ? -errno : (int64_t)result;
}

static int64_t vsl_sys_perf_event_open(uint64_t attr, uint64_t pid, uint64_t cpu,
                                        uint64_t group_fd, uint64_t flags, uint64_t f) {
    (void)f;
    struct perf_event_attr *pe = (struct perf_event_attr *)attr;
    int result = syscall(SYS_perf_event_open, pe, (pid_t)pid, (int)cpu,
                         (int)group_fd, (unsigned long)flags);
    return result < 0 ? -errno : (int64_t)result;
}

/* -- Syscall Table ------------------------------------------------- */

static const vsl_syscall_fn vsl_syscall_table[] = {
    [VSL_SYS_READ]         = vsl_sys_read,
    [VSL_SYS_WRITE]        = vsl_sys_write,
    [VSL_SYS_OPEN]         = vsl_sys_open,
    [VSL_SYS_CLOSE]        = vsl_sys_close,
    [VSL_SYS_STAT]         = vsl_sys_stat,
    [VSL_SYS_FSTAT]        = vsl_sys_fstat,
    [VSL_SYS_LSEEK]        = vsl_sys_lseek,
    [VSL_SYS_MMAP]         = vsl_sys_mmap,
    [VSL_SYS_MUNMAP]       = vsl_sys_munmap,
    [VSL_SYS_BRK]          = vsl_sys_brk,
    [VSL_SYS_IOCTL]        = vsl_sys_ioctl,
    [VSL_SYS_ACCESS]       = vsl_sys_access,
    [VSL_SYS_PIPE]         = vsl_sys_pipe,
    [VSL_SYS_FORK]         = vsl_sys_fork,
    [VSL_SYS_EXECVE]       = vsl_sys_execve,
    [VSL_SYS_EXIT]         = vsl_sys_exit,
    [VSL_SYS_WAIT4]        = vsl_sys_wait4,
    [VSL_SYS_KILL]         = vsl_sys_kill,
    [VSL_SYS_GETPID]       = vsl_sys_getpid,
    [VSL_SYS_GETPPID]      = vsl_sys_getppid,
    [VSL_SYS_GETUID]       = vsl_sys_getuid,
    [VSL_SYS_GETGID]       = vsl_sys_getgid,
    [VSL_SYS_CLONE]        = vsl_sys_clone,
    [VSL_SYS_DUP]          = vsl_sys_dup,
    [VSL_SYS_DUP2]         = vsl_sys_dup2,
    [VSL_SYS_FCNTL]        = vsl_sys_fcntl,
    [VSL_SYS_FSYNC]        = vsl_sys_fsync,
    [VSL_SYS_UNLINK]       = vsl_sys_unlink,
    [VSL_SYS_MKDIR]        = vsl_sys_mkdir,
    [VSL_SYS_RMDIR]        = vsl_sys_rmdir,
    [VSL_SYS_RENAME]       = vsl_sys_rename,
    [VSL_SYS_GETCWD]       = vsl_sys_getcwd,
    [VSL_SYS_CHDIR]        = vsl_sys_chdir,
    [VSL_SYS_SCHED_YIELD]  = vsl_sys_sched_yield,
    [VSL_SYS_CLOCK_GETTIME]= vsl_sys_clock_gettime,
    [VSL_SYS_RT_SIGACTION]  = vsl_sys_rt_sigaction,
    [VSL_SYS_RT_SIGPROCMASK]= vsl_sys_rt_sigprocmask,
    [VSL_SYS_FUTEX]         = vsl_sys_futex,
    [VSL_SYS_EXIT_GROUP]   = vsl_sys_exit_group,
    [VSL_SYS_SOCKET]        = vsl_sys_socket,
    [VSL_SYS_CONNECT]       = vsl_sys_connect,
    [VSL_SYS_ACCEPT]        = vsl_sys_accept,
    [VSL_SYS_BIND]          = vsl_sys_bind,
    [VSL_SYS_LISTEN]        = vsl_sys_listen,
    [VSL_SYS_VFORK]         = vsl_sys_vfork,
    [VSL_SYS_PIPE2]         = vsl_sys_pipe2,

    /* Cell 360-370: New syscalls */
    [VSL_SYS_PREAD64]      = vsl_sys_pread64,
    [VSL_SYS_PWRITE64]     = vsl_sys_pwrite64,
    [VSL_SYS_READV]        = vsl_sys_readv,
    [VSL_SYS_WRITEV]       = vsl_sys_writev,
    [VSL_SYS_SENDTO]       = vsl_sys_sendto,
    [VSL_SYS_RECVFROM]     = vsl_sys_recvfrom,
    [VSL_SYS_SHUTDOWN]     = vsl_sys_shutdown,
    [VSL_SYS_GETSOCKNAME]  = vsl_sys_getsockname,
    [VSL_SYS_GETPEERNAME]  = vsl_sys_getpeername,
    [VSL_SYS_SETSOCKOPT]   = vsl_sys_setsockopt,
    [VSL_SYS_SOCKETPAIR]   = vsl_sys_socketpair,
    [VSL_SYS_TRUNCATE]     = vsl_sys_truncate,
    [VSL_SYS_FTRUNCATE]    = vsl_sys_ftruncate,
    [VSL_SYS_CREAT]        = vsl_sys_creat,
    [VSL_SYS_SYMLINK]      = vsl_sys_symlink,
    [VSL_SYS_READLINK]     = vsl_sys_readlink,
    [VSL_SYS_CHMOD]        = vsl_sys_chmod,
    [VSL_SYS_FCHMOD]       = vsl_sys_fchmod,
    [VSL_SYS_CHOWN]        = vsl_sys_chown,
    [VSL_SYS_LCHOWN]       = vsl_sys_lchown,
    [VSL_SYS_FCHOWN]       = vsl_sys_fchown,
    [VSL_SYS_LSTAT]        = vsl_sys_lstat,
    [VSL_SYS_OPENAT]       = vsl_sys_openat,
    [VSL_SYS_NEWFSTATAT]   = vsl_sys_newfstatat,
    [VSL_SYS_FSTATAT]      = vsl_sys_newfstatat,
    [VSL_SYS_UNLINKAT]     = vsl_sys_unlinkat,
    [VSL_SYS_FACCESSAT]    = vsl_sys_faccessat,
    [VSL_SYS_POLL]         = vsl_sys_poll,
    [VSL_SYS_EPOLL_CREATE] = vsl_sys_epoll_create,
    [VSL_SYS_EPOLL_CTL]    = vsl_sys_epoll_ctl,
    [VSL_SYS_EPOLL_WAIT]   = vsl_sys_epoll_wait,

    /* Cell 360-370: More syscalls */
    [VSL_SYS_GETDENTS64]  = vsl_sys_getdents64,
    [VSL_SYS_STATX]       = vsl_sys_statx,
    [VSL_SYS_MREMAP]      = vsl_sys_mremap,
    [VSL_SYS_MPROTECT]    = vsl_sys_mprotect,
    [VSL_SYS_MSYNC]       = vsl_sys_msync,
    [VSL_SYS_RT_SIGSUSPEND]  = vsl_sys_rt_sigsuspend,
    [VSL_SYS_RT_SIGPENDING]  = vsl_sys_rt_sigpending,
    [VSL_SYS_RT_SIGQUEUEINFO] = vsl_sys_rt_sigqueueinfo,
    [VSL_SYS_RT_SIGTIMEDWAIT] = vsl_sys_rt_sigtimedwait,
    [VSL_SYS_TIMER_CREATE]  = vsl_sys_timer_create,
    [VSL_SYS_TIMER_SETTIME] = vsl_sys_timer_settime,
    [VSL_SYS_TIMER_GETTIME] = vsl_sys_timer_gettime,
    [VSL_SYS_TIMER_DELETE]  = vsl_sys_timer_delete,
    [VSL_SYS_TIMERFD_CREATE] = vsl_sys_timerfd_create,
    [VSL_SYS_TIMERFD_SETTIME] = vsl_sys_timerfd_settime,
    [VSL_SYS_TIMERFD_GETTIME] = vsl_sys_timerfd_gettime,
    [VSL_SYS_EVENTFD]      = vsl_sys_eventfd,
    [VSL_SYS_EVENTFD2]     = vsl_sys_eventfd2,
    [VSL_SYS_INOTIFY_INIT] = vsl_sys_inotify_init,
    [VSL_SYS_INOTIFY_ADD_WATCH] = vsl_sys_inotify_add_watch,
    [VSL_SYS_INOTIFY_RM_WATCH] = vsl_sys_inotify_rm_watch,
    [VSL_SYS_SIGNALFD]     = vsl_sys_signalfd,
    [VSL_SYS_SIGNALFD4]    = vsl_sys_signalfd4,
    [VSL_SYS_PSELECT6]     = vsl_sys_pselect6,
    [VSL_SYS_PPOLL]        = vsl_sys_ppoll,

    /* Cell 360-370: io_uring syscalls */
    [VSL_SYS_IO_URING_SETUP]    = vsl_sys_io_uring_setup,
    [VSL_SYS_IO_URING_ENTER]    = vsl_sys_io_uring_enter,
    [VSL_SYS_IO_URING_REGISTER] = vsl_sys_io_uring_register,

    /* Cell 360-370: Missing syscalls now implemented */
    [VSL_SYS_SELECT]       = vsl_sys_select,
    [VSL_SYS_FCHDIR]       = vsl_sys_fchdir,
    [VSL_SYS_GETWD]        = vsl_sys_getwd,
    [VSL_SYS_MKDIRAT]      = vsl_sys_mkdirat,
    [VSL_SYS_MKNODAT]      = vsl_sys_mknodat,
    [VSL_SYS_FCHOWNAT]     = vsl_sys_fchownat,
    [VSL_SYS_FUTIMESAT]    = vsl_sys_futimesat,
    [VSL_SYS_RENAMEAT]     = vsl_sys_renameat,
    [VSL_SYS_LINKAT]       = vsl_sys_linkat,
    [VSL_SYS_SYMLINKAT]    = vsl_sys_symlinkat,
    [VSL_SYS_READLINKAT]   = vsl_sys_readlinkat,
    [VSL_SYS_FCHMODAT]     = vsl_sys_fchmodat,
    [VSL_SYS_UTIMENSAT]    = vsl_sys_utimensat,
    [VSL_SYS_CLONE3]       = vsl_sys_clone3,

    /* Cell 360-370: Namespace & Security Syscalls (NEW) */
    [VSL_SYS_UNSHARE]           = vsl_sys_unshare,
    [VSL_SYS_SETNS]             = vsl_sys_setns,
    [VSL_SYS_FANOTIFY_INIT]     = vsl_sys_fanotify_init,
    [VSL_SYS_FANOTIFY_MARK]     = vsl_sys_fanotify_mark,
    [VSL_SYS_LANDLOCK]          = vsl_sys_landlock,
    [VSL_SYS_BPF]               = vsl_sys_bpf,
    [VSL_SYS_PERF_EVENT_OPEN]   = vsl_sys_perf_event_open,
};

#define VSL_SYSCALL_TABLE_SIZE (sizeof(vsl_syscall_table) / sizeof(vsl_syscall_table[0]))

/* -- Syscall Bridge ----------------------------------------------- */

int64_t vsl_syscall(uint64_t num, uint64_t rdi, uint64_t rsi,
                    uint64_t rdx, uint64_t r10, uint64_t r8, uint64_t r9) {
    if (!g_vsl.active) return -1;
    g_vsl.syscall_count++;

    if (num < VSL_SYSCALL_TABLE_SIZE && vsl_syscall_table[num]) {
        return vsl_syscall_table[num](rdi, rsi, rdx, r10, r8, r9);
    }

    g_vsl.syscall_errors++;
    return -38; /* ENOSYS */
}

int64_t vsl_syscall_dispatch(uint64_t num, uint64_t *regs) {
    return vsl_syscall(num, regs[0], regs[1], regs[2], regs[3], regs[4], regs[5]);
}

/* -- Helper Functions ---------------------------------------------- */

static int vsl_get_host_fd(int vsl_fd) {
    if (vsl_fd < 3) return vsl_fd; /* stdin/stdout/stderr */
    for (uint32_t i = 0; i < g_vsl.n_fds; i++) {
        if (g_vsl.fds[i].fd == vsl_fd) return g_vsl.fds[i].vsl_fd;
    }
    return -1;
}

static int find_free_vsl_pid(void) {
    for (uint32_t pid = 2; pid < 100000; pid++) {
        if (!vsl_get_process(pid)) return (int)pid;
    }
    return -1;
}

static int register_child_pid(pid_t child_host_pid, uint32_t parent_vsl_pid) {
    if (child_host_pid <= 0) return -1;
    if (g_vsl.n_procs >= VSL_MAX_PROCS) return -1;
    int vsl_pid = find_free_vsl_pid();
    if (vsl_pid < 0) return -1;
    VSL_PROC *proc = &g_vsl.procs[g_vsl.n_procs];
    memset(proc, 0, sizeof(*proc));
    proc->pid = (uint32_t)vsl_pid;
    proc->ppid = parent_vsl_pid;
    proc->state = VSL_PROC_READY;
    g_vsl.n_procs++;
    return vsl_pid;
}

int vsl_openat(int dirfd, const char *pathname, int flags, mode_t mode) {
    if (!g_vsl.active) return -1;
    if (g_vsl.n_fds >= VSL_MAX_FDS) return -1;

    int fd = openat(dirfd, pathname, flags, mode);
    if (fd < 0) return -errno;

    int vsl_fd = g_vsl.n_fds + 3;
    VSL_FD *vfd = &g_vsl.fds[g_vsl.n_fds++];
    vfd->fd = vsl_fd;
    vfd->flags = (uint32_t)flags;
    vfd->mode = (uint32_t)mode;
    vfd->vsl_fd = fd; /* Store host fd */
    if (pathname) strncpy(vfd->path, pathname, 255);

    return vsl_fd;
}