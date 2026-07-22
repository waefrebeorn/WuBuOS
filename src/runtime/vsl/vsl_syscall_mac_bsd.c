#include "vsl/vsl_syscall_mac_internal.h"

/* ===================================================================
 * BSD SYSCALL HANDLERS
 * Most BSD syscalls map directly to Linux syscalls.
 * =================================================================== */

int64_t mac_sys_exit(uint64_t a, uint64_t b, uint64_t c,
                             uint64_t d, uint64_t e, uint64_t f) {
    (void)b;(void)c;(void)d;(void)e;(void)f;
    _exit((int)a);
    return 0;
}

int64_t mac_sys_fork(uint64_t a, uint64_t b, uint64_t c,
                             uint64_t d, uint64_t e, uint64_t f) {
    (void)a;(void)b;(void)c;(void)d;(void)e;(void)f;
    pid_t pid = fork();
    if (pid < 0) return mac_errno(errno);
    return (int64_t)pid;
}

int64_t mac_sys_read(uint64_t a, uint64_t b, uint64_t c,
                             uint64_t d, uint64_t e, uint64_t f) {
    (void)d;(void)e;(void)f;
    ssize_t n = read((int)a, (void*)(uintptr_t)b, (size_t)c);
    if (n < 0) return mac_errno(errno);
    return (int64_t)n;
}

int64_t mac_sys_write(uint64_t a, uint64_t b, uint64_t c,
                              uint64_t d, uint64_t e, uint64_t f) {
    (void)d;(void)e;(void)f;
    ssize_t n = write((int)a, (const void*)(uintptr_t)b, (size_t)c);
    if (n < 0) return mac_errno(errno);
    return (int64_t)n;
}

int64_t mac_sys_open(uint64_t a, uint64_t b, uint64_t c,
                             uint64_t d, uint64_t e, uint64_t f) {
    (void)d;(void)e;(void)f;
    int fd = open((const char*)(uintptr_t)a, (int)b, (mode_t)c);
    if (fd < 0) return mac_errno(errno);
    return (int64_t)fd;
}

int64_t mac_sys_close(uint64_t a, uint64_t b, uint64_t c,
                              uint64_t d, uint64_t e, uint64_t f) {
    (void)b;(void)c;(void)d;(void)e;(void)f;
    if (close((int)a) < 0) return mac_errno(errno);
    return 0;
}

int64_t mac_sys_wait4(uint64_t a, uint64_t b, uint64_t c,
                              uint64_t d, uint64_t e, uint64_t f) {
    (void)d;(void)e;(void)f;
    int status;
    pid_t pid = wait4((pid_t)a, &status, (int)b, (struct rusage*)(uintptr_t)c);
    if (pid < 0) return mac_errno(errno);
    return (int64_t)pid;
}

int64_t mac_sys_unlink(uint64_t a, uint64_t b, uint64_t c,
                               uint64_t d, uint64_t e, uint64_t f) {
    (void)b;(void)c;(void)d;(void)e;(void)f;
    if (unlink((const char*)(uintptr_t)a) < 0) return mac_errno(errno);
    return 0;
}

int64_t mac_sys_execve(uint64_t a, uint64_t b, uint64_t c,
                               uint64_t d, uint64_t e, uint64_t f) {
    (void)d;(void)e;(void)f;
    /* Execute via VSL exec with environment */
    char *const *argv = (char *const *)(uintptr_t)b;
    char *const *envp = (char *const *)(uintptr_t)c;
    execve((const char*)(uintptr_t)a, argv, envp);
    return mac_errno(errno);
}

int64_t mac_sys_chdir(uint64_t a, uint64_t b, uint64_t c,
                              uint64_t d, uint64_t e, uint64_t f) {
    (void)b;(void)c;(void)d;(void)e;(void)f;
    if (chdir((const char*)(uintptr_t)a) < 0) return mac_errno(errno);
    return 0;
}

int64_t mac_sys_mkdir(uint64_t a, uint64_t b, uint64_t c,
                              uint64_t d, uint64_t e, uint64_t f) {
    (void)c;(void)d;(void)e;(void)f;
    if (mkdir((const char*)(uintptr_t)a, (mode_t)b) < 0) return mac_errno(errno);
    return 0;
}

int64_t mac_sys_rmdir(uint64_t a, uint64_t b, uint64_t c,
                              uint64_t d, uint64_t e, uint64_t f) {
    (void)b;(void)c;(void)d;(void)e;(void)f;
    if (rmdir((const char*)(uintptr_t)a) < 0) return mac_errno(errno);
    return 0;
}

int64_t mac_sys_access(uint64_t a, uint64_t b, uint64_t c,
                               uint64_t d, uint64_t e, uint64_t f) {
    (void)c;(void)d;(void)e;(void)f;
    if (access((const char*)(uintptr_t)a, (int)b) < 0) return mac_errno(errno);
    return 0;
}

int64_t mac_sys_getpid(uint64_t a, uint64_t b, uint64_t c,
                               uint64_t d, uint64_t e, uint64_t f) {
    (void)a;(void)b;(void)c;(void)d;(void)e;(void)f;
    return (int64_t)getpid();
}

int64_t mac_sys_getppid(uint64_t a, uint64_t b, uint64_t c,
                                uint64_t d, uint64_t e, uint64_t f) {
    (void)a;(void)b;(void)c;(void)d;(void)e;(void)f;
    return (int64_t)getppid();
}

