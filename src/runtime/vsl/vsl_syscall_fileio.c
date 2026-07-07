/*
 * vsl_syscall_fileio.c  --  VSL File I/O Syscalls
 * Read, write, open, close, lseek, stat, fstat, etc.
 */

#include "vsl_syscall_internal.h"

/* ====================================================================
 * FILE I/O SYSCALLS
 * ==================================================================== */

int64_t vsl_sys_read(uint64_t fd, uint64_t buf, uint64_t count,
                      uint64_t d, uint64_t e, uint64_t f) {
    (void)d; (void)e; (void)f;
    ssize_t result = read((int)fd, (void *)buf, (size_t)count);
    return result < 0 ? -errno : (int64_t)result;
}

int64_t vsl_sys_write(uint64_t fd, uint64_t buf, uint64_t count,
                       uint64_t d, uint64_t e, uint64_t f) {
    (void)d; (void)e; (void)f;
    ssize_t result = write((int)fd, (const void *)buf, (size_t)count);
    return result < 0 ? -errno : (int64_t)result;
}

int64_t vsl_sys_open(uint64_t path, uint64_t flags, uint64_t mode,
                      uint64_t d, uint64_t e, uint64_t f) {
    (void)d; (void)e; (void)f;
    return vsl_open((const char *)path, (int)flags, (int)mode);
}

int64_t vsl_sys_close(uint64_t fd, uint64_t b, uint64_t c,
                       uint64_t d, uint64_t e, uint64_t f) {
    (void)b; (void)c; (void)d; (void)e; (void)f;
    return vsl_close((int)fd);
}

int64_t vsl_sys_lseek(uint64_t fd, uint64_t offset, uint64_t whence,
                       uint64_t d, uint64_t e, uint64_t f) {
    (void)d; (void)e; (void)f;
    return vsl_lseek((int)fd, (int64_t)offset, (int)whence);
}

