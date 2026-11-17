/*
 * wubu_posix -- our own POSIX surface, issued as raw Linux syscalls.
 *
 * Freestanding: no libc header is included.  On x86-64 the trap is inline asm,
 * so the syscall numbers are ours and visible; other targets fall back to
 * syscall().  Proof of independence:
 *   gcc -std=c11 -nostdinc -Iinclude -c src/runtime/wubu_posix.c
 */
#include "wubu_posix.h"

#if defined(__x86_64__)
/* Syscall numbers, declared by us (x86-64 ABI). */
#define SC_READ          0
#define SC_WRITE         1
#define SC_OPEN          2
#define SC_CLOSE         3
#define SC_FSTAT         5
#define SC_LSTAT         6
#define SC_MMAP          9
#define SC_MPROTECT     10
#define SC_MUNMAP       11
#define SC_PIPE         22
#define SC_PREAD        17
#define SC_PWRITE       18
#define SC_DUP          32
#define SC_DUP2         33
#define SC_NANOSLEEP    35
#define SC_GETPID       39
#define SC_LINK         86
#define SC_UNLINK       87
#define SC_SYMLINK      88
#define SC_READLINK     89
#define SC_CHMOD        90
#define SC_CHOWN        92
#define SC_UMASK        95
#define SC_RENAME       82
#define SC_MKDIR        83
#define SC_RMDIR        84
#define SC_TRUNCATE     76
#define SC_FTRUNCATE    77
#define SC_FSYNC        74
#define SC_FDATASYNC    75
#define SC_LSEEK        8
#define SC_ACCESS       21
#define SC_GETUID      102
#define SC_GETGID      104
#define SC_GETEUID     107
#define SC_GETEGID     108
#define SC_GETPPID     110
#define SC_CLOCK_GETTIME 228
#define SC_CLOCK_NANOSLEEP 230
#define SC_GETDENTS64  217

long wubu_syscall6(long nr, long a, long b, long c,
                   long d, long e, long f)
{
    long ret;
    register long r10 __asm__("r10") = d;
    register long r8  __asm__("r8")  = e;
    register long r9  __asm__("r9")  = f;
    __asm__ volatile ("syscall"
                      : "=a"(ret)
                      : "a"(nr), "D"(a), "S"(b), "d"(c),
                        "r"(r10), "r"(r8), "r"(r9)
                      : "rcx", "r11", "memory");
    return ret;
}
#else
#include <sys/syscall.h>
#include <unistd.h>
/* non-x86-64 target: documented syscall() fallback */
long wubu_syscall6(long nr, long a, long b, long c,
                   long d, long e, long f)
{
    return syscall(nr, a, b, c, d, e, f);
}
#endif

/* --------------------------------------------------------------- files */

int wubu_open(const char *path, int flags, wubu_mode mode)
{
    return (int)wubu_syscall6(SC_OPEN, (long)path, flags, mode, 0, 0, 0);
}

int wubu_close(int fd)
{
    return (int)wubu_syscall6(SC_CLOSE, fd, 0, 0, 0, 0, 0);
}

long wubu_read(int fd, void *buf, size_t len)
{
    return wubu_syscall6(SC_READ, fd, (long)buf, (long)len, 0, 0, 0);
}

long wubu_write(int fd, const void *buf, size_t len)
{
    return wubu_syscall6(SC_WRITE, fd, (long)buf, (long)len, 0, 0, 0);
}

wubu_off wubu_lseek(int fd, wubu_off off, int whence)
{
    return (wubu_off)wubu_syscall6(SC_LSEEK, fd, off, whence, 0, 0, 0);
}

int wubu_pread(int fd, void *buf, size_t len, wubu_off off)
{
    return (int)wubu_syscall6(SC_PREAD, fd, (long)buf, (long)len, off, 0, 0);
}

int wubu_pwrite(int fd, const void *buf, size_t len, wubu_off off)
{
    return (int)wubu_syscall6(SC_PWRITE, fd, (long)buf, (long)len, off, 0, 0);
}

int wubu_fsync(int fd)
{
    return (int)wubu_syscall6(SC_FSYNC, fd, 0, 0, 0, 0, 0);
}