int64_t mac_sys_getuid(uint64_t a, uint64_t b, uint64_t c,
                               uint64_t d, uint64_t e, uint64_t f) {
    (void)a;(void)b;(void)c;(void)d;(void)e;(void)f;
    return (int64_t)getuid();
}

int64_t mac_sys_getgid(uint64_t a, uint64_t b, uint64_t c,
                               uint64_t d, uint64_t e, uint64_t f) {
    (void)a;(void)b;(void)c;(void)d;(void)e;(void)f;
    return (int64_t)getgid();
}

int64_t mac_sys_kill(uint64_t a, uint64_t b, uint64_t c,
                             uint64_t d, uint64_t e, uint64_t f) {
    (void)c;(void)d;(void)e;(void)f;
    if (kill((pid_t)a, (int)b) < 0) return mac_errno(errno);
    return 0;
}

int64_t mac_sys_dup(uint64_t a, uint64_t b, uint64_t c,
                            uint64_t d, uint64_t e, uint64_t f) {
    (void)b;(void)c;(void)d;(void)e;(void)f;
    int fd = dup((int)a);
    if (fd < 0) return mac_errno(errno);
    return (int64_t)fd;
}

int64_t mac_sys_pipe(uint64_t a, uint64_t b, uint64_t c,
                             uint64_t d, uint64_t e, uint64_t f) {
    (void)b;(void)c;(void)d;(void)e;(void)f;
    int fds[2];
    if (pipe(fds) < 0) return mac_errno(errno);
    if (a) { ((int*)(uintptr_t)a)[0] = fds[0]; ((int*)(uintptr_t)a)[1] = fds[1]; }
    return 0;
}

int64_t mac_sys_fcntl(uint64_t a, uint64_t b, uint64_t c,
                              uint64_t d, uint64_t e, uint64_t f) {
    (void)d;(void)e;(void)f;
    int rc = fcntl((int)a, (int)b, (long)c);
    if (rc < 0) return mac_errno(errno);
    return (int64_t)rc;
}

int64_t mac_sys_ioctl(uint64_t a, uint64_t b, uint64_t c,
                              uint64_t d, uint64_t e, uint64_t f) {
    (void)d;(void)e;(void)f;
    int rc = ioctl((int)a, (unsigned long)b, (void*)(uintptr_t)c);
    if (rc < 0) return mac_errno(errno);
    return (int64_t)rc;
}

int64_t mac_sys_gettimeofday(uint64_t a, uint64_t b, uint64_t c,
                                     uint64_t d, uint64_t e, uint64_t f) {
    (void)c;(void)d;(void)e;(void)f;
    struct timeval *tv = (struct timeval*)(uintptr_t)a;
    struct timezone *tz = (struct timezone*)(uintptr_t)b;
    if (gettimeofday(tv, tz) < 0) return mac_errno(errno);
    return 0;
}

int64_t mac_sys_select(uint64_t a, uint64_t b, uint64_t c,
                               uint64_t d, uint64_t e, uint64_t f) {
    (void)e;(void)f;
    int n = select((int)a, (fd_set*)(uintptr_t)b, (fd_set*)(uintptr_t)c,
                   (fd_set*)(uintptr_t)d, NULL);
    if (n < 0) return mac_errno(errno);
    return (int64_t)n;
}

/* -- macOS mmap flag translation --------------------------------- */

/* macOS mmap flags that overlap with or differ from Linux:
 *   MAP_SHARED        0x01   (same on Linux)
 *   MAP_PRIVATE       0x02   (same)
 *   MAP_FIXED         0x10   (same)
 *   MAP_ANON          0x1000 → Linux MAP_ANONYMOUS 0x20
 *   MAP_NORESERVE     0x40   → Linux MAP_NORESERVE 0x4000
 *   MAP_NOCACHE       0x0400 → Linux no direct equiv (ignore)
 *   MAP_HASSEMAPHORE  0x0200 → Linux no direct equiv (ignore)
 *   MAP_JIT           0x0800 → mprotect with PROT_EXEC
 */
#define MAC_MAP_SHARED      0x01
#define MAC_MAP_PRIVATE     0x02
#define MAC_MAP_FIXED       0x10
#define MAC_MAP_NORESERVE   0x0040
#define MAC_MAP_ANON        0x1000
#define MAC_MAP_HASSEMAPHORE 0x0200
#define MAC_MAP_NOCACHE     0x0400
#define MAC_MAP_JIT         0x0800
#define MAC_MAP_RESILIENT_CODESIGN 0x2000
#define MAC_MAP_RESILIENT_MEDIA    0x4000

int mac_translate_mmap_flags(int mac_flags) {
    int linux_flags = 0;

    /* Convert base type */
    if (mac_flags & MAC_MAP_SHARED) linux_flags |= MAP_SHARED;
    if (mac_flags & MAC_MAP_PRIVATE) linux_flags |= MAP_PRIVATE;
    if (mac_flags & MAC_MAP_FIXED) linux_flags |= MAP_FIXED;

    /* MAP_ANON (0x1000) → MAP_ANONYMOUS (0x20) */
    if (mac_flags & MAC_MAP_ANON) linux_flags |= MAP_ANONYMOUS;

    /* MAP_NORESERVE: macOS=0x40, Linux=0x4000 */
    if (mac_flags & MAC_MAP_NORESERVE) linux_flags |= MAP_NORESERVE;

    /* HASSEMAPHORE, NOCACHE, JIT, RESILIENT_* — not supported on Linux */
    (void)MAC_MAP_HASSEMAPHORE;
    (void)MAC_MAP_NOCACHE;
    (void)MAC_MAP_JIT;
    (void)MAC_MAP_RESILIENT_CODESIGN;
    (void)MAC_MAP_RESILIENT_MEDIA;

    return linux_flags;
}

