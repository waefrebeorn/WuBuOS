/*
 * seccomp_registry.c -- seccomp profile registry (the Revolver Doctrine).
 *
 * Split out of ct_iso_seccomp.c so the profile registry is a
 * self-contained, linkable module (no cgroup/ns deps). The three
 * built-in profiles are declared extern (defined in ct_iso_seccomp.c);
 * tests link just this file.
 */
#include "ct_iso_seccomp.h"
#include <stdio.h>
#include <string.h>
#include <linux/seccomp.h>
#include <linux/filter.h>
#include <linux/audit.h>
#include <sys/prctl.h>
#include <sys/syscall.h>

/* The built-in allowlists (defined in ct_iso_seccomp.c). */
extern const int g_seccomp_basic_allowlist[];
extern const int g_seccomp_gpu_allowlist[];
extern const int g_seccomp_wine_allowlist[];

#define SECCOMP_REGISTRY_MAX 16

typedef struct {
    char name[32];
    const int *allowlist;   /* NULL-terminated (last entry -1) */
} seccomp_profile_entry;

static seccomp_profile_entry g_seccomp_registry[SECCOMP_REGISTRY_MAX];
static int g_seccomp_registry_n = 0;
static int g_seccomp_registry_seeded = 0;

static void seccomp_registry_seed(void)
{
    if (g_seccomp_registry_seeded) return;
    g_seccomp_registry[g_seccomp_registry_n++] = (seccomp_profile_entry){"basic", g_seccomp_basic_allowlist};
    g_seccomp_registry[g_seccomp_registry_n++] = (seccomp_profile_entry){"gpu",   g_seccomp_gpu_allowlist};
    g_seccomp_registry[g_seccomp_registry_n++] = (seccomp_profile_entry){"wine",  g_seccomp_wine_allowlist};
    g_seccomp_registry_seeded = 1;
}

int wubu_seccomp_profile_register(const char *name, const int *allowlist)
{
    if (!name || !allowlist) return -1;
    seccomp_registry_seed();
    /* Overwrite an existing profile of the same name (the cartridge swap). */
    for (int i = 0; i < g_seccomp_registry_n; i++) {
        if (strcmp(g_seccomp_registry[i].name, name) == 0) {
            g_seccomp_registry[i].allowlist = allowlist;
            return 0;
        }
    }
    if (g_seccomp_registry_n >= SECCOMP_REGISTRY_MAX) return -1;
    seccomp_profile_entry *e = &g_seccomp_registry[g_seccomp_registry_n++];
    snprintf(e->name, sizeof(e->name), "%s", name);
    e->allowlist = allowlist;
    return 0;
}

/* Look up a profile's allowlist by name (the trigger probes the live
 * cylinder). Returns NULL if not registered. */
