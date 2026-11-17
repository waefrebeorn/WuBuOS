/*
 * wubu_posix -- our own POSIX surface, issued as raw Linux syscalls.
 *
 * No libc headers are included.  On x86-64 the trap is inline asm so the
 * syscall numbers are ours and visible; other targets fall back to syscall().
 */
#include "wubu_posix.h"

#if defined(__x86_64__)
/* Syscall numbers as we define them; they are the x86-64 ABI ones. */
#define SC_READ      0
#define SC_WRITE     1
#define SC_OPEN      2
#define SC_CLOSE     3
#define SC_FSTAT     5
#define SC_LSEEK     8
#define SC_MMAP      9
#define SC_MUNMAP    11
#define SC_NANOSLEEP 35
#define SC_GETPID    39
#define SC_GETUID   102
#define SC_FSYNC     74
#define SC_GETCWD    79
#define SC_UNLINK   87
#define SC_RENAME   82
#define SC_MKDIR    83
#define SC_CLOCK_GETTIME 228

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

int wubu_fsync(int fd)
{
    return (int)wubu_syscall6(SC_FSYNC, fd, 0, 0, 0, 0, 0);
}

int wubu_fstat(int fd, struct wubu_stat *st)
{
    return (int)wubu_syscall6(SC_FSTAT, fd, (long)st, 0, 0, 0, 0);
}

int wubu_stat_path(const char *path, struct wubu_stat *st)
{
    /* statx is not universally available; use open+fstat through our layer. */
    int fd = wubu_open(path, WUBU_O_RDONLY, 0);
    if (fd < 0) return -1;
    int rc = wubu_fstat(fd, st);
    wubu_close(fd);
    return rc;
}

int wubu_unlink(const char *path)
{
    return (int)wubu_syscall6(SC_UNLINK, (long)path, 0, 0, 0, 0, 0);
}

int wubu_rename(const char *from, const char *to)
{
    return (int)wubu_syscall6(SC_RENAME, (long)from, (long)to, 0, 0, 0, 0);
}

int wubu_mkdir(const char *path, wubu_mode mode)
{
    return (int)wubu_syscall6(SC_MKDIR, (long)path, mode, 0, 0, 0, 0);
}

int wubu_getpid(void)
{
    return (int)wubu_syscall6(SC_GETPID, 0, 0, 0, 0, 0, 0);
}

int wubu_getuid(void)
{
    return (int)wubu_syscall6(SC_GETUID, 0, 0, 0, 0, 0, 0);
}

void wubu_exit(int code)
{
    wubu_syscall6(60 /* exit */, code, 0, 0, 0, 0, 0);
    for (;;) { }
}

int wubu_clock_gettime(int clk, struct wubu_timespec *ts)
{
    return (int)wubu_syscall6(SC_CLOCK_GETTIME, clk, (long)ts, 0, 0, 0, 0);
}

int wubu_nanosleep(const struct wubu_timespec *req, struct wubu_timespec *rem)
{
    return (int)wubu_syscall6(SC_NANOSLEEP, (long)req, (long)rem, 0, 0, 0, 0);
}