int64_t mac_sys_mmap(uint64_t a, uint64_t b, uint64_t c,
                             uint64_t d, uint64_t e, uint64_t f) {
    (void)f;
    int mac_flags = (int)d;
    int linux_flags = mac_translate_mmap_flags(mac_flags);
    void *r = mmap((void*)(uintptr_t)a, (size_t)b, (int)c,
                   linux_flags, (int)(int64_t)e, (off_t)(int64_t)f);
    if (r == MAP_FAILED) return mac_errno(errno);
    return (int64_t)(uintptr_t)r;
}

int64_t mac_sys_munmap(uint64_t a, uint64_t b, uint64_t c,
                               uint64_t d, uint64_t e, uint64_t f) {
    (void)c;(void)d;(void)e;(void)f;
    if (munmap((void*)(uintptr_t)a, (size_t)b) < 0) return mac_errno(errno);
    return 0;
}

int64_t mac_sys_socket(uint64_t a, uint64_t b, uint64_t c,
                               uint64_t d, uint64_t e, uint64_t f) {
    (void)d;(void)e;(void)f;
    int fd = socket((int)a, (int)b, (int)c);
    if (fd < 0) return mac_errno(errno);
    return (int64_t)fd;
}

int64_t mac_sys_connect(uint64_t a, uint64_t b, uint64_t c,
                                uint64_t d, uint64_t e, uint64_t f) {
    (void)d;(void)e;(void)f;
    if (connect((int)a, (const struct sockaddr*)(uintptr_t)b, (socklen_t)c) < 0)
        return mac_errno(errno);
    return 0;
}

int64_t mac_sys_accept(uint64_t a, uint64_t b, uint64_t c,
                               uint64_t d, uint64_t e, uint64_t f) {
    (void)d;(void)e;(void)f;
    socklen_t addrlen = sizeof(struct sockaddr_storage);
    int fd = accept((int)a, (struct sockaddr*)(uintptr_t)b, &addrlen);
    if (fd < 0) return mac_errno(errno);
    if (c && b) *(socklen_t*)(uintptr_t)c = addrlen;
    return (int64_t)fd;
}

int64_t mac_sys_bind(uint64_t a, uint64_t b, uint64_t c,
                             uint64_t d, uint64_t e, uint64_t f) {
    (void)d;(void)e;(void)f;
    if (bind((int)a, (const struct sockaddr*)(uintptr_t)b, (socklen_t)c) < 0)
        return mac_errno(errno);
    return 0;
}

int64_t mac_sys_listen(uint64_t a, uint64_t b, uint64_t c,
                               uint64_t d, uint64_t e, uint64_t f) {
    (void)c;(void)d;(void)e;(void)f;
    if (listen((int)a, (int)b) < 0) return mac_errno(errno);
    return 0;
}

/* -- Additional BSD syscall handlers ------------------------------ */

int64_t mac_sys_stat(uint64_t a, uint64_t b, uint64_t c,
                             uint64_t d, uint64_t e, uint64_t f) {
    (void)c;(void)d;(void)e;(void)f;
    if (stat((const char*)(uintptr_t)a, (struct stat*)(uintptr_t)b) < 0)
        return mac_errno(errno);
    return 0;
}

int64_t mac_sys_fstat(uint64_t a, uint64_t b, uint64_t c,
                              uint64_t d, uint64_t e, uint64_t f) {
    (void)c;(void)d;(void)e;(void)f;
    struct stat st;
    /* On macOS fstat works on fd directly. We translate VSL fd to host fd. */
    if (fstat((int)a, &st) < 0) return mac_errno(errno);
    /* Copy to user buffer */
    if (b) memcpy((void*)(uintptr_t)b, &st, sizeof(st));
    return 0;
}

int64_t mac_sys_lstat(uint64_t a, uint64_t b, uint64_t c,
                              uint64_t d, uint64_t e, uint64_t f) {
    (void)c;(void)d;(void)e;(void)f;
    if (lstat((const char*)(uintptr_t)a, (struct stat*)(uintptr_t)b) < 0)
        return mac_errno(errno);
    return 0;
}

int64_t mac_sys_lseek(uint64_t a, uint64_t b, uint64_t c,
                              uint64_t d, uint64_t e, uint64_t f) {
    (void)d;(void)e;(void)f;
    off_t rc = lseek((int)a, (off_t)(int64_t)b, (int)c);
    if (rc < 0) return mac_errno(errno);
    return (int64_t)rc;
}

int64_t mac_sys_getcwd(uint64_t a, uint64_t b, uint64_t c,
                               uint64_t d, uint64_t e, uint64_t f) {
    (void)c;(void)d;(void)e;(void)f;
    char *r = getcwd((char*)(uintptr_t)a, (size_t)b);
    if (!r) return mac_errno(errno);
    return 0;
}