int64_t vsl_sys_stat(uint64_t path, uint64_t buf, uint64_t c,
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

int64_t vsl_sys_fstat(uint64_t fd, uint64_t buf, uint64_t c,
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

int64_t vsl_sys_ioctl(uint64_t fd, uint64_t req, uint64_t arg,
                       uint64_t d, uint64_t e, uint64_t f) {
    (void)d; (void)e; (void)f;
    int result = ioctl((int)fd, (unsigned long)req, (void *)arg);
    return result < 0 ? -errno : (int64_t)result;
}

int64_t vsl_sys_access(uint64_t path, uint64_t mode, uint64_t c,
                        uint64_t d, uint64_t e, uint64_t f) {
    (void)c; (void)d; (void)e; (void)f;
    int rc = access((const char *)path, (int)mode);
    return rc < 0 ? -errno : (int64_t)rc;
}

int64_t vsl_sys_pipe(uint64_t pipefd, uint64_t b, uint64_t c,
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

int64_t vsl_sys_dup(uint64_t fd, uint64_t b, uint64_t c,
                     uint64_t d, uint64_t e, uint64_t f) {
    (void)b; (void)c; (void)d; (void)e; (void)f;
    int host_fd = vsl_get_host_fd((int)fd);
    if (host_fd < 0) return -9;
    int result = dup(host_fd);
    return result < 0 ? -errno : (int64_t)result;
}

int64_t vsl_sys_dup2(uint64_t oldfd, uint64_t newfd, uint64_t c,
                      uint64_t d, uint64_t e, uint64_t f) {
    (void)c; (void)d; (void)e; (void)f;
    int host_oldfd = vsl_get_host_fd((int)oldfd);
    int host_newfd = vsl_get_host_fd((int)newfd);
    if (host_oldfd < 0 || host_newfd < 0) return -9;
    int result = dup2(host_oldfd, host_newfd);
    return result < 0 ? -errno : (int64_t)result;
}

int64_t vsl_sys_fcntl(uint64_t fd, uint64_t cmd, uint64_t arg,
                       uint64_t d, uint64_t e, uint64_t f) {
    (void)d; (void)e; (void)f;
    int host_fd = vsl_get_host_fd((int)fd);
    if (host_fd < 0) return -9;
    int rc = fcntl(host_fd, (int)cmd, (int)arg);
    return rc < 0 ? -errno : (int64_t)rc;
}

int64_t vsl_sys_fsync(uint64_t fd, uint64_t b, uint64_t c,
                       uint64_t d, uint64_t e, uint64_t f) {
    (void)b; (void)c; (void)d; (void)e; (void)f;
    int host_fd = vsl_get_host_fd((int)fd);
    if (host_fd < 0) return -9;
    int rc = fsync(host_fd);
    return rc < 0 ? -errno : (int64_t)rc;
}

int64_t vsl_sys_unlink(uint64_t path, uint64_t b, uint64_t c,
                        uint64_t d, uint64_t e, uint64_t f) {
    (void)b; (void)c; (void)d; (void)e; (void)f;
    int rc = unlink((const char *)path);
    return rc < 0 ? -errno : (int64_t)rc;
}

int64_t vsl_sys_mkdir(uint64_t path, uint64_t mode, uint64_t c,
                       uint64_t d, uint64_t e, uint64_t f) {
    (void)c; (void)d; (void)e; (void)f;
    int rc = mkdir((const char *)path, (mode_t)mode);
    return rc < 0 ? -errno : (int64_t)rc;
}

int64_t vsl_sys_rmdir(uint64_t path, uint64_t b, uint64_t c,
                       uint64_t d, uint64_t e, uint64_t f) {
    (void)b; (void)c; (void)d; (void)e; (void)f;
    int rc = rmdir((const char *)path);
    return rc < 0 ? -errno : (int64_t)rc;
}

int64_t vsl_sys_rename(uint64_t oldpath, uint64_t newpath, uint64_t c,
                        uint64_t d, uint64_t e, uint64_t f) {
    (void)c; (void)d; (void)e; (void)f;
    int rc = rename((const char *)oldpath, (const char *)newpath);
    return rc < 0 ? -errno : (int64_t)rc;
}

int64_t vsl_sys_getcwd(uint64_t buf, uint64_t size, uint64_t c,
                        uint64_t d, uint64_t e, uint64_t f) {
    (void)c; (void)d; (void)e; (void)f;
    char *result = getcwd((char *)buf, (size_t)size);
    return result ? (int64_t)buf : -errno;
}

int64_t vsl_sys_chdir(uint64_t path, uint64_t b, uint64_t c,
                       uint64_t d, uint64_t e, uint64_t f) {
    (void)b; (void)c; (void)d; (void)e; (void)f;
    int rc = chdir((const char *)path);
    return rc < 0 ? -errno : (int64_t)rc;
}

int64_t vsl_sys_clock_gettime(uint64_t clk_id, uint64_t tp, uint64_t c,
                               uint64_t d, uint64_t e, uint64_t f) {
    (void)c; (void)d; (void)e; (void)f;
    struct timespec ts;
    int rc = clock_gettime((clockid_t)clk_id, &ts);
    if (rc < 0) return -errno;
    if (tp) memcpy((void *)tp, &ts, sizeof(struct timespec));
    return 0;
}

/* ====================================================================
 * ADVANCED FILE I/O SYSCALLS (Cell 360-370)
 * ==================================================================== */

int64_t vsl_sys_pread64(uint64_t fd, uint64_t buf, uint64_t count,
                         uint64_t pos_low, uint64_t pos_high, uint64_t f) {
    (void)f;
    int64_t offset = ((int64_t)pos_high << 32) | (int64_t)pos_low;
    ssize_t result = pread64((int)fd, (void *)buf, (size_t)count, offset);
    return result < 0 ? -errno : (int64_t)result;
}

int64_t vsl_sys_pwrite64(uint64_t fd, uint64_t buf, uint64_t count,
                          uint64_t pos_low, uint64_t pos_high, uint64_t f) {
    (void)f;
    int64_t offset = ((int64_t)pos_high << 32) | (int64_t)pos_low;
    ssize_t result = pwrite64((int)fd, (const void *)buf, (size_t)count, offset);
    return result < 0 ? -errno : (int64_t)result;
}

int64_t vsl_sys_readv(uint64_t fd, uint64_t iov, uint64_t iovcnt,
                       uint64_t d, uint64_t e, uint64_t f) {
    (void)d; (void)e; (void)f;
    struct iovec *vec = (struct iovec *)iov;
    ssize_t result = readv((int)fd, vec, (int)iovcnt);
    return result < 0 ? -errno : (int64_t)result;
}

int64_t vsl_sys_writev(uint64_t fd, uint64_t iov, uint64_t iovcnt,
                       uint64_t d, uint64_t e, uint64_t f) {
    (void)d; (void)e; (void)f;
    const struct iovec *vec = (const struct iovec *)iov;
    ssize_t result = writev((int)fd, vec, (int)iovcnt);
    return result < 0 ? -errno : (int64_t)result;
}

int64_t vsl_sys_truncate(uint64_t path, uint64_t length,
                          uint64_t c, uint64_t d, uint64_t e, uint64_t f) {
    (void)c; (void)d; (void)e; (void)f;
    int result = truncate((const char *)path, (off_t)length);
    return result < 0 ? -errno : (int64_t)result;
}

int64_t vsl_sys_ftruncate(uint64_t fd, uint64_t length,
                           uint64_t c, uint64_t d, uint64_t e, uint64_t f) {
    (void)c; (void)d; (void)e; (void)f;
    int result = ftruncate((int)fd, (off_t)length);
    return result < 0 ? -errno : (int64_t)result;
}

int64_t vsl_sys_creat(uint64_t pathname, uint64_t mode,
                      uint64_t c, uint64_t d, uint64_t e, uint64_t f) {
    (void)c; (void)d; (void)e; (void)f;
    int fd = open((const char *)pathname, O_CREAT | O_WRONLY | O_TRUNC, (mode_t)mode);
    return fd < 0 ? -errno : (int64_t)fd;
}

int64_t vsl_sys_symlink(uint64_t target, uint64_t linkpath,
                         uint64_t c, uint64_t d, uint64_t e, uint64_t f) {
    (void)c; (void)d; (void)e; (void)f;
    int result = symlink((const char *)target, (const char *)linkpath);
    return result < 0 ? -errno : (int64_t)result;
}

int64_t vsl_sys_readlink(uint64_t path, uint64_t buf, uint64_t bufsiz,
                          uint64_t d, uint64_t e, uint64_t f) {
    (void)d; (void)e; (void)f;
    ssize_t result = readlink((const char *)path, (char *)buf, (size_t)bufsiz);
    return result < 0 ? -errno : (int64_t)result;
}

int64_t vsl_sys_chmod(uint64_t path, uint64_t mode,
                       uint64_t c, uint64_t d, uint64_t e, uint64_t f) {
    (void)c; (void)d; (void)e; (void)f;
    int result = chmod((const char *)path, (mode_t)mode);
    return result < 0 ? -errno : (int64_t)result;
}

int64_t vsl_sys_fchmod(uint64_t fd, uint64_t mode,
                        uint64_t c, uint64_t d, uint64_t e, uint64_t f) {
    (void)c; (void)d; (void)e; (void)f;
    int result = fchmod((int)fd, (mode_t)mode);
    return result < 0 ? -errno : (int64_t)result;
}

int64_t vsl_sys_chown(uint64_t path, uint64_t owner, uint64_t group,
                       uint64_t d, uint64_t e, uint64_t f) {
    (void)d; (void)e; (void)f;
    int result = chown((const char *)path, (uid_t)owner, (gid_t)group);
    return result < 0 ? -errno : (int64_t)result;
}

int64_t vsl_sys_lchown(uint64_t path, uint64_t owner, uint64_t group,
                        uint64_t d, uint64_t e, uint64_t f) {
    (void)d; (void)e; (void)f;
    int result = lchown((const char *)path, (uid_t)owner, (gid_t)group);
    return result < 0 ? -errno : (int64_t)result;
}

int64_t vsl_sys_fchown(uint64_t fd, uint64_t owner, uint64_t group,
                        uint64_t d, uint64_t e, uint64_t f) {
    (void)d; (void)e; (void)f;
    int result = fchown((int)fd, (uid_t)owner, (gid_t)group);
    return result < 0 ? -errno : (int64_t)result;
}

int64_t vsl_sys_lstat(uint64_t path, uint64_t buf, uint64_t c,
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

int64_t vsl_sys_openat(uint64_t dirfd, uint64_t pathname, uint64_t flags,
                        uint64_t mode, uint64_t e, uint64_t f) {
    (void)e; (void)f;
    return vsl_openat((int)dirfd, (const char *)pathname, (int)flags, (mode_t)mode);
}

int64_t vsl_sys_newfstatat(uint64_t dirfd, uint64_t pathname, uint64_t buf,
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

int64_t vsl_sys_unlinkat(uint64_t dirfd, uint64_t pathname, uint64_t flags,
                          uint64_t d, uint64_t e, uint64_t f) {
    (void)d; (void)e; (void)f;
    int result = unlinkat((int)dirfd, (const char *)pathname, (int)flags);
    return result < 0 ? -errno : (int64_t)result;
}

int64_t vsl_sys_faccessat(uint64_t dirfd, uint64_t pathname, uint64_t mode,
                           uint64_t flags, uint64_t e, uint64_t f) {
    (void)e; (void)f;
    int result = faccessat((int)dirfd, (const char *)pathname, (int)mode, (int)flags);
    return result < 0 ? -errno : (int64_t)result;
}

int64_t vsl_sys_getdents64(uint64_t fd, uint64_t dirp, uint64_t count,
                            uint64_t d, uint64_t e, uint64_t f) {
    (void)d; (void)e; (void)f;
    int host_fd = vsl_get_host_fd((int)fd);
    if (host_fd < 0) return -9;
    ssize_t result = syscall(SYS_getdents64, host_fd, (void *)dirp, (size_t)count);
    return result < 0 ? -errno : (int64_t)result;
}

int64_t vsl_sys_statx(uint64_t dirfd, uint64_t pathname, uint64_t flags,
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

/* ====================================================================
 * POLL / EPOLL SYSCALLS
 * ==================================================================== */

int64_t vsl_sys_poll(uint64_t ufds, uint64_t nfds, uint64_t timeout,
                      uint64_t d, uint64_t e, uint64_t f) {
    (void)d; (void)e; (void)f;
    struct pollfd *fds = (struct pollfd *)ufds;
    int result = poll(fds, (nfds_t)nfds, (int)timeout);
    return result < 0 ? -errno : (int64_t)result;
}

int64_t vsl_sys_epoll_create(uint64_t size, uint64_t b, uint64_t c,
                              uint64_t d, uint64_t e, uint64_t f) {
    (void)b; (void)c; (void)d; (void)e; (void)f;
    int result = epoll_create1(0);
    return result < 0 ? -errno : (int64_t)result;
}

int64_t vsl_sys_epoll_ctl(uint64_t epfd, uint64_t op, uint64_t fd,
                           uint64_t event, uint64_t e, uint64_t f) {
    (void)e; (void)f;
    struct epoll_event ev;
    memcpy(&ev, (void *)event, sizeof(struct epoll_event));
    int result = epoll_ctl((int)epfd, (int)op, (int)fd, &ev);
    return result < 0 ? -errno : (int64_t)result;
}

int64_t vsl_sys_epoll_wait(uint64_t epfd, uint64_t events, uint64_t maxevents,
                            uint64_t timeout, uint64_t e, uint64_t f) {
    (void)e; (void)f;
    struct epoll_event *ev = (struct epoll_event *)events;
    int result = epoll_wait((int)epfd, ev, (int)maxevents, (int)timeout);
    return result < 0 ? -errno : (int64_t)result;
}