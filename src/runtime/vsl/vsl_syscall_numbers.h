/*
 * vsl_syscall_numbers.h  --  VSL Syscall Numbers (Linux x86-64 ABI)
 * Self-contained header: just #define constants, NO includes, NO structs
 * Minimal dependency: can be included from any file without pulling in VSL types.
 */
#ifndef WUBUOS_VSL_SYSCALL_NUMBERS_H
#define WUBUOS_VSL_SYSCALL_NUMBERS_H

/* VSL syscall numbers (Linux x86_64 ABI) */
#define VSL_SYS_READ        0
#define VSL_SYS_WRITE       1
#define VSL_SYS_OPEN        2
#define VSL_SYS_CLOSE       3
#define VSL_SYS_STAT        4
#define VSL_SYS_FSTAT       5
#define VSL_SYS_LSEEK       8
#define VSL_SYS_MMAP        9
#define VSL_SYS_MUNMAP      11
#define VSL_SYS_BRK         12
#define VSL_SYS_IOCTL       16
#define VSL_SYS_ACCESS      21
#define VSL_SYS_PIPE        22
#define VSL_SYS_SELECT      23
#define VSL_SYS_SCHED_YIELD 24
#define VSL_SYS_FORK        57
#define VSL_SYS_VFORK       58
#define VSL_SYS_EXECVE      59
#define VSL_SYS_EXIT        60
#define VSL_SYS_WAIT4       61
#define VSL_SYS_KILL        62
#define VSL_SYS_GETPID      39
#define VSL_SYS_GETPPID     110
#define VSL_SYS_GETUID      102
#define VSL_SYS_GETGID      104
#define VSL_SYS_CLONE       56
#define VSL_SYS_PREAD64     17
#define VSL_SYS_PWRITE64    18
#define VSL_SYS_READV       19
#define VSL_SYS_WRITEV      20
#define VSL_SYS_DUP         32
#define VSL_SYS_DUP2        33
#define VSL_SYS_FCNTL       72
#define VSL_SYS_FSYNC       74
#define VSL_SYS_TRUNCATE    76
#define VSL_SYS_FTRUNCATE   77
#define VSL_SYS_GETCWD      79
#define VSL_SYS_CHDIR       80
#define VSL_SYS_FCHDIR      81
#define VSL_SYS_RENAME      82
#define VSL_SYS_MKDIR       83
#define VSL_SYS_RMDIR       84
#define VSL_SYS_CREAT       85
#define VSL_SYS_UNLINK      87
#define VSL_SYS_SYMLINK     88
#define VSL_SYS_READLINK    89
#define VSL_SYS_CHMOD       90
#define VSL_SYS_FCHMOD      94
#define VSL_SYS_CHOWN       92
#define VSL_SYS_LCHOWN      95
#define VSL_SYS_FCHOWN      96
#define VSL_SYS_LSTAT       6
#define VSL_SYS_POLL        7
#define VSL_SYS_EPOLL_CREATE 213
#define VSL_SYS_EPOLL_CTL   233
#define VSL_SYS_EPOLL_WAIT  232
#define VSL_SYS_SOCKET      41
#define VSL_SYS_CONNECT     42
#define VSL_SYS_ACCEPT      43
#define VSL_SYS_SENDTO      44
#define VSL_SYS_RECVFROM    45
#define VSL_SYS_SHUTDOWN    48
#define VSL_SYS_BIND        49
#define VSL_SYS_LISTEN      50
#define VSL_SYS_GETSOCKNAME 51
#define VSL_SYS_GETPEERNAME 52
#define VSL_SYS_SETSOCKOPT  54
#define VSL_SYS_FUTEX       202
#define VSL_SYS_RT_SIGACTION 13
#define VSL_SYS_RT_SIGPROCMASK 14
#define VSL_SYS_CLOCK_GETTIME 228
#define VSL_SYS_EXIT_GROUP  231
#define VSL_SYS_OPENAT      257
#define VSL_SYS_NEWFSTATAT  262
#define VSL_SYS_UNLINKAT    263
#define VSL_SYS_FACCESSAT   269
#define VSL_SYS_PIPE2       293
#define VSL_SYS_CLONE3      435
#define VSL_SYS_WAITPID     VSL_SYS_WAIT4  /* waitpid maps to wait4 */
#define VSL_SYS_SOCKETPAIR  53