int64_t mac_sys_geteuid(uint64_t a, uint64_t b, uint64_t c,
                                uint64_t d, uint64_t e, uint64_t f) {
    (void)a;(void)b;(void)c;(void)d;(void)e;(void)f;
    return (int64_t)geteuid();
}

int64_t mac_sys_getegid(uint64_t a, uint64_t b, uint64_t c,
                                uint64_t d, uint64_t e, uint64_t f) {
    (void)a;(void)b;(void)c;(void)d;(void)e;(void)f;
    return (int64_t)getegid();
}

int64_t mac_sys_sigaction(uint64_t a, uint64_t b, uint64_t c,
                                  uint64_t d, uint64_t e, uint64_t f) {
    (void)d;(void)e;(void)f;
    /* macOS sigaction maps roughly to Linux sigaction.
     * struct sigaction layouts differ slightly (no sa_restorer on macOS) */
    struct sigaction *act = (struct sigaction*)(uintptr_t)b;
    struct sigaction *old = (struct sigaction*)(uintptr_t)c;
    if (sigaction((int)a, act, old) < 0) return mac_errno(errno);
    return 0;
}

int64_t mac_sys_sigprocmask(uint64_t a, uint64_t b, uint64_t c,
                                    uint64_t d, uint64_t e, uint64_t f) {
    (void)d;(void)e;(void)f;
    sigset_t *set = (sigset_t*)(uintptr_t)b;
    sigset_t *old = (sigset_t*)(uintptr_t)c;
    /* Use Linux's rt_sigprocmask via syscall for compatibility */
    if (sigprocmask((int)a, set, old) < 0) return mac_errno(errno);
    return 0;
}

int64_t mac_sys_readlink(uint64_t a, uint64_t b, uint64_t c,
                                 uint64_t d, uint64_t e, uint64_t f) {
    (void)d;(void)e;(void)f;
    ssize_t r = readlink((const char*)(uintptr_t)a, (char*)(uintptr_t)b, (size_t)c);
    if (r < 0) return mac_errno(errno);
    return (int64_t)r;
}

int64_t mac_sys_symlink(uint64_t a, uint64_t b, uint64_t c,
                                uint64_t d, uint64_t e, uint64_t f) {
    (void)c;(void)d;(void)e;(void)f;
    if (symlink((const char*)(uintptr_t)a, (const char*)(uintptr_t)b) < 0)
        return mac_errno(errno);
    return 0;
}

int64_t mac_sys_rename(uint64_t a, uint64_t b, uint64_t c,
                               uint64_t d, uint64_t e, uint64_t f) {
    (void)c;(void)d;(void)e;(void)f;
    if (rename((const char*)(uintptr_t)a, (const char*)(uintptr_t)b) < 0)
        return mac_errno(errno);
    return 0;
}

int64_t mac_sys_fsync(uint64_t a, uint64_t b, uint64_t c,
                              uint64_t d, uint64_t e, uint64_t f) {
    (void)b;(void)c;(void)d;(void)e;(void)f;
    if (fsync((int)a) < 0) return mac_errno(errno);
    return 0;
}

int64_t mac_sys_ftruncate(uint64_t a, uint64_t b, uint64_t c,
                                  uint64_t d, uint64_t e, uint64_t f) {
    (void)c;(void)d;(void)e;(void)f;
    if (ftruncate((int)a, (off_t)(int64_t)b) < 0) return mac_errno(errno);
    return 0;
}

int64_t mac_sys_truncate(uint64_t a, uint64_t b, uint64_t c,
                                 uint64_t d, uint64_t e, uint64_t f) {
    (void)c;(void)d;(void)e;(void)f;
    if (truncate((const char*)(uintptr_t)a, (off_t)(int64_t)b) < 0)
        return mac_errno(errno);
    return 0;
}

int64_t mac_sys_mprotect(uint64_t a, uint64_t b, uint64_t c,
                                 uint64_t d, uint64_t e, uint64_t f) {
    (void)d;(void)e;(void)f;
    int prot = 0;
    if ((int)c & 1) prot |= PROT_READ;
    if ((int)c & 2) prot |= PROT_WRITE;
    if ((int)c & 4) prot |= PROT_EXEC;
    if (mprotect((void*)(uintptr_t)a, (size_t)b, prot) < 0)
        return mac_errno(errno);
    return 0;
}

int64_t mac_sys_madvise(uint64_t a, uint64_t b, uint64_t c,
                                uint64_t d, uint64_t e, uint64_t f) {
    (void)d;(void)e;(void)f;
    if (madvise((void*)(uintptr_t)a, (size_t)b, (int)c) < 0)
        return mac_errno(errno);
    return 0;
}

int64_t mac_sys_writev(uint64_t a, uint64_t b, uint64_t c,
                               uint64_t d, uint64_t e, uint64_t f) {
    (void)d;(void)e;(void)f;
    ssize_t r = writev((int)a, (const struct iovec*)(uintptr_t)b, (int)c);
    if (r < 0) return mac_errno(errno);
    return (int64_t)r;
}

int64_t mac_sys_readv(uint64_t a, uint64_t b, uint64_t c,
                              uint64_t d, uint64_t e, uint64_t f) {
    (void)d;(void)e;(void)f;
    ssize_t r = readv((int)a, (const struct iovec*)(uintptr_t)b, (int)c);
    if (r < 0) return mac_errno(errno);
    return (int64_t)r;
}

