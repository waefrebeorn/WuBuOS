/*
 * wubu_vsl.c  --  WuBuOS Virtualization Substrate Layer Implementation
 *
 * VSL runs a minimal Linux environment under WuBuOS ring-0.
 * It's not full virtualization  --  it's a syscall translation layer
 * with direct hardware passthrough for performance-critical drivers.
 *
 * Design principles:
 *   - WuBuOS owns the hardware (ring-0)
 *   - VSL provides Linux ABI compatibility
 *   - Syscalls are translated, not emulated
 *   - GPU/drivers use direct passthrough (no emulation overhead)
 *   - Memory is partitioned, not virtualized
 *
 * This is the "simple Colonel" interrupt handler you described:
 *   WuBuOS runs what it wants
 *   When a Linux app needs something, it interrupts through VSL
 *   VSL handles the Linux syscall, translates to WuBuOS call
 *   Result goes back through the "Colonel" interrupt path
 */

#include "wubu_vsl.h"
#include "wubu_container.h"

#define _GNU_SOURCE
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
#include <signal.h>
#include <time.h>
#include <stddef.h>
#include <sys/inotify.h>
#include <sys/mman.h>
#include <sys/timerfd.h>
#include <sys/eventfd.h>
#include <sys/signalfd.h>

/* Forward declaration */
static int vsl_get_host_fd(int vsl_fd);
#include <sys/inotify.h>
#include <sys/mman.h>
#include <sys/timerfd.h>
#include <sys/eventfd.h>
#include <sys/signalfd.h>

/* -- Global State ------------------------------------------------ */

static VSL_STATE g_vsl;

/* -- PID Mapping -------------------------------------------------- */
/* Simple mapping: VSL PID = host PID for direct children.          */
/* For multi-process, we maintain a VSL process table mapping.      */

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

/* -- Syscall Table ----------------------------------------------- */

typedef int64_t (*vsl_syscall_fn)(uint64_t, uint64_t, uint64_t,
                                   uint64_t, uint64_t, uint64_t);

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
    (void)a; (void)b; (void)c; (void)d; (void)e; (void)f;
    return (int64_t)g_vsl.current_pid;
}

static int64_t vsl_sys_getppid(uint64_t a, uint64_t b, uint64_t c,
                                uint64_t d, uint64_t e, uint64_t f) {
    (void)a; (void)b; (void)c; (void)d; (void)e; (void)f;
    VSL_PROC *p = vsl_get_process(g_vsl.current_pid);
    return p ? (int64_t)p->ppid : 0;
}

static int64_t vsl_sys_brk(uint64_t new_brk, uint64_t b, uint64_t c,
                            uint64_t d, uint64_t e, uint64_t f) {
    (void)b; (void)c; (void)d; (void)e; (void)f;
    return vsl_brk(new_brk);
}

/* Cell 360: fork implementation  --  host delegation */
static int64_t vsl_sys_fork(uint64_t a, uint64_t b, uint64_t c,
                             uint64_t d, uint64_t e, uint64_t f) {
    (void)a; (void)b; (void)c; (void)d; (void)e; (void)f;
    pid_t host_pid = fork();
    if (host_pid < 0) return -errno;
    if (host_pid == 0) {
        /* Child: update VSL current_pid to match host */
        g_vsl.current_pid = (uint32_t)host_pid;
        return 0;
    }
    /* Parent: register child in VSL process table */
    int vsl_child = register_child_pid(host_pid, g_vsl.current_pid);
    if (vsl_child < 0) {
        kill(host_pid, SIGKILL);
        waitpid(host_pid, NULL, 0);
        return -1;
    }
    return (int64_t)vsl_child;
}

/* Cell 360: clone implementation  --  simplified (fork for hosted mode) */
static int64_t vsl_sys_clone(uint64_t flags, uint64_t stack, uint64_t ptid,
                              uint64_t ctid, uint64_t tls, uint64_t f) {
    (void)stack; (void)ptid; (void)ctid; (void)tls; (void)f;
    /* In hosted mode, clone without CLONE_VM ≈ fork */
    return vsl_sys_fork(flags, 0, 0, 0, 0, 0);
}