const int *wubu_seccomp_profile_lookup(const char *name)
{
    seccomp_registry_seed();
    for (int i = 0; i < g_seccomp_registry_n; i++)
        if (strcmp(g_seccomp_registry[i].name, name) == 0)
            return g_seccomp_registry[i].allowlist;
    return NULL;
}
const int g_seccomp_basic_allowlist[] = {
    /* Process control */
    __NR_exit,
    __NR_exit_group,
    __NR_getpid,
    __NR_getppid,
    __NR_gettid,
    __NR_getuid,
    __NR_getgid,
    __NR_geteuid,
    __NR_getegid,
    __NR_setuid,
    __NR_setgid,

    /* Memory */
    __NR_mmap,
    __NR_munmap,
    __NR_mprotect,
    __NR_mremap,
    __NR_brk,
    __NR_madvise,
    __NR_mincore,
    __NR_mlock,
    __NR_munlock,
    __NR_mlockall,
    __NR_munlockall,

    /* File I/O */
    __NR_open,
    __NR_openat,
    __NR_close,
    __NR_read,
    __NR_write,
    __NR_pread64,
    __NR_pwrite64,
    __NR_lseek,
    __NR_fstat,
    __NR_stat,
    __NR_newfstatat,
    __NR_lstat,
    __NR_access,
    __NR_faccessat,
    __NR_fsync,
    __NR_fdatasync,
    __NR_ftruncate,
    __NR_truncate,
    __NR_getdents64,
    __NR_fcntl,
    __NR_ioctl,
    __NR_dup,
    __NR_dup2,
    __NR_dup3,
    __NR_pipe,
    __NR_pipe2,
    __NR_readv,
    __NR_writev,
    __NR_preadv,
    __NR_pwritev,

    /* Directory */
    __NR_chdir,
    __NR_fchdir,
    __NR_getcwd,
    __NR_mkdir,
    __NR_mkdirat,
    __NR_rmdir,
    __NR_unlink,
    __NR_unlinkat,
    __NR_rename,
    __NR_renameat,
    __NR_link,
    __NR_linkat,
    __NR_symlink,
    __NR_symlinkat,
    __NR_readlink,
    __NR_readlinkat,
    __NR_chmod,
    __NR_fchmod,
    __NR_fchmodat,
    __NR_chown,
    __NR_fchown,
    __NR_lchown,
    __NR_fchownat,

    /* Process creation (RESTRICTED - only fork/execve for containers) */
    __NR_fork,
    __NR_vfork,
    __NR_clone,
    __NR_execve,
    __NR_execveat,
    __NR_wait4,
    /* __NR_waitpid is not a direct syscall on x86_64, use wait4 */

    /* Signals */
    __NR_rt_sigaction,
    __NR_rt_sigprocmask,
    __NR_rt_sigpending,
    __NR_rt_sigsuspend,
    __NR_rt_sigtimedwait,
    __NR_kill,
    __NR_tkill,
    __NR_tgkill,
    __NR_signalfd,
    __NR_signalfd4,

    /* Time */
    __NR_clock_gettime,
    __NR_clock_getres,
    __NR_nanosleep,
    __NR_timer_create,
    __NR_timer_settime,
    __NR_timer_gettime,
    __NR_timer_delete,
    __NR_timerfd_create,
    __NR_timerfd_settime,
    __NR_timerfd_gettime,

    /* Poll/epoll */
    __NR_poll,
    __NR_ppoll,
    __NR_epoll_create,
    __NR_epoll_create1,
    __NR_epoll_ctl,
    __NR_epoll_wait,
    __NR_epoll_pwait,
    __NR_select,
    __NR_pselect6,

    /* Futex */
    __NR_futex,

    /* Scheduling */
    __NR_sched_yield,
    __NR_sched_getaffinity,
    __NR_sched_setaffinity,
    __NR_sched_getparam,
    __NR_sched_setparam,
    __NR_sched_getscheduler,
    __NR_sched_setscheduler,
    __NR_sched_get_priority_max,
    __NR_sched_get_priority_min,

    /* Prlimit */
    __NR_prlimit64,

    /* System info */
    __NR_uname,
    __NR_getrlimit,
    __NR_setrlimit,
    __NR_getrusage,
    __NR_sysinfo,

    /* Architecture-specific (x86_64) */
    __NR_arch_prctl,

    /* Eventfd/inotify */
    __NR_eventfd,
    __NR_eventfd2,
    __NR_inotify_init,
    __NR_inotify_init1,
    __NR_inotify_add_watch,
    __NR_inotify_rm_watch,

    /* SHM */
    __NR_shmget,
    __NR_shmat,
    __NR_shmdt,
    __NR_shmctl,

    /* Socket (basic) */
    __NR_socket,
    __NR_socketpair,
    __NR_connect,
    __NR_bind,
    __NR_listen,
    __NR_accept,
    __NR_accept4,
    __NR_sendto,
    __NR_recvfrom,
    __NR_sendmsg,
    __NR_recvmsg,
    __NR_shutdown,
    __NR_getsockname,
    __NR_getpeername,
    __NR_getsockopt,
    __NR_setsockopt,

    /* Sentinel */
    -1
};

/* Extended allowlist for SteamOS/Proton (DRM, Vulkan, GPU) */
const int g_seccomp_gpu_allowlist[] = {
    /* DRM/KMS handled via ioctl */
    __NR_ioctl,
    __NR_mmap,
    __NR_munmap,
    __NR_mprotect,
    __NR_read,
    __NR_write,
    __NR_poll,
    __NR_epoll_ctl,
    __NR_epoll_wait,
    -1
};

/* Extended for Wine/Proton (Windows syscall emulation needs) */
const int g_seccomp_wine_allowlist[] = {
    /* Wine needs these for 32-bit emulation */
    __NR_mmap,
    __NR_munmap,
    __NR_mprotect,
    __NR_mremap,
    __NR_brk,
    __NR_clone,
    __NR_fork,
    __NR_vfork,
    __NR_execve,
    __NR_wait4,
    __NR_ptrace,  /* Wine uses ptrace for debugging */
    __NR_personality,
    __NR_modify_ldt,
    __NR_arch_prctl,
    __NR_set_thread_area,
    __NR_get_thread_area,
    __NR_remap_file_pages,
    __NR_sigaltstack,
    -1
};