int64_t mac_sys_link(uint64_t a, uint64_t b, uint64_t c,
                             uint64_t d, uint64_t e, uint64_t f) {
    (void)c;(void)d;(void)e;(void)f;
    if (link((const char*)(uintptr_t)a, (const char*)(uintptr_t)b) < 0)
        return mac_errno(errno);
    return 0;
}

int64_t mac_sys_setsockopt(uint64_t a, uint64_t b, uint64_t c,
                                   uint64_t d, uint64_t e, uint64_t f) {
    (void)e;(void)f;
    if (setsockopt((int)a, (int)b, (int)c,
                   (const void*)(uintptr_t)d, (socklen_t)(int64_t)e) < 0)
        return mac_errno(errno);
    return 0;
}

int64_t mac_sys_getsockopt(uint64_t a, uint64_t b, uint64_t c,
                                   uint64_t d, uint64_t e, uint64_t f) {
    (void)e;(void)f;
    socklen_t optlen = sizeof(int);
    if (getsockopt((int)a, (int)b, (int)c,
                   (void*)(uintptr_t)d, &optlen) < 0)
        return mac_errno(errno);
    return 0;
}

int64_t mac_sys_getsockname(uint64_t a, uint64_t b, uint64_t c,
                                    uint64_t d, uint64_t e, uint64_t f) {
    (void)d;(void)e;(void)f;
    socklen_t addrlen = sizeof(struct sockaddr_storage);
    if (getsockname((int)a, (struct sockaddr*)(uintptr_t)b, &addrlen) < 0)
        return mac_errno(errno);
    if (c) *(socklen_t*)(uintptr_t)c = addrlen;
    return 0;
}

int64_t mac_sys_getpeername(uint64_t a, uint64_t b, uint64_t c,
                                    uint64_t d, uint64_t e, uint64_t f) {
    (void)d;(void)e;(void)f;
    socklen_t addrlen = sizeof(struct sockaddr_storage);
    if (getpeername((int)a, (struct sockaddr*)(uintptr_t)b, &addrlen) < 0)
        return mac_errno(errno);
    if (c) *(socklen_t*)(uintptr_t)c = addrlen;
    return 0;
}

int64_t mac_sys_sendto(uint64_t a, uint64_t b, uint64_t c,
                               uint64_t d, uint64_t e, uint64_t f) {
    ssize_t r = sendto((int)a, (const void*)(uintptr_t)b, (size_t)c,
                       (int)(uint64_t)d, (const struct sockaddr*)(uintptr_t)e,
                       (socklen_t)(int64_t)f);
    if (r < 0) return mac_errno(errno);
    return (int64_t)r;
}

int64_t mac_sys_recvfrom(uint64_t a, uint64_t b, uint64_t c,
                                 uint64_t d, uint64_t e, uint64_t f) {
    socklen_t addrlen = sizeof(struct sockaddr_storage);
    ssize_t r = recvfrom((int)a, (void*)(uintptr_t)b, (size_t)c,
                         (int)(uint64_t)d,
                         (struct sockaddr*)(uintptr_t)e, &addrlen);
    if (r < 0) return mac_errno(errno);
    return (int64_t)r;
}

int64_t mac_sys_shutdown(uint64_t a, uint64_t b, uint64_t c,
                                 uint64_t d, uint64_t e, uint64_t f) {
    (void)c;(void)d;(void)e;(void)f;
    if (shutdown((int)a, (int)b) < 0) return mac_errno(errno);
    return 0;
}

int64_t mac_sys_socketpair(uint64_t a, uint64_t b, uint64_t c,
                                   uint64_t d, uint64_t e, uint64_t f) {
    (void)e;(void)f;
    int sv[2];
    if (socketpair((int)a, (int)b, (int)c, sv) < 0)
        return mac_errno(errno);
    if (d) { ((int*)(uintptr_t)d)[0] = sv[0]; ((int*)(uintptr_t)d)[1] = sv[1]; }
    return 0;
}

int64_t mac_sys_getgroups(uint64_t a, uint64_t b, uint64_t c,
                                  uint64_t d, uint64_t e, uint64_t f) {
    (void)c;(void)d;(void)e;(void)f;
    /* getgroups on macOS: getgrouplist or syscall */
    int ngroups = (int)a;
    gid_t *grouplist = (gid_t*)(uintptr_t)b;
    if (!grouplist) return (int64_t)getgroups(0, NULL);
    int r = (int)getgroups(ngroups, grouplist);
    if (r < 0) return mac_errno(errno);
    return (int64_t)r;
}

int64_t mac_sys_setgroups(uint64_t a, uint64_t b, uint64_t c,
                                  uint64_t d, uint64_t e, uint64_t f) {
    (void)c;(void)d;(void)e;(void)f;
    if (setgroups((int)a, (const gid_t*)(uintptr_t)b) < 0)
        return mac_errno(errno);
    return 0;
}

int64_t mac_sys_sysctl(uint64_t a, uint64_t b, uint64_t c,
                               uint64_t d, uint64_t e, uint64_t f) {
    (void)a;(void)b;(void)c;(void)d;(void)e;(void)f;
    /* sysctl is deprecated on Linux; stub for now */
    return mac_errno(ENOSYS);
}

/* -- Additional BSD syscall handlers ------------------------------ */

/* Register Mach-O loader callback so wubu_exec_macho can use VSL.
 * Uses weak references to avoid link failures when VSL process module
 * or exec layer aren't linked in. */
__attribute__((weak))
int vsl_create_process_macho(const void *macho_data, size_t macho_size);

__attribute__((weak))
void wubu_exec_register_macho_loader(int (*loader)(const void*, size_t));

__attribute__((constructor))
static void mac_register_macho_loader(void) {
    if (wubu_exec_register_macho_loader && vsl_create_process_macho) {
        wubu_exec_register_macho_loader(vsl_create_process_macho);
    }
}

int64_t mac_sys_issetugid(uint64_t a, uint64_t b, uint64_t c,
                                  uint64_t d, uint64_t e, uint64_t f) {
    (void)a;(void)b;(void)c;(void)d;(void)e;(void)f;
    /* Return 0 (not set-uid) for VSL processes */
    return 0;
}

int64_t mac_sys_cs_ops(uint64_t a, uint64_t b, uint64_t c,
                               uint64_t d, uint64_t e, uint64_t f) {
    (void)a;(void)b;(void)c;(void)d;(void)e;(void)f;
    /* Code signing: return success with unsigned identity */
    return 0;
}

int64_t mac_sys_proc_info(uint64_t a, uint64_t b, uint64_t c,
                                  uint64_t d, uint64_t e, uint64_t f) {
    (void)b;(void)c;(void)d;(void)e;(void)f;
    /* proc_info(callnum, pid, flags, buffer, buffersize) */
    int32_t callnum = (int32_t)(uint64_t)a;
    int32_t pid = (int32_t)(uint64_t)b;
    uint64_t flags = c;
    void *buffer = (void*)(uintptr_t)d;
    size_t bufsize = (size_t)e;

    (void)flags;
    (void)buffer;
    (void)bufsize;

    if (pid < 0) pid = getpid();
    if (callnum == 2) {
        /* PROC_INFO_CALL_PIDINFO — return basic info */
        return 0; /* minimal stub — return empty */
    }
    if (callnum == 3) {
        /* PROC_INFO_CALL_PIDTASKINFO */
        return 0;
    }
    if (callnum == 5) {
        /* PROC_INFO_CALL_LISTPIDS */
        return 0;
    }
    return mac_errno(ENOSYS);
}

int64_t mac_sys_fsgetpath(uint64_t a, uint64_t b, uint64_t c,
                                  uint64_t d, uint64_t e, uint64_t f) {
    (void)d;(void)e;(void)f;
    /* fsgetpath(buffer, bufsize, fd) */
    char proc_path[256];
    snprintf(proc_path, sizeof(proc_path), "/proc/self/fd/%d", (int)(uint64_t)c);
    ssize_t len = readlink(proc_path, (char*)(uintptr_t)a, (size_t)b);
    if (len < 0) return mac_errno(errno);
    return (int64_t)len;
}

int64_t mac_sys_memorystatus(uint64_t a, uint64_t b, uint64_t c,
                                     uint64_t d, uint64_t e, uint64_t f) {
    (void)a;(void)b;(void)c;(void)d;(void)e;(void)f;
    /* memorystatus_control — stub */
    return 0;
}

int64_t mac_sys_terminate_process(uint64_t a, uint64_t b, uint64_t c,
                                          uint64_t d, uint64_t e, uint64_t f) {
    (void)b;(void)c;(void)d;(void)e;(void)f;
    /* terminate_with_payload(pid, signum, ...) */
    int pid = (int)(uint64_t)a;
    int sig = (int)(uint64_t)b;
    if (pid > 0 && sig > 0) {
        kill(pid, sig);
    }
    return 0;
}

int64_t mac_sys_getattrlist(uint64_t a, uint64_t b, uint64_t c,
                                    uint64_t d, uint64_t e, uint64_t f) {
    (void)a;(void)b;(void)c;(void)d;(void)e;(void)f;
    /* getattrlist(path, attrlist, buf, bufsize, options) — stub */
    return mac_errno(ENOSYS);
}

int64_t mac_sys_setattrlist(uint64_t a, uint64_t b, uint64_t c,
                                    uint64_t d, uint64_t e, uint64_t f) {
    (void)a;(void)b;(void)c;(void)d;(void)e;(void)f;
    return mac_errno(ENOSYS);
}

int64_t mac_sys_kqueue(uint64_t a, uint64_t b, uint64_t c,
                               uint64_t d, uint64_t e, uint64_t f) {
    (void)a;(void)b;(void)c;(void)d;(void)e;(void)f;
    /* kqueue not available on Linux; stub with eventfd */
    int fd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    if (fd < 0) return mac_errno(errno);
    return (int64_t)fd;
}

int64_t mac_sys_kevent_id(uint64_t a, uint64_t b, uint64_t c,
                                  uint64_t d, uint64_t e, uint64_t f) {
    (void)a;(void)b;(void)c;(void)d;(void)e;(void)f;
    /* kevent_id: kevent(kq, changelist, nchanges, eventlist, nevents, timeout) */
    return mac_errno(ENOSYS);
}

int64_t mac_sys_kevent64(uint64_t a, uint64_t b, uint64_t c,
                                 uint64_t d, uint64_t e, uint64_t f) {
    (void)a;(void)b;(void)c;(void)d;(void)e;(void)f;
    return mac_errno(ENOSYS);
}