int wubu_fdatasync(int fd)
{
    return (int)wubu_syscall6(SC_FDATASYNC, fd, 0, 0, 0, 0, 0);
}

int wubu_ftruncate(int fd, wubu_off len)
{
    return (int)wubu_syscall6(SC_FTRUNCATE, fd, len, 0, 0, 0, 0);
}

int wubu_truncate(const char *path, wubu_off len)
{
    return (int)wubu_syscall6(SC_TRUNCATE, (long)path, len, 0, 0, 0, 0);
}

int wubu_fstat(int fd, struct wubu_stat *st)
{
    return (int)wubu_syscall6(SC_FSTAT, fd, (long)st, 0, 0, 0, 0);
}

int wubu_lstat_path(const char *path, struct wubu_stat *st)
{
    return (int)wubu_syscall6(SC_LSTAT, (long)path, (long)st, 0, 0, 0, 0);
}

int wubu_stat_path(const char *path, struct wubu_stat *st)
{
    /* stat follows symlinks; lstat does not.  Regular paths go through
     * open+fstat so the reported identity matches what was opened. */
    int fd = wubu_open(path, WUBU_O_RDONLY, 0);
    if (fd < 0) return wubu_lstat_path(path, st);
    int rc = wubu_fstat(fd, st);
    wubu_close(fd);
    return rc;
}

int wubu_access(const char *path, int mode)
{
    return (int)wubu_syscall6(SC_ACCESS, (long)path, mode, 0, 0, 0, 0);
}

int wubu_unlink(const char *path)
{
    return (int)wubu_syscall6(SC_UNLINK, (long)path, 0, 0, 0, 0, 0);
}

int wubu_rmdir(const char *path)
{
    return (int)wubu_syscall6(SC_RMDIR, (long)path, 0, 0, 0, 0, 0);
}

int wubu_mkdir(const char *path, wubu_mode mode)
{
    return (int)wubu_syscall6(SC_MKDIR, (long)path, mode, 0, 0, 0, 0);
}

int wubu_rename(const char *from, const char *to)
{
    return (int)wubu_syscall6(SC_RENAME, (long)from, (long)to, 0, 0, 0, 0);
}

int wubu_link(const char *from, const char *to)
{
    return (int)wubu_syscall6(SC_LINK, (long)from, (long)to, 0, 0, 0, 0);
}

int wubu_symlink(const char *target, const char *linkpath)
{
    return (int)wubu_syscall6(SC_SYMLINK, (long)target, (long)linkpath,
                              0, 0, 0, 0);
}

long wubu_readlink(const char *path, char *buf, size_t cap)
{
    return wubu_syscall6(SC_READLINK, (long)path, (long)buf, (long)cap,
                         0, 0, 0);
}

int wubu_chmod(const char *path, wubu_mode mode)
{
    return (int)wubu_syscall6(SC_CHMOD, (long)path, mode, 0, 0, 0, 0);
}

int wubu_chown(const char *path, wubu_uid uid, wubu_gid gid)
{
    return (int)wubu_syscall6(SC_CHOWN, (long)path, uid, gid, 0, 0, 0);
}

/* ---------------------------------------------------------- directories */

long wubu_getdents(int fd, void *buf, size_t len)
{
    return wubu_syscall6(SC_GETDENTS64, fd, (long)buf, (long)len, 0, 0, 0);
}

wubu_dir *wubu_opendir(const char *path)
{
    int fd = wubu_open(path, WUBU_O_RDONLY | WUBU_O_DIRECTORY, 0);
    if (fd < 0) return 0;
    wubu_dir *d = (wubu_dir *)wubu_mmap(0, sizeof *d,
                                       WUBU_PROT_READ | WUBU_PROT_WRITE,
                                       WUBU_MAP_PRIVATE | WUBU_MAP_ANONYMOUS,
                                       -1, 0);
    if (!d) { wubu_close(fd); return 0; }
    d->fd = fd; d->pos = 0; d->len = 0; d->done = 0;
    return d;
}

