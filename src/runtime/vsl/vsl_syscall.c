/*
 * vsl_syscall.c  --  VSL Syscall Bridge Facade
 * Table-driven dispatch + helper functions.
 * Handler implementations live in:
 *   vsl_syscall_proc.c   - process mgmt, identity/credentials, sysinfo
 *   vsl_syscall_fileio.c - file I/O, advanced I/O, poll/epoll
 *   vsl_syscall_memory.c - mmap, munmap, brk, mprotect, shm
 *   vsl_syscall_net.c    - socket, signals, timers, namespace, *at
 */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "vsl_syscall_internal.h"

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

    /* Identity, Credentials & System Info Syscalls */
    [VSL_SYS_GETEUID]      = vsl_sys_geteuid,
    [VSL_SYS_GETEGID]      = vsl_sys_getegid,
    [VSL_SYS_SETUID]       = vsl_sys_setuid,
    [VSL_SYS_SETGID]       = vsl_sys_setgid,
    [VSL_SYS_SETREUID]     = vsl_sys_setreuid,
    [VSL_SYS_SETREGID]     = vsl_sys_setregid,
    [VSL_SYS_GETRESUID]    = vsl_sys_getresuid,
    [VSL_SYS_GETRESGID]    = vsl_sys_getresgid,
    [VSL_SYS_SETRESUID]    = vsl_sys_setresuid,
    [VSL_SYS_SETRESGID]    = vsl_sys_setresgid,
    [VSL_SYS_GETGROUPS]    = vsl_sys_getgroups,
    [VSL_SYS_SETGROUPS]    = vsl_sys_setgroups,
    [VSL_SYS_SETPGID]      = vsl_sys_setpgid,
    [VSL_SYS_GETPGID]      = vsl_sys_getpgid,
    [VSL_SYS_GETSID]       = vsl_sys_getsid,
    [VSL_SYS_SETSID]       = vsl_sys_setsid,
    [VSL_SYS_UMASK]        = vsl_sys_umask,
    [VSL_SYS_UNAME]        = vsl_sys_uname,
    [VSL_SYS_SYSINFO]      = vsl_sys_sysinfo,
    [VSL_SYS_GETRANDOM]    = vsl_sys_getrandom,
    [VSL_SYS_GETRLIMIT]    = vsl_sys_getrlimit,
    [VSL_SYS_SETRLIMIT]    = vsl_sys_setrlimit,
    [VSL_SYS_PRLIMIT64]    = vsl_sys_prlimit64,
    [VSL_SYS_ALARM]        = vsl_sys_alarm,
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

int vsl_get_host_fd(int vsl_fd) {
    if (vsl_fd < 3) return vsl_fd; /* stdin/stdout/stderr */
    for (uint32_t i = 0; i < g_vsl.n_fds; i++) {
        if (g_vsl.fds[i].fd == vsl_fd) return g_vsl.fds[i].vsl_fd;
    }
    return -1;
}

int find_free_vsl_pid(void) {
    for (uint32_t pid = 2; pid < 100000; pid++) {
        if (!vsl_get_process(pid)) return (int)pid;
    }
    return -1;
}

int register_child_pid(pid_t child_host_pid, uint32_t parent_vsl_pid) {
    if (child_host_pid <= 0) return -1;
    if (g_vsl.n_procs >= VSL_MAX_PROCS) return -1;
    int vsl_pid = find_free_vsl_pid();
    if (vsl_pid < 0) return -1;
    VSL_PROC *proc = &g_vsl.procs[g_vsl.n_procs];
    memset(proc, 0, sizeof(*proc));
    proc->pid = (uint32_t)vsl_pid;
    proc->ppid = parent_vsl_pid;
    proc->state = VSL_PROC_READY;
    proc->uid = (uint32_t)getuid();
    proc->gid = (uint32_t)getgid();
    proc->euid = (uint32_t)geteuid();
    proc->egid = (uint32_t)getegid();
    proc->suid = (uint32_t)geteuid();   /* saved-set == euid at spawn (like exec) */
    proc->sgid = (uint32_t)getegid();
    proc->pgid = -1;
    proc->sesid = -1;
    proc->umask = 0022;
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

/* -- Syscall Statistics ------------------------------------------- */

void vsl_get_syscall_stats(uint64_t *out_count, uint64_t *out_errors) {
    if (out_count) *out_count = g_vsl.syscall_count;
    if (out_errors) *out_errors = g_vsl.syscall_errors;
}