int64_t mac_sys_workq_open(uint64_t a, uint64_t b, uint64_t c,
                                   uint64_t d, uint64_t e, uint64_t f) {
    (void)a;(void)b;(void)c;(void)d;(void)e;(void)f;
    /* workq_open — returns a work queue port (stub) */
    return 0;
}

int64_t mac_sys_workq_kern_return(uint64_t a, uint64_t b, uint64_t c,
                                          uint64_t d, uint64_t e, uint64_t f) {
    (void)a;(void)b;(void)c;(void)d;(void)e;(void)f;
    return 0;
}

int64_t mac_sys_psynch_mutexwait(uint64_t a, uint64_t b, uint64_t c,
                                         uint64_t d, uint64_t e, uint64_t f) {
    (void)a;(void)b;(void)c;(void)d;(void)e;(void)f;
    /* pthread mutex wait via futex */
    uint32_t *mutex = (uint32_t*)(uintptr_t)a;
    uint32_t val = (uint32_t)(uint64_t)b;
    if (mutex) {
        /* Simple spin for now — return when mutex available */
        while (__sync_val_compare_and_swap(mutex, val, 0) != val) {
            /* spin — 'pause' instruction via builtin */
            __builtin_ia32_pause();
        }
    }
    return 0;
}

int64_t mac_sys_psynch_mutexdrop(uint64_t a, uint64_t b, uint64_t c,
                                         uint64_t d, uint64_t e, uint64_t f) {
    (void)b;(void)c;(void)d;(void)e;(void)f;
    uint32_t *mutex = (uint32_t*)(uintptr_t)a;
    if (mutex) {
        __sync_lock_release(mutex);
    }
    return 0;
}

int64_t mac_sys_psynch_cvbroad(uint64_t a, uint64_t b, uint64_t c,
                                       uint64_t d, uint64_t e, uint64_t f) {
    (void)a;(void)b;(void)c;(void)d;(void)e;(void)f;
    return 0;
}

int64_t mac_sys_psynch_cvsignal(uint64_t a, uint64_t b, uint64_t c,
                                        uint64_t d, uint64_t e, uint64_t f) {
    (void)a;(void)b;(void)c;(void)d;(void)e;(void)f;
    return 0;
}

int64_t mac_sys_psynch_cvwait(uint64_t a, uint64_t b, uint64_t c,
                                      uint64_t d, uint64_t e, uint64_t f) {
    (void)a;(void)b;(void)c;(void)d;(void)e;(void)f;
    return 0;
}

int64_t mac_sys_shm_open(uint64_t a, uint64_t b, uint64_t c,
                                 uint64_t d, uint64_t e, uint64_t f) {
    (void)d;(void)e;(void)f;
    /* shm_open(name, oflag, mode) */
    int fd = shm_open((const char*)(uintptr_t)a, (int)b, (mode_t)c);
    if (fd < 0) return mac_errno(errno);
    return (int64_t)fd;
}

int64_t mac_sys_shm_unlink(uint64_t a, uint64_t b, uint64_t c,
                                   uint64_t d, uint64_t e, uint64_t f) {
    (void)b;(void)c;(void)d;(void)e;(void)f;
    if (shm_unlink((const char*)(uintptr_t)a) < 0) return mac_errno(errno);
    return 0;
}

int64_t mac_sys_pid_suspend(uint64_t a, uint64_t b, uint64_t c,
                                    uint64_t d, uint64_t e, uint64_t f) {
    (void)b;(void)c;(void)d;(void)e;(void)f;
    int pid = (int)(uint64_t)a;
    kill(pid, SIGSTOP);
    return 0;
}

int64_t mac_sys_pid_resume(uint64_t a, uint64_t b, uint64_t c,
                                   uint64_t d, uint64_t e, uint64_t f) {
    (void)b;(void)c;(void)d;(void)e;(void)f;
    int pid = (int)(uint64_t)a;
    kill(pid, SIGCONT);
    return 0;
}

