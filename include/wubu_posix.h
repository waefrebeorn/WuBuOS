#ifndef WUBU_POSIX_H
#define WUBU_POSIX_H

/*
 * wubu_posix -- our own minimal POSIX surface, written ground-up in C11.
 *
 * Deliberately NOT a copy of any libc: the file/process/time calls we need are
 * declared here with our own types, and the implementation issues the Linux
 * syscalls directly through an inline-asm trap on x86-64 (with a documented
 * syscall() fallback elsewhere).  Nothing in this header comes from a libc
 * header, so a component built on it does not inherit the host's headers.
 */

#include <stddef.h>
#include <stdint.h>

typedef int          wubu_mode;
typedef unsigned int wubu_uid;
typedef long         wubu_off;

struct wubu_timespec {
    long tv_sec;
    long tv_nsec;
};

/* x86-64 kernel `struct stat` layout, reproduced exactly (144 bytes). */
struct wubu_stat {
    uint64_t st_dev;
    uint64_t st_ino;
    uint64_t st_nlink;
    int32_t  st_mode;
    uint32_t st_uid;
    uint32_t st_gid;
    int32_t  __pad0;
    uint64_t st_rdev;
    int64_t  st_size;
    int64_t  st_blksize;
    int64_t  st_blocks;
    struct wubu_timespec st_atim;
    struct wubu_timespec st_mtim;
    struct wubu_timespec st_ctim;
    int32_t  __reserved[3];
};

#define WUBU_O_RDONLY   0x0000
#define WUBU_O_WRONLY   0x0001
#define WUBU_O_RDWR     0x0002
#define WUBU_O_CREAT    0x0040
#define WUBU_O_TRUNC    0x0200
#define WUBU_O_APPEND   0x0400
#define WUBU_S_IRWXU    0x01C0

#define WUBU_CLOCK_REALTIME  0
#define WUBU_CLOCK_MONOTONIC 1
#define WUBU_CLOCK_BOOTTIME  7

#define WUBU_SEEK_SET 0
#define WUBU_SEEK_CUR 1
#define WUBU_SEEK_END 2

#define WUBU_STDOUT 1

/* Raw syscall plumbing -- exposed so WuBuTIME can calibrate against a second
 * clock without dragging in anything else.  Negative results are -errno. */
long wubu_syscall6(long nr, long a, long b, long c,
                   long d, long e, long f);

int      wubu_open(const char *path, int flags, wubu_mode mode);
int      wubu_close(int fd);
long     wubu_read(int fd, void *buf, size_t len);
long     wubu_write(int fd, const void *buf, size_t len);
wubu_off wubu_lseek(int fd, wubu_off off, int whence);
int      wubu_fsync(int fd);
int      wubu_fstat(int fd, struct wubu_stat *st);
int      wubu_stat_path(const char *path, struct wubu_stat *st);
int      wubu_unlink(const char *path);
int      wubu_rename(const char *from, const char *to);
int      wubu_mkdir(const char *path, wubu_mode mode);

int      wubu_getpid(void);
int      wubu_getuid(void);
void     wubu_exit(int code) __attribute__((noreturn));

int      wubu_clock_gettime(int clk, struct wubu_timespec *ts);
int      wubu_nanosleep(const struct wubu_timespec *req,
                        struct wubu_timespec *rem);

#endif /* WUBU_POSIX_H */
