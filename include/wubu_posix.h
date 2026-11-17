#ifndef WUBU_POSIX_H
#define WUBU_POSIX_H

/*
 * wubu_posix -- WuBuOS' own POSIX surface, written ground-up in strict C11.
 *
 * Slerm, not wrap: every call is declared here and typed by us, and issued as
 * a Linux syscall through our own trap.  No libc header is included anywhere
 * in this layer, so `gcc -nostdinc -Iinclude -c` compiles it standalone --
 * that compile is the proof that nothing is borrowed.
 */

/* Freestanding mode: define the scalar types from the compiler's own
 * predefined macros instead of borrowing libc's stdint.h.  Hosted builds keep
 * including the standard headers. */
#if defined(__STDC_HOSTED__) && !__STDC_HOSTED__
typedef __SIZE_TYPE__        size_t;
typedef __PTRDIFF_TYPE__     ptrdiff_t;
typedef __UINT8_TYPE__       uint8_t;
typedef __UINT16_TYPE__      uint16_t;
typedef __UINT32_TYPE__      uint32_t;
typedef __UINT64_TYPE__      uint64_t;
typedef __INT32_TYPE__       int32_t;
typedef __INT64_TYPE__       int64_t;
#else
#include <stddef.h>
#include <stdint.h>
#endif

typedef int          wubu_mode;
typedef unsigned int wubu_uid;
typedef unsigned int wubu_gid;
typedef int          wubu_pid;
typedef long         wubu_off;

struct wubu_timespec {
    long tv_sec;
    long tv_nsec;
};

struct wubu_timeval {
    long tv_sec;
    long tv_usec;
};

/* x86-64 kernel `struct stat`, reproduced exactly (144 bytes). */
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

/* One getdents64 record, as the kernel emits it. */
struct wubu_dirent {
    uint64_t d_ino;
    int64_t  d_off;
    uint16_t d_reclen;
    uint8_t  d_type;
    char     d_name[];
};

/* Directory stream: our own buffering over getdents64. */
typedef struct wubu_dir {
    int  fd;
    int  pos;
    int  len;
    int  done;
    char buf[8192];
} wubu_dir;

#define WUBU_O_RDONLY    0x0000
#define WUBU_O_WRONLY    0x0001
#define WUBU_O_RDWR      0x0002
#define WUBU_O_CREAT     0x0040
#define WUBU_O_EXCL      0x0080
#define WUBU_O_TRUNC     0x0200
#define WUBU_O_APPEND    0x0400
#define WUBU_O_DIRECTORY 0x10000

#define WUBU_S_IRWXU    0x01C0
#define WUBU_S_IRUSR    0x0100
#define WUBU_S_IWUSR    0x0080
#define WUBU_S_IXUSR    0x0040

#define WUBU_CLOCK_REALTIME   0
#define WUBU_CLOCK_MONOTONIC  1
#define WUBU_CLOCK_BOOTTIME   7
#define WUBU_CLOCK_PROCESS    2

#define WUBU_SEEK_SET 0
#define WUBU_SEEK_CUR 1
#define WUBU_SEEK_END 2

#define WUBU_STDOUT 1
#define WUBU_STDERR 2

#define WUBU_F_OK 0
#define WUBU_X_OK 1
#define WUBU_W_OK 2
#define WUBU_R_OK 4

#define WUBU_PROT_READ  0x1
#define WUBU_PROT_WRITE 0x2
#define WUBU_PROT_EXEC  0x4
#define WUBU_MAP_PRIVATE   0x02
#define WUBU_MAP_ANONYMOUS 0x20

#define WUBU_DT_UNKNOWN 0
#define WUBU_DT_DIR     4
#define WUBU_DT_REG     8

/* ------------------------------------------------------------------ raw */
long wubu_syscall6(long nr, long a, long b, long c,
                   long d, long e, long f);

/* --------------------------------------------------------------- files */
int      wubu_open(const char *path, int flags, wubu_mode mode);
int      wubu_close(int fd);
long     wubu_read(int fd, void *buf, size_t len);
long     wubu_write(int fd, const void *buf, size_t len);
wubu_off wubu_lseek(int fd, wubu_off off, int whence);
int      wubu_pread(int fd, void *buf, size_t len, wubu_off off);
int      wubu_pwrite(int fd, const void *buf, size_t len, wubu_off off);
int      wubu_fsync(int fd);
int      wubu_fdatasync(int fd);
int      wubu_ftruncate(int fd, wubu_off len);
int      wubu_truncate(const char *path, wubu_off len);
int      wubu_fstat(int fd, struct wubu_stat *st);
int      wubu_stat_path(const char *path, struct wubu_stat *st);
int      wubu_lstat_path(const char *path, struct wubu_stat *st);
int      wubu_access(const char *path, int mode);
int      wubu_unlink(const char *path);
int      wubu_rmdir(const char *path);
int      wubu_mkdir(const char *path, wubu_mode mode);
int      wubu_rename(const char *from, const char *to);
int      wubu_link(const char *from, const char *to);
int      wubu_symlink(const char *target, const char *linkpath);
long     wubu_readlink(const char *path, char *buf, size_t cap);
int      wubu_chmod(const char *path, wubu_mode mode);
int      wubu_chown(const char *path, wubu_uid uid, wubu_gid gid);

/* ----------------------------------------------------------- directories */
wubu_dir            *wubu_opendir(const char *path);
struct wubu_dirent  *wubu_readdir(wubu_dir *d);
int                  wubu_closedir(wubu_dir *d);
long                 wubu_getdents(int fd, void *buf, size_t len);

/* -------------------------------------------------------------- memory */
void    *wubu_mmap(void *addr, size_t len, int prot, int flags, int fd,
                   wubu_off off);
int      wubu_munmap(void *addr, size_t len);
int      wubu_mprotect(void *addr, size_t len, int prot);

/* ------------------------------------------------------------ processes */
int      wubu_getpid(void);
int      wubu_getppid(void);
int      wubu_getuid(void);
int      wubu_geteuid(void);
int      wubu_getgid(void);
int      wubu_getegid(void);
int      wubu_umask(int mask);
int      wubu_dup(int fd);
int      wubu_dup2(int oldfd, int newfd);
int      wubu_pipe(int fds[2]);
void     wubu_exit(int code) __attribute__((noreturn));

/* ---------------------------------------------------------------- time */
int      wubu_clock_gettime(int clk, struct wubu_timespec *ts);
int      wubu_gettimeofday(struct wubu_timeval *tv);
int      wubu_nanosleep(const struct wubu_timespec *req,
                        struct wubu_timespec *rem);
int      wubu_clock_nanosleep(int clk, int flags,
                              const struct wubu_timespec *req,
                              struct wubu_timespec *rem);

#endif /* WUBU_POSIX_H */