/* Identity, Credentials & System Info Syscalls */
#define VSL_SYS_UMASK       95
#define VSL_SYS_SETUID      105
#define VSL_SYS_SETGID      106
#define VSL_SYS_GETEUID     107
#define VSL_SYS_GETEGID     108
#define VSL_SYS_SETPGID     109
#define VSL_SYS_SETSID      112
#define VSL_SYS_SETREUID    113
#define VSL_SYS_SETREGID    114
#define VSL_SYS_GETGROUPS   115
#define VSL_SYS_SETGROUPS   116
#define VSL_SYS_SETRESUID   117
#define VSL_SYS_GETRESUID   118
#define VSL_SYS_SETRESGID   119
#define VSL_SYS_GETRESGID   120
#define VSL_SYS_GETPGID     121
#define VSL_SYS_GETSID      124
#define VSL_SYS_SYSINFO     99
#define VSL_SYS_GETRANDOM   318
#define VSL_SYS_PRLIMIT64   302
#define VSL_SYS_ALARM       37
#define VSL_SYS_UNAME       63
#define VSL_SYS_GETRLIMIT   97
#define VSL_SYS_SETRLIMIT   98

/* Additional syscalls */
#define VSL_SYS_GETDENTS64  217
#define VSL_SYS_STATX       332
#define VSL_SYS_MREMAP      25
#define VSL_SYS_MPROTECT    10
#define VSL_SYS_MSYNC       26
#define VSL_SYS_RT_SIGSUSPEND 130
#define VSL_SYS_RT_SIGPENDING 127
#define VSL_SYS_RT_SIGQUEUEINFO 129
#define VSL_SYS_RT_SIGTIMEDWAIT 128
#define VSL_SYS_TIMER_CREATE 222
#define VSL_SYS_TIMER_SETTIME 223
#define VSL_SYS_TIMER_GETTIME 224
#define VSL_SYS_TIMER_DELETE 226
#define VSL_SYS_TIMERFD_CREATE 283
#define VSL_SYS_TIMERFD_SETTIME 286
#define VSL_SYS_TIMERFD_GETTIME 287
#define VSL_SYS_EVENTFD     323
#define VSL_SYS_EVENTFD2    290
#define VSL_SYS_INOTIFY_INIT 253
#define VSL_SYS_INOTIFY_ADD_WATCH 254
#define VSL_SYS_INOTIFY_RM_WATCH 255
/* VSL_SYS_SIGNALFD (308) removed - obsolete, use signalfd4 (282) instead */
#define VSL_SYS_SIGNALFD4   282
#define VSL_SYS_PSELECT6    270
#define VSL_SYS_PPOLL       271
#define VSL_SYS_READLINKAT  267
/* VSL_SYS_FSTATAT (262) removed - alias for VSL_SYS_NEWFSTATAT (262) */
#define VSL_SYS_FCHMODAT    268
#define VSL_SYS_FCHOWNAT    260
#define VSL_SYS_UTIMENSAT   280
#define VSL_SYS_FUTIMESAT   261
#define VSL_SYS_RENAMEAT    264
#define VSL_SYS_MKDIRAT     258
/* VSL_SYS_UNLINKAT (263) removed - duplicate of line above */
#define VSL_SYS_SYMLINKAT   266
#define VSL_SYS_LINKAT      265
#define VSL_SYS_MKNODAT     259
#define VSL_SYS_GETWD       183

/* Namespace & Security Syscalls */
#define VSL_SYS_UNSHARE     272
#define VSL_SYS_SETNS       308
#define VSL_SYS_FANOTIFY_INIT 300
#define VSL_SYS_FANOTIFY_MARK 301
#define VSL_SYS_LANDLOCK    444
#define VSL_SYS_BPF         321
#define VSL_SYS_PERF_EVENT_OPEN 298

/* io_uring Syscalls */
#define VSL_SYS_IO_URING_SETUP    425
#define VSL_SYS_IO_URING_ENTER    426
#define VSL_SYS_IO_URING_REGISTER 427

#endif /* WUBUOS_VSL_SYSCALL_NUMBERS_H */