struct wubu_dirent *wubu_readdir(wubu_dir *d)
{
    if (!d || d->done) return 0;
    if (d->pos >= d->len) {
        long n = wubu_getdents(d->fd, d->buf, sizeof d->buf);
        if (n <= 0) { d->done = 1; return 0; }
        d->pos = 0;
        d->len = (int)n;
    }
    struct wubu_dirent *de = (struct wubu_dirent *)(d->buf + d->pos);
    d->pos += de->d_reclen;
    return de;
}

int wubu_closedir(wubu_dir *d)
{
    if (!d) return -1;
    int fd = d->fd;
    wubu_munmap(d, sizeof *d);
    return wubu_close(fd);
}

/* -------------------------------------------------------------- memory */

void *wubu_mmap(void *addr, size_t len, int prot, int flags, int fd,
                wubu_off off)
{
    long r = wubu_syscall6(SC_MMAP, (long)addr, (long)len, prot, flags, fd,
                           off);
    if (r < 0 && r > -4096) return 0;   /* MAP_FAILED */
    return (void *)r;
}

int wubu_munmap(void *addr, size_t len)
{
    return (int)wubu_syscall6(SC_MUNMAP, (long)addr, (long)len, 0, 0, 0, 0);
}

int wubu_mprotect(void *addr, size_t len, int prot)
{
    return (int)wubu_syscall6(SC_MPROTECT, (long)addr, (long)len, prot,
                              0, 0, 0);
}

/* ------------------------------------------------------------ processes */

int wubu_getpid(void)
{
    return (int)wubu_syscall6(SC_GETPID, 0, 0, 0, 0, 0, 0);
}

int wubu_getppid(void)
{
    return (int)wubu_syscall6(SC_GETPPID, 0, 0, 0, 0, 0, 0);
}

int wubu_getuid(void)
{
    return (int)wubu_syscall6(SC_GETUID, 0, 0, 0, 0, 0, 0);
}

int wubu_geteuid(void)
{
    return (int)wubu_syscall6(SC_GETEUID, 0, 0, 0, 0, 0, 0);
}

int wubu_getgid(void)
{
    return (int)wubu_syscall6(SC_GETGID, 0, 0, 0, 0, 0, 0);
}

int wubu_getegid(void)
{
    return (int)wubu_syscall6(SC_GETEGID, 0, 0, 0, 0, 0, 0);
}

int wubu_umask(int mask)
{
    return (int)wubu_syscall6(SC_UMASK, mask, 0, 0, 0, 0, 0);
}

int wubu_dup(int fd)
{
    return (int)wubu_syscall6(SC_DUP, fd, 0, 0, 0, 0, 0);
}

int wubu_dup2(int oldfd, int newfd)
{
    return (int)wubu_syscall6(SC_DUP2, oldfd, newfd, 0, 0, 0, 0);
}

int wubu_pipe(int fds[2])
{
    return (int)wubu_syscall6(SC_PIPE, (long)fds, 0, 0, 0, 0, 0);
}

void wubu_exit(int code)
{
    wubu_syscall6(60 /* exit */, code, 0, 0, 0, 0, 0);
    for (;;) { }
}

/* ---------------------------------------------------------------- time */

int wubu_clock_gettime(int clk, struct wubu_timespec *ts)
{
    return (int)wubu_syscall6(SC_CLOCK_GETTIME, clk, (long)ts, 0, 0, 0, 0);
}

int wubu_gettimeofday(struct wubu_timeval *tv)
{
    if (!tv) return -1;
    struct wubu_timespec ts;
    int rc = wubu_clock_gettime(WUBU_CLOCK_REALTIME, &ts);
    if (rc != 0) return rc;
    tv->tv_sec  = ts.tv_sec;
    tv->tv_usec = ts.tv_nsec / 1000;
    return 0;
}

int wubu_nanosleep(const struct wubu_timespec *req, struct wubu_timespec *rem)
{
    return (int)wubu_syscall6(SC_NANOSLEEP, (long)req, (long)rem, 0, 0, 0, 0);
}

int wubu_clock_nanosleep(int clk, int flags,
                         const struct wubu_timespec *req,
                         struct wubu_timespec *rem)
{
    return (int)wubu_syscall6(SC_CLOCK_NANOSLEEP, clk, flags, (long)req,
                              (long)rem, 0, 0);
}