/* Cell 360: vfork implementation  --  fork in hosted mode */
static int64_t vsl_sys_vfork(uint64_t a, uint64_t b, uint64_t c,
                              uint64_t d, uint64_t e, uint64_t f) {
    return vsl_sys_fork(a, b, c, d, e, f);
}

/* Cell 361: execve implementation  --  host delegation */
static int64_t vsl_sys_execve(uint64_t path, uint64_t argv, uint64_t envp,
                               uint64_t d, uint64_t e, uint64_t f) {
    (void)d; (void)e; (void)f;
    const char *pathname = (const char *)path;
    if (!pathname) return -2; /* ENOENT */

    /* Convert VSL argv (uint64_t*) to host char** */
    char **host_argv = NULL;
    int argc = 0;
    if (argv) {
        uint64_t *vsl_argv = (uint64_t *)argv;
        while (vsl_argv[argc]) argc++;
        host_argv = (char **)calloc((size_t)argc + 1, sizeof(char *));
        if (!host_argv) return -12; /* ENOMEM */
        for (int i = 0; i < argc; i++)
            host_argv[i] = (char *)(uintptr_t)vsl_argv[i];
    }

    execve(pathname, host_argv, (char *const *)(uintptr_t)envp);
    /* If we get here, execve failed */
    free(host_argv);
    return -errno;
}

/* Cell 362: wait4 implementation  --  host delegation */
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

/* Cell 362: waitpid → wait4 wrapper */
static int64_t vsl_sys_waitpid(uint64_t pid, uint64_t status, uint64_t options,
                                uint64_t d, uint64_t e, uint64_t f) {
    return vsl_sys_wait4(pid, status, options, 0, e, f);
}

/* Cell 364: socket implementation  --  host delegation */
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

static int64_t vsl_sys_mmap(uint64_t addr, uint64_t size, uint64_t prot,
                             uint64_t flags, uint64_t fd, uint64_t offset) {
    return (int64_t)vsl_mmap(addr, (size_t)size, (int)prot,
                              (int)flags, (int)fd, offset);
}

static int64_t vsl_sys_munmap(uint64_t addr, uint64_t size, uint64_t c,
                               uint64_t d, uint64_t e, uint64_t f) {
    (void)c; (void)d; (void)e; (void)f;
    return vsl_munmap(addr, (size_t)size);
}

static int64_t vsl_sys_write(uint64_t fd, uint64_t buf, uint64_t count,
                              uint64_t d, uint64_t e, uint64_t f) {
    (void)d; (void)e; (void)f;
    /* Cell 366: write to all fds via host delegation */
    ssize_t result = write((int)fd, (const void *)buf, (size_t)count);
    return result < 0 ? -errno : (int64_t)result;
}

static int64_t vsl_sys_read(uint64_t fd, uint64_t buf, uint64_t count,
                             uint64_t d, uint64_t e, uint64_t f) {
    (void)d; (void)e; (void)f;
    /* Cell 365: host fd delegation  --  read from real host fd */
    ssize_t result = read((int)fd, (void *)buf, (size_t)count);
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
    /* Cell 369: ioctl via host delegation */
    int result = ioctl((int)fd, (unsigned long)req, (void *)arg);
    return result < 0 ? -errno : (int64_t)result;
}

static int64_t vsl_sys_access(uint64_t path, uint64_t mode, uint64_t c,
                               uint64_t d, uint64_t e, uint64_t f) {
    (void)c; (void)d; (void)e; (void)f;
    int rc = access((const char *)path, (int)mode);
    return rc < 0 ? -errno : (int64_t)rc;
}

static int64_t vsl_sys_fsync(uint64_t fd, uint64_t b, uint64_t c,
                              uint64_t d, uint64_t e, uint64_t f) {
    (void)b; (void)c; (void)d; (void)e; (void)f;
    int rc = fsync((int)fd);
    return rc < 0 ? -errno : (int64_t)rc;
}