/* -- BSD syscall dispatch table ----------------------------------- */
const mac_syscall_fn mac_bsd_table[MAC_SYS_MAX] = {
    [MAC_SYS_EXIT]       = mac_sys_exit,
    [MAC_SYS_FORK]       = mac_sys_fork,
    [MAC_SYS_READ]       = mac_sys_read,
    [MAC_SYS_WRITE]      = mac_sys_write,
    [MAC_SYS_OPEN]       = mac_sys_open,
    [MAC_SYS_CLOSE]      = mac_sys_close,
    [MAC_SYS_WAIT4]      = mac_sys_wait4,
    [MAC_SYS_LINK]       = mac_sys_link,
    [MAC_SYS_UNLINK]     = mac_sys_unlink,
    [MAC_SYS_EXECVE]     = mac_sys_execve,
    [MAC_SYS_CHDIR]      = mac_sys_chdir,
    [MAC_SYS_MKDIR]      = mac_sys_mkdir,
    [MAC_SYS_RMDIR]      = mac_sys_rmdir,
    [MAC_SYS_ACCESS]     = mac_sys_access,
    [MAC_SYS_GETPID]     = mac_sys_getpid,
    [MAC_SYS_GETPPID]    = mac_sys_getppid,
    [MAC_SYS_GETUID]     = mac_sys_getuid,
    [MAC_SYS_GETGID]     = mac_sys_getgid,
    [MAC_SYS_GETEUID]    = mac_sys_geteuid,
    [MAC_SYS_GETEGID]    = mac_sys_getegid,
    [MAC_SYS_KILL]       = mac_sys_kill,
    [MAC_SYS_DUP]        = mac_sys_dup,
    [MAC_SYS_PIPE]       = mac_sys_pipe,
    [MAC_SYS_FCNTL]      = mac_sys_fcntl,
    [MAC_SYS_IOCTL]      = mac_sys_ioctl,
    [MAC_SYS_GETTIMEOFDAY] = mac_sys_gettimeofday,
    [MAC_SYS_SELECT]     = mac_sys_select,
    [MAC_SYS_MMAP]       = mac_sys_mmap,
    [MAC_SYS_MUNMAP]     = mac_sys_munmap,
    [MAC_SYS_MPROTECT]   = mac_sys_mprotect,
    [MAC_SYS_MADVISE]    = mac_sys_madvise,
    [MAC_SYS_SOCKET]     = mac_sys_socket,
    [MAC_SYS_CONNECT]    = mac_sys_connect,
    [MAC_SYS_ACCEPT]     = mac_sys_accept,
    [MAC_SYS_BIND]       = mac_sys_bind,
    [MAC_SYS_LISTEN]     = mac_sys_listen,
    [MAC_SYS_SETSOCKOPT] = mac_sys_setsockopt,
    [MAC_SYS_GETSOCKOPT] = mac_sys_getsockopt,
    [MAC_SYS_GETSOCKNAME] = mac_sys_getsockname,
    [MAC_SYS_GETPEERNAME] = mac_sys_getpeername,
    [MAC_SYS_SENDTO]     = mac_sys_sendto,
    [MAC_SYS_RECVFROM]   = mac_sys_recvfrom,
    [MAC_SYS_SHUTDOWN]   = mac_sys_shutdown,
    [MAC_SYS_SOCKETPAIR] = mac_sys_socketpair,
    [MAC_SYS_GETGROUPS]  = mac_sys_getgroups,
    [MAC_SYS_SETGROUPS]  = mac_sys_setgroups,
    [MAC_SYS_SIGACTION]  = mac_sys_sigaction,
    [MAC_SYS_SIGPROCMASK] = mac_sys_sigprocmask,
    [MAC_SYS_STAT]       = mac_sys_stat,
    [MAC_SYS_FSTAT]      = mac_sys_fstat,
    [MAC_SYS_LSTAT]      = mac_sys_lstat,
    [MAC_SYS_LSEEK]      = mac_sys_lseek,
    [MAC_SYS_GETCWD]     = mac_sys_getcwd,
    [MAC_SYS_READLINK]   = mac_sys_readlink,
    [MAC_SYS_SYMLINK]    = mac_sys_symlink,
    [MAC_SYS_RENAME]     = mac_sys_rename,
    [MAC_SYS_FSYNC]      = mac_sys_fsync,
    [MAC_SYS_FTRUNCATE]  = mac_sys_ftruncate,
    [MAC_SYS_TRUNCATE]   = mac_sys_truncate,
    [MAC_SYS_READV]      = mac_sys_readv,
    [MAC_SYS_WRITEV]     = mac_sys_writev,
    [MAC_SYS_SYSCTL]     = mac_sys_sysctl,
    [MAC_SYS_ISSETUGID]  = mac_sys_issetugid,
    [MAC_SYS_CS_OPS]     = mac_sys_cs_ops,
    [MAC_SYS_PROC_INFO]  = mac_sys_proc_info,
    [MAC_SYS_FSGETPATH]  = mac_sys_fsgetpath,
    [MAC_SYS_MEMORYSTATUS] = mac_sys_memorystatus,
    [MAC_SYS_TERMINATE_PROCESS] = mac_sys_terminate_process,
    [MAC_SYS_GETATTRLIST] = mac_sys_getattrlist,
    [MAC_SYS_SETATTRLIST] = mac_sys_setattrlist,
    [MAC_SYS_KQUEUE]     = mac_sys_kqueue,
    [MAC_SYS_KEVENT_ID]  = mac_sys_kevent_id,
    [MAC_SYS_KEVENT64]   = mac_sys_kevent64,
    [MAC_SYS_WORKQ_OPEN] = mac_sys_workq_open,
    [MAC_SYS_WORKQ_KERN_RETURN] = mac_sys_workq_kern_return,
    [MAC_SYS_PSYNCH_MUTEXWAIT] = mac_sys_psynch_mutexwait,
    [MAC_SYS_PSYNCH_MUTEXDROP] = mac_sys_psynch_mutexdrop,
    [MAC_SYS_PSYNCH_CVBROAD]   = mac_sys_psynch_cvbroad,
    [MAC_SYS_PSYNCH_CVSIGNAL]  = mac_sys_psynch_cvsignal,
    [MAC_SYS_PSYNCH_CVWAIT]    = mac_sys_psynch_cvwait,
    [MAC_SYS_SHM_OPEN]   = mac_sys_shm_open,
    [MAC_SYS_SHM_UNLINK] = mac_sys_shm_unlink,
    [MAC_SYS_PID_SUSPEND] = mac_sys_pid_suspend,
    [MAC_SYS_PID_RESUME]  = mac_sys_pid_resume,
};