static int64_t vsl_sys_fcntl(uint64_t fd, uint64_t cmd, uint64_t arg,
                              uint64_t d, uint64_t e, uint64_t f) {
    (void)d; (void)e; (void)f;
    int rc = fcntl((int)fd, (int)cmd, (int)arg);
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

static int64_t vsl_sys_kill(uint64_t pid, uint64_t sig, uint64_t c,
                             uint64_t d, uint64_t e, uint64_t f) {
    (void)c; (void)d; (void)e; (void)f;
    int rc = kill((pid_t)pid, (int)sig);
    return rc < 0 ? -errno : (int64_t)rc;
}

static int64_t vsl_sys_pipe(uint64_t pipefd, uint64_t b, uint64_t c,
                             uint64_t d, uint64_t e, uint64_t f) {
    (void)b; (void)c; (void)d; (void)e; (void)f;
    /* Cell 370: pipe via host delegation */
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
    /* Cell 363: dup via host delegation */
    int result = dup((int)fd);
    return result < 0 ? -errno : (int64_t)result;
}

static int64_t vsl_sys_dup2(uint64_t oldfd, uint64_t newfd, uint64_t c,
                             uint64_t d, uint64_t e, uint64_t f) {
    (void)c; (void)d; (void)e; (void)f;
    /* Cell 363: dup2 via host delegation */
    int result = dup2((int)oldfd, (int)newfd);
    return result < 0 ? -errno : (int64_t)result;
}

/* Cell 382: Signal handling  --  host delegation */
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
    /* sigreturn is architecture-specific; in hosted mode we use sigreturn() */
    return -ENOSYS; /* Not directly callable from userspace in hosted mode */
}

/* Cell 386: Futex  --  host delegation */
static int64_t vsl_sys_futex(uint64_t uaddr, uint64_t op, uint64_t val,
                               uint64_t timeout, uint64_t uaddr2, uint64_t val3) {
    /* Cell 386: futex via host delegation */
    int *addr = (int *)uaddr;
    int rc = (int)syscall(SYS_futex, addr, (int)op, (int)val,
                          (struct timespec *)timeout, (int *)uaddr2, (int)val3);
    return rc < 0 ? -errno : (int64_t)rc;
}

static int64_t vsl_sys_sched_yield(uint64_t a, uint64_t b, uint64_t c,
                                    uint64_t d, uint64_t e, uint64_t f) {
    (void)a; (void)b; (void)c; (void)d; (void)e; (void)f;
    return 0;
}

static int64_t vsl_sys_clock_gettime(uint64_t clk_id, uint64_t tp, uint64_t c,
                                      uint64_t d, uint64_t e, uint64_t f) {
    (void)c; (void)d; (void)e; (void)f;
    struct timespec ts;
    int rc = clock_gettime((clockid_t)clk_id, &ts);
    if (rc < 0) return -errno;
    if (tp) {
        memcpy((void *)tp, &ts, sizeof(struct timespec));
    }
    return 0;
}

static int64_t vsl_sys_exit_group(uint64_t code, uint64_t b, uint64_t c,
                                   uint64_t d, uint64_t e, uint64_t f) {
    return vsl_sys_exit(code, b, c, d, e, f);
}

/* Cell 360-370: Additional syscalls for better Linux compat */

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

/* Forward declaration for vsl_openat helper */
int vsl_openat(int dirfd, const char *pathname, int flags, mode_t mode);

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

/* Cell 360-370: poll and epoll */

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
    /* size is ignored in modern kernels; use epoll_create1(0) */
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

/* Cell 360-370: More syscalls */

static int64_t vsl_sys_getdents64(uint64_t fd, uint64_t dirp, uint64_t count,
                                   uint64_t d, uint64_t e, uint64_t f) {
    (void)d; (void)e; (void)f;
    int host_fd = vsl_get_host_fd((int)fd);
    if (host_fd < 0) return -9; /* EBADF */
    ssize_t result = syscall(SYS_getdents64, host_fd, (void *)dirp, (size_t)count);
    return result < 0 ? -errno : (int64_t)result;
}

static int64_t vsl_sys_statx(uint64_t dirfd, uint64_t pathname, uint64_t flags,
                              uint64_t mask, uint64_t statxbuf, uint64_t f) {
    (void)f; (void)mask; (void)statxbuf;
    /* statx requires kernel 4.11+ and may not be available */
    /* Fall back to fstatat / newfstatat */
    return vsl_sys_newfstatat(dirfd, pathname, 0, flags, 0, 0);
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
    struct itimerspec new_;
    struct itimerspec old_;
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

/* Helper: get host fd from VSL fd */
static int vsl_get_host_fd(int vsl_fd) {
    if (vsl_fd < 3) return vsl_fd; /* stdin/stdout/stderr */
    for (uint32_t i = 0; i < g_vsl.n_fds; i++) {
        if (g_vsl.fds[i].fd == vsl_fd) return g_vsl.fds[i].vsl_fd;
    }
    return -1;
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

/* vsl_openat helper for openat syscall */
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

/* Syscall table  --  indexed by syscall number */
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
    [VSL_SYS_RT_SIGACTION]  = vsl_sys_sigaction,
    [VSL_SYS_RT_SIGPROCMASK]= vsl_sys_sigprocmask,
    [VSL_SYS_FUTEX]         = vsl_sys_futex,
    [VSL_SYS_EXIT_GROUP]   = vsl_sys_exit_group,
    [VSL_SYS_SOCKET]        = vsl_sys_socket,
    [VSL_SYS_CONNECT]       = vsl_sys_connect,
    [VSL_SYS_ACCEPT]        = vsl_sys_accept,
    [VSL_SYS_BIND]          = vsl_sys_bind,
    [VSL_SYS_LISTEN]        = vsl_sys_listen,
    [VSL_SYS_VFORK]         = vsl_sys_vfork,
    [VSL_SYS_PIPE2]         = vsl_sys_pipe,

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
};

#define VSL_SYSCALL_TABLE_SIZE (sizeof(vsl_syscall_table) / sizeof(vsl_syscall_table[0]))

/* -- Lifecycle --------------------------------------------------- */

int vsl_init(void) {
    if (g_vsl.active) return 0;

    memset(&g_vsl, 0, sizeof(g_vsl));
    g_vsl.version_major = VSL_VERSION_MAJOR;
    g_vsl.version_minor = VSL_VERSION_MINOR;
    g_vsl.kernel_base = VSL_KERNEL_BASE;
    g_vsl.kernel_size = VSL_KERNEL_SIZE;
    g_vsl.user_base = VSL_USER_BASE;
    g_vsl.user_size = VSL_USER_SIZE;
    g_vsl.shared_base = VSL_SHARED_BASE;
    g_vsl.shared_size = VSL_SHARED_SIZE;

    /* Set up shared memory region  --  use heap for hosted tests */
    uint64_t *shared = (uint64_t *)calloc(4, sizeof(uint64_t));
    g_vsl.shared_cmd    = &shared[0];
    g_vsl.shared_arg    = &shared[1];
    g_vsl.shared_ret    = &shared[2];
    g_vsl.shared_status = &shared[3];

    /* Initialize shared memory */
    *g_vsl.shared_cmd = 0;
    *g_vsl.shared_arg = 0;
    *g_vsl.shared_ret = 0;
    *g_vsl.shared_status = 0;

    /* Create init process (PID 1) */
    VSL_PROC *init = &g_vsl.procs[0];
    init->pid = 1;
    init->ppid = 0;
    init->state = VSL_PROC_READY;
    init->entry_point = 0;
    init->stack_pointer = VSL_USER_BASE + VSL_USER_SIZE - 0x1000ULL;
    init->brk = VSL_USER_BASE + 0x100000; /* 1MB into user space */
    init->mmap_base = VSL_USER_BASE + 0x1000000; /* 16MB into user space */
    g_vsl.n_procs = 1;
    g_vsl.current_pid = 1;

    g_vsl.active = true;
    return 0;
}

void vsl_shutdown(void) {
    if (!g_vsl.active) return;
    memset(&g_vsl, 0, sizeof(g_vsl));
}

bool vsl_active(void) {
    return g_vsl.active;
}

/* -- Process Management ------------------------------------------- */

VSL_PROC *vsl_get_process(uint32_t pid) {
    for (uint32_t i = 0; i < g_vsl.n_procs; i++) {
        if (g_vsl.procs[i].pid == pid) return &g_vsl.procs[i];
    }
    return NULL;
}

int vsl_create_process(const void *elf_data, size_t elf_size) {
    if (!g_vsl.active) return -1;
    if (g_vsl.n_procs >= VSL_MAX_PROCS) return -1;

    uint64_t entry;
    if (vsl_elf_validate(elf_data, elf_size, &entry) != 0) return -1;

    uint32_t pid = g_vsl.n_procs + 1;
    VSL_PROC *proc = &g_vsl.procs[g_vsl.n_procs];
    memset(proc, 0, sizeof(*proc));
    proc->pid = pid;
    proc->ppid = g_vsl.current_pid;
    proc->state = VSL_PROC_READY;
    proc->entry_point = entry;
    proc->stack_pointer = VSL_USER_BASE + VSL_USER_SIZE - 0x1000ULL;
    proc->brk = VSL_USER_BASE + 0x100000;
    proc->mmap_base = VSL_USER_BASE + 0x1000000;

    g_vsl.n_procs++;
    return (int)pid;
}

int vsl_destroy_process(uint32_t pid) {
    VSL_PROC *proc = vsl_get_process(pid);
    if (!proc) return -1;
    proc->state = VSL_PROC_DEAD;
    return 0;
}

int vsl_list_processes(VSL_PROC *out, int max_count) {
    int count = 0;
    for (uint32_t i = 0; i < g_vsl.n_procs && count < max_count; i++) {
        if (g_vsl.procs[i].state != VSL_PROC_UNUSED &&
            g_vsl.procs[i].state != VSL_PROC_DEAD) {
            out[count++] = g_vsl.procs[i];
        }
    }
    return count;
}

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

/* -- Memory Management -------------------------------------------- */

uint64_t vsl_mmap(uint64_t addr, size_t size, int prot, int flags,
                  int fd, uint64_t offset) {
    if (!g_vsl.active) return 0;
    if (g_vsl.n_mmaps >= VSL_MAX_MMAPS) return 0;

    VSL_PROC *proc = vsl_get_process(g_vsl.current_pid);
    if (!proc) return 0;

    /* Simple allocation: bump pointer from mmap_base */
    uint64_t alloc_addr = addr ? addr : proc->mmap_base;
    if (alloc_addr < VSL_USER_BASE) alloc_addr = VSL_USER_BASE;
    if ((uint64_t)alloc_addr + (uint64_t)size > (uint64_t)VSL_USER_BASE + (uint64_t)VSL_USER_SIZE) return 0;

    VSL_MMAP *mm = &g_vsl.mmaps[g_vsl.n_mmaps++];
    mm->addr = alloc_addr;
    mm->size = size;
    mm->prot = prot;
    mm->flags = flags;
    mm->fd = fd;
    mm->offset = offset;

    proc->mmap_base = alloc_addr + size;
    return alloc_addr;
}

int vsl_munmap(uint64_t addr, size_t size) {
    if (!g_vsl.active) return -1;
    for (uint32_t i = 0; i < g_vsl.n_mmaps; i++) {
        if (g_vsl.mmaps[i].addr == addr && g_vsl.mmaps[i].size == size) {
            /* Remove by shifting */
            for (uint32_t j = i; j < g_vsl.n_mmaps - 1; j++)
                g_vsl.mmaps[j] = g_vsl.mmaps[j + 1];
            g_vsl.n_mmaps--;
            return 0;
        }
    }
    return -1;
}

int64_t vsl_brk(uint64_t new_brk) {
    VSL_PROC *proc = vsl_get_process(g_vsl.current_pid);
    if (!proc) return -1;

    if (new_brk == 0) return (int64_t)proc->brk; /* query */

    if (new_brk < VSL_USER_BASE || (uint64_t)new_brk >= (uint64_t)VSL_USER_BASE + (uint64_t)VSL_USER_SIZE)
        return -1;

    proc->brk = new_brk;
    return (int64_t)new_brk;
}

/* -- File Operations ---------------------------------------------- */

int vsl_open(const char *path, int flags, int mode) {
    if (!g_vsl.active) return -1;
    if (g_vsl.n_fds >= VSL_MAX_FDS) return -1;
    if (!path) return -2; /* ENOENT */

    int host_fd = open(path, flags, mode);
    if (host_fd < 0) return -errno;

    int vsl_fd = g_vsl.n_fds + 3; /* 0=stdin, 1=stdout, 2=stderr */
    VSL_FD *vfd = &g_vsl.fds[g_vsl.n_fds++];
    vfd->fd = vsl_fd;
    vfd->flags = (uint32_t)flags;
    vfd->mode = (uint32_t)mode;
    vfd->vsl_fd = host_fd; /* Store host fd for delegation */
    strncpy(vfd->path, path, 255);
    vfd->path[255] = '\0';

    return vsl_fd;
}

int vsl_close(int fd) {
    if (fd < 3) return -1; /* Can't close stdin/stdout/stderr */
    for (uint32_t i = 0; i < g_vsl.n_fds; i++) {
        if (g_vsl.fds[i].fd == fd) {
            int host_fd = g_vsl.fds[i].vsl_fd;
            if (host_fd >= 0) close(host_fd);
            for (uint32_t j = i; j < g_vsl.n_fds - 1; j++)
                g_vsl.fds[j] = g_vsl.fds[j + 1];
            g_vsl.n_fds--;
            return 0;
        }
    }
    return -1;
}

int64_t vsl_read(int fd, void *buf, size_t count) {
    if (!g_vsl.active) return -1;
    if (!buf && count) return -1;
    for (uint32_t i = 0; i < g_vsl.n_fds; i++) {
        if (g_vsl.fds[i].fd == fd) {
            int host_fd = g_vsl.fds[i].vsl_fd;
            if (host_fd < 0) return -1;
            ssize_t n = read(host_fd, buf, count);
            return (n >= 0) ? (int64_t)n : -1;
        }
    }
    return -1;
}

int64_t vsl_write(int fd, const void *buf, size_t count) {
    if (!g_vsl.active) return -1;
    if (!buf && count) return -1;
    for (uint32_t i = 0; i < g_vsl.n_fds; i++) {
        if (g_vsl.fds[i].fd == fd) {
            int host_fd = g_vsl.fds[i].vsl_fd;
            if (host_fd < 0) return -1;
            if (fd == 1 || fd == 2) {
                printf("%.*s", (int)count, (const char *)buf);
                return (int64_t)count;
            }
            ssize_t n = write(host_fd, buf, count);
            return (n >= 0) ? (int64_t)n : -1;
        }
    }
    return -1;
}

int64_t vsl_lseek(int fd, int64_t offset, int whence) {
    if (!g_vsl.active) return -1;
    /* For stdin/stdout/stderr, delegate directly */
    if (fd < 3) {
        off_t result = lseek(fd, (off_t)offset, whence);
        return (result < 0) ? -errno : (int64_t)result;
    }
    /* Look up host fd from VSL fd table */
    for (uint32_t i = 0; i < g_vsl.n_fds; i++) {
        if (g_vsl.fds[i].fd == fd) {
            int host_fd = g_vsl.fds[i].vsl_fd;
            if (host_fd < 0) return -9; /* EBADF */
            off_t result = lseek(host_fd, (off_t)offset, whence);
            return (result < 0) ? -errno : (int64_t)result;
        }
    }
    return -9; /* EBADF */
}

/* -- Driver Management ------------------------------------------- */

int vsl_register_driver(VSL_DRV_TYPE type, uint64_t io_base,
                        uint64_t mem_base, size_t mem_size, uint32_t irq) {
    if (!g_vsl.active) return -1;
    if (g_vsl.n_drivers >= 16) return -1;

    int id = (int)g_vsl.n_drivers;
    VSL_DRV *drv = &g_vsl.drivers[id];
    drv->type = type;
    drv->active = false;
    drv->io_base = io_base;
    drv->mem_base = mem_base;
    drv->mem_size = mem_size;
    drv->irq = irq;
    drv->priv = NULL;

    g_vsl.n_drivers++;
    return id;
}

int vsl_activate_driver(int drv_id) {
    if (drv_id < 0 || drv_id >= (int)g_vsl.n_drivers) return -1;
    g_vsl.drivers[drv_id].active = true;
    return 0;
}

int vsl_deactivate_driver(int drv_id) {
    if (drv_id < 0 || drv_id >= (int)g_vsl.n_drivers) return -1;
    g_vsl.drivers[drv_id].active = false;
    return 0;
}

bool vsl_driver_active(VSL_DRV_TYPE type) {
    for (uint32_t i = 0; i < g_vsl.n_drivers; i++) {
        if (g_vsl.drivers[i].type == type && g_vsl.drivers[i].active)
            return true;
    }
    return false;
}

VSL_DRV *vsl_get_driver(VSL_DRV_TYPE type) {
    for (uint32_t i = 0; i < g_vsl.n_drivers; i++) {
        if (g_vsl.drivers[i].type == type) return &g_vsl.drivers[i];
    }
    return NULL;
}

/* -- Shared Memory ------------------------------------------------ */

int vsl_send_cmd(uint64_t cmd, uint64_t arg) {
    if (!g_vsl.active) return -1;
    *g_vsl.shared_cmd = cmd;
    *g_vsl.shared_arg = arg;
    *g_vsl.shared_status = 1; /* command pending */
    return 0;
}

uint64_t vsl_read_response(void) {
    return *g_vsl.shared_ret;
}

uint64_t vsl_get_status(void) {
    return *g_vsl.shared_status;
}

/* -- ELF Loading -------------------------------------------------- */

int vsl_elf_validate(const void *elf_data, size_t elf_size,
                     uint64_t *out_entry) {
    if (!elf_data || elf_size < 64) return -1;

    const uint8_t *p = (const uint8_t *)elf_data;

    /* ELF magic */
    if (p[0] != 0x7F || p[1] != 'E' || p[2] != 'L' || p[3] != 'F')
        return -1;

    /* 64-bit ELF */
    if (p[4] != 2) return -1;

    /* Little-endian */
    if (p[5] != 1) return -1;

    /* ELF version */
    if (p[6] != 1) return -1;

    /* e_type: ET_EXEC (2) or ET_DYN (3) */
    uint16_t e_type = (uint16_t)p[16] | ((uint16_t)p[17] << 8);
    if (e_type != 2 && e_type != 3) return -1;

    /* e_machine: x86_64 (0x3E) */
    uint16_t e_machine = (uint16_t)p[18] | ((uint16_t)p[19] << 8);
    if (e_machine != 0x3E) return -1;

    /* Entry point */
    if (out_entry) {
        memcpy(out_entry, p + 24, 8);
    }

    return 0;
}

uint64_t vsl_elf_load(const void *elf_data, size_t elf_size) {
    uint64_t entry;
    if (vsl_elf_validate(elf_data, elf_size, &entry) != 0) return 0;

    const uint8_t *p = (const uint8_t *)elf_data;

    /* Parse ELF64 program headers to load PT_LOAD segments */
    uint16_t e_phnum = (uint16_t)p[56] | ((uint16_t)p[57] << 8);
    uint16_t e_phentsize = (uint16_t)p[54] | ((uint16_t)p[55] << 8);
    uint64_t e_phoff;
    memcpy(&e_phoff, p + 32, 8);

    if (e_phoff + (uint64_t)e_phnum * e_phentsize > elf_size) return entry;

    const uint8_t *phdr = p + e_phoff;

    for (int i = 0; i < e_phnum; i++) {
        const uint8_t *cur = phdr + i * e_phentsize;

        uint32_t p_type;
        memcpy(&p_type, cur, 4);

        if (p_type != 1) continue; /* PT_LOAD = 1 */

        uint64_t p_offset, p_vaddr, p_filesz, p_memsz;
        memcpy(&p_offset, cur + 8, 8);
        memcpy(&p_vaddr, cur + 16, 8);
        memcpy(&p_filesz, cur + 32, 8);
        memcpy(&p_memsz, cur + 40, 8);

        if (p_memsz == 0) continue;

        /* Validate segment is within ELF data */
        if (p_offset + p_filesz > elf_size) continue;

        /* Calculate destination address in VSL user space */
        uint64_t dest_addr = VSL_USER_BASE + (p_vaddr & 0x00FFFFFF);

        /* Ensure allocated memory via mmap if needed */
        uint64_t page_start = dest_addr & ~0xFFFULL;
        uint64_t page_end = (dest_addr + p_memsz + 0xFFF) & ~0xFFFULL;
        size_t map_size = (size_t)(page_end - page_start);

        /* Check if this region is already tracked */
        int existing = -1;
        for (int j = 0; j < (int)g_vsl.n_mmaps; j++) {
            if (g_vsl.mmaps[j].addr <= dest_addr &&
                g_vsl.mmaps[j].addr + g_vsl.mmaps[j].size >= dest_addr + p_memsz) {
                existing = j;
                break;
            }
        }

        if (existing < 0 && g_vsl.n_mmaps < VSL_MAX_MMAPS) {
            int idx = g_vsl.n_mmaps++;
            g_vsl.mmaps[idx].addr = page_start;
            g_vsl.mmaps[idx].size = map_size;
            g_vsl.mmaps[idx].prot = 7; /* RWX */
            g_vsl.mmaps[idx].flags = 0x22; /* MAP_PRIVATE | MAP_ANONYMOUS */
            g_vsl.mmaps[idx].fd = -1;
            g_vsl.mmaps[idx].offset = 0;
        }

        /* Copy segment data from ELF into VSL address space */
        if (p_filesz > 0) {
            memcpy((void *)dest_addr, p + p_offset, p_filesz);
        }

        /* Zero-fill remainder (BSS) */
        if (p_memsz > p_filesz) {
            memset((void *)(dest_addr + p_filesz), 0, (size_t)(p_memsz - p_filesz));
        }
    }

    return entry;
}

int vsl_elf_interpreter(const void *elf_data, size_t elf_size,
                        char *buf, size_t buf_size) {
    (void)elf_data; (void)elf_size;
    if (buf && buf_size > 0) buf[0] = '\0';
    return -1; /* Statically linked (no interpreter) */
}

/* -- Diagnostics -------------------------------------------------- */

void vsl_info(char *buf, size_t buf_size) {
    if (!buf || buf_size == 0) return;
    snprintf(buf, buf_size,
        "VSL v%u.%u: %u procs, %u drivers, %lu syscalls (%lu errors)\n"
        "  Kernel: 0x%08llX (%zu bytes)\n"
        "  User:   0x%08llX (%zu bytes)\n"
        "  Shared: 0x%08llX (%zu bytes)\n",
        g_vsl.version_major, g_vsl.version_minor,
        g_vsl.n_procs, g_vsl.n_drivers,
        (unsigned long)g_vsl.syscall_count,
        (unsigned long)g_vsl.syscall_errors,
        (unsigned long long)g_vsl.kernel_base, g_vsl.kernel_size,
        (unsigned long long)g_vsl.user_base, g_vsl.user_size,
        (unsigned long long)g_vsl.shared_base, g_vsl.shared_size);
}

void vsl_dump_state(void) {
    char buf[512];
    vsl_info(buf, sizeof(buf));
    printf("%s", buf);

    printf("  Processes:\n");
    for (uint32_t i = 0; i < g_vsl.n_procs; i++) {
        VSL_PROC *p = &g_vsl.procs[i];
        const char *state_str[] = {"UNUSED","READY","RUNNING","BLOCKED","ZOMBIE","DEAD"};
        printf("    PID %u: state=%s entry=0x%llX stack=0x%llX brk=0x%llX\n",
               p->pid, state_str[p->state],
               (unsigned long long)p->entry_point,
               (unsigned long long)p->stack_pointer,
               (unsigned long long)p->brk);
    }

    printf("  Drivers:\n");
    const char *drv_names[] = {
        "NONE","VULKAN","CUDA","NET","BLOCK","INPUT","DISPLAY","AUDIO","USB","PCI"
    };
    for (uint32_t i = 0; i < g_vsl.n_drivers; i++) {
        VSL_DRV *d = &g_vsl.drivers[i];
        printf("    %s: %s (io=0x%llX mem=0x%llX irq=%u)\n",
               drv_names[d->type], d->active ? "active" : "inactive",
               (unsigned long long)d->io_base,
               (unsigned long long)d->mem_base, d->irq);
    }
}

void vsl_get_stats(uint64_t *out_syscalls, uint64_t *out_errors,
                   uint32_t *out_procs, uint32_t *out_drivers) {
    if (out_syscalls)  *out_syscalls = g_vsl.syscall_count;
    if (out_errors)    *out_errors = g_vsl.syscall_errors;
    if (out_procs)     *out_procs = g_vsl.n_procs;
    if (out_drivers)   *out_drivers = g_vsl.n_drivers;
}
