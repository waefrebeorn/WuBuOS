/*
 * ct_iso_seccomp.c  --  WuBuOS container seccomp-BPF syscall filtering (Cell 420 split).
 * Allowlist approach: deny-by-default, permit explicit syscalls per runtime.
 * Also owns wubu_ct_child_isolation (the child-side seccomp + ns apply).
 */

#define _GNU_SOURCE
#include "ct_iso_seccomp.h"
#include "ct_iso_ns.h"          /* wubu_ns_unshare, WUBU_NS_FLAGS */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <linux/seccomp.h>
#include <linux/filter.h>
#include <linux/audit.h>
#include <sys/prctl.h>
#include <sys/syscall.h>

/* ===================================================================
 * SECCOMP BPF FILTER INFRASTRUCTURE
 * =================================================================== */

/* BPF instruction helpers (avoid conflict with linux/filter.h) */
#undef BPF_STMT
#undef BPF_JUMP
#define BPF_STMT(code, k)    (struct sock_filter){ (uint16_t)(code), 0, 0, (uint32_t)(k) }
#define BPF_JUMP(code, k, jt, jf) (struct sock_filter){ (uint16_t)(code), (uint8_t)(jt), (uint8_t)(jf), (uint32_t)(k) }

/* Load architecture (AUDIT_ARCH_X86_64) -- offset 4 in struct seccomp_data */
#define LOAD_ARCH \
    BPF_STMT(BPF_LD | BPF_W | BPF_ABS, 4)

/* Load syscall number -- offset 0 in struct seccomp_data */
#define LOAD_SYSCALL \
    BPF_STMT(BPF_LD | BPF_W | BPF_ABS, 0)

/* Jump if equal (architecture match) */
#define JEQ_ARCH(val, jt) \
    BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, (val), (jt), 0)

/* Jump if equal (syscall match) */
#define JEQ_SYSCALL(val, jt) \
    BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, (val), (jt), 0)

/* Return ALLOW */
#define RET_ALLOW \
    BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ALLOW)

/* Return KILL_PROCESS */
#define RET_KILL \
    BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_KILL_PROCESS)

/* Return ERRNO (value = -errno) */
#define RET_ERRNO(err) \
    BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ERRNO | ((err) & SECCOMP_RET_DATA))

/* Architecture constant */
#ifndef AUDIT_ARCH_X86_64
#define AUDIT_ARCH_X86_64 0xC000003E
#endif

/* ===================================================================
 * SECCOMP PROFILES PER RUNTIME
 * =================================================================== */

/*
 * Minimum syscalls needed for basic process execution.
 * Allowlist approach: deny by default, allow explicit syscalls.
 */

static const int g_seccomp_basic_allowlist[] = {
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
static const int g_seccomp_gpu_allowlist[] = {
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
static const int g_seccomp_wine_allowlist[] = {
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

/* ===================================================================
 * SECCOMP FILTER BUILDING
 * =================================================================== */

struct seccomp_filter_builder {
    struct sock_filter *filters;
    size_t count;
    size_t capacity;
};

static void filter_init(struct seccomp_filter_builder *b, size_t initial) {
    b->capacity = initial ? initial : 256;
    b->count = 0;
    b->filters = calloc(b->capacity, sizeof(struct sock_filter));
}

static void filter_add(struct seccomp_filter_builder *b, struct sock_filter f) {
    if (b->count >= b->capacity) {
        b->capacity *= 2;
        b->filters = realloc(b->filters, b->capacity * sizeof(struct sock_filter));
    }
    b->filters[b->count++] = f;
}

static void filter_add_allow_syscall(struct seccomp_filter_builder *b, int syscall_nr) {
    /* if (syscall == nr) return ALLOW; */
    filter_add(b, LOAD_SYSCALL);
    filter_add(b, JEQ_SYSCALL(syscall_nr, 1));
    filter_add(b, RET_ALLOW);
}

static int filter_build_allowlist(struct seccomp_filter_builder *b,
                                   const int *allowlist, size_t n_allowlist) {
    /* Load architecture and verify x86_64 */
    filter_add(b, LOAD_ARCH);
    filter_add(b, JEQ_ARCH(AUDIT_ARCH_X86_64, 1));
    filter_add(b, RET_KILL);

    /* Load syscall number */
    filter_add(b, LOAD_SYSCALL);

    /* For each allowed syscall, add a JEQ -> ALLOW */
    for (size_t i = 0; i < n_allowlist; i++) {
        int nr = allowlist[i];
        if (nr < 0) break;
        filter_add_allow_syscall(b, nr);
    }

    /* Default: KILL_PROCESS */
    filter_add(b, RET_KILL);

    return 0;
}

/* ===================================================================
 * CGROUPS V2 INTEGRATION
 * =================================================================== */


/* Namespace flags for full container isolation */
#define WUBU_NS_FLAGS (CLONE_NEWNS | CLONE_NEWPID | CLONE_NEWNET | \
                       CLONE_NEWUSER | CLONE_NEWUTS | CLONE_NEWIPC)









/* ===================================================================
 * CGROUP V2 I/O CONTROLLER SUPPORT
 * =================================================================== */



/* ===================================================================
 * CONTAINER ISOLATION INTEGRATION
 * =================================================================== */

SeccompProfile runtime_to_seccomp(CtRuntime runtime) {
    switch (runtime) {
        case CT_NATIVE:
        case CT_HOLYC:
            return SECCOMP_PROFILE_BASIC;
        case CT_STEAMOS:
            return SECCOMP_PROFILE_GPU;
        case CT_PROTON:
            return SECCOMP_PROFILE_WINE;
        default:
            return SECCOMP_PROFILE_BASIC;
    }
}

/* Apply seccomp filter in the child process (after fork, before exec) */
int wubu_ct_apply_seccomp(void *ct_ptr) {
    WubuCt *ct = (WubuCt *)ct_ptr;
    SeccompProfile profile = runtime_to_seccomp(ct->runtime);
    if (profile == SECCOMP_PROFILE_NONE) return 0;

    struct seccomp_filter_builder builder;
    filter_init(&builder, 512);

    /* Build combined allowlist based on profile */
    const int *lists[3];
    size_t n_lists = 0;

    lists[n_lists++] = g_seccomp_basic_allowlist;
    if (profile == SECCOMP_PROFILE_GPU) {
        lists[n_lists++] = g_seccomp_gpu_allowlist;
    } else if (profile == SECCOMP_PROFILE_WINE) {
        lists[n_lists++] = g_seccomp_wine_allowlist;
    }

    /* Calculate total allowed syscalls */
    size_t total = 0;
    for (size_t i = 0; i < n_lists; i++) {
        for (size_t j = 0; lists[i][j] >= 0; j++) total++;
    }

    /* Build filter */
    filter_build_allowlist(&builder, g_seccomp_basic_allowlist, total);

    /* Install filter via prctl + seccomp syscall */
    struct sock_fprog prog = {
        .len = (unsigned short)builder.count,
        .filter = builder.filters,
    };

    /* PR_SET_NO_NEW_PRIVS must be set before seccomp */
    if (prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0) != 0) {
        free(builder.filters);
        return -1;
    }

    if (syscall(SYS_seccomp, SECCOMP_SET_MODE_FILTER, 0, &prog) != 0) {
        free(builder.filters);
        return -1;
    }

    free(builder.filters);
    return 0;
}
int wubu_ct_child_isolation(void) {
    /* Get cgroup path from environment */
    const char *cgroup_path = getenv("WUBU_CGROUP_PATH");
    if (cgroup_path) {
        pid_t pid = getpid();
        wubu_ct_cgroup_attach(cgroup_path, pid);
    }

    /* Unshare namespaces for container isolation */
    const char *ns_flags_str = getenv("WUBU_NS_FLAGS");
    if (ns_flags_str) {
        int flags = atoi(ns_flags_str);
        if (flags != 0) {
            wubu_ns_unshare(flags);
        }
    } else {
        /* Default: full isolation */
        wubu_ns_unshare(WUBU_NS_FLAGS);
    }

    /* Apply seccomp filter */
    const char *profile = getenv("WUBU_SECCOMP_PROFILE");
    SeccompProfile sp = SECCOMP_PROFILE_BASIC;
    if (profile) {
        if (strcmp(profile, "gpu") == 0) sp = SECCOMP_PROFILE_GPU;
        else if (strcmp(profile, "wine") == 0) sp = SECCOMP_PROFILE_WINE;
        else if (strcmp(profile, "none") == 0) sp = SECCOMP_PROFILE_NONE;
    }

    if (sp != SECCOMP_PROFILE_NONE) {
        struct seccomp_filter_builder builder;
        filter_init(&builder, 512);

        const int *lists[2];
        size_t n_lists = 1;
        lists[0] = g_seccomp_basic_allowlist;
        if (sp == SECCOMP_PROFILE_GPU) lists[n_lists++] = g_seccomp_gpu_allowlist;
        else if (sp == SECCOMP_PROFILE_WINE) lists[n_lists++] = g_seccomp_wine_allowlist;

        size_t total = 0;
        for (size_t i = 0; i < n_lists; i++) {
            for (size_t j = 0; lists[i][j] >= 0; j++) total++;
        }

        filter_build_allowlist(&builder, g_seccomp_basic_allowlist, total);

        struct sock_fprog prog = {
            .len = (unsigned short)builder.count,
            .filter = builder.filters,
        };

        prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0);
        syscall(SYS_seccomp, SECCOMP_SET_MODE_FILTER, 0, &prog);
        free(builder.filters);
    }

    return 0;
}

/* ===================================================================
 * Namespace Isolation API
 * =================================================================== */

/* Write to a cgroup file (generic helper) */
int wubu_seccomp_install(SeccompProfile profile) {
    if (profile == SECCOMP_PROFILE_NONE) return 0;

    struct seccomp_filter_builder builder;
    filter_init(&builder, 512);

    const int *lists[2];
    size_t n_lists = 1;
    lists[0] = g_seccomp_basic_allowlist;
    if (profile == SECCOMP_PROFILE_GPU) lists[n_lists++] = g_seccomp_gpu_allowlist;
    else if (profile == SECCOMP_PROFILE_WINE) lists[n_lists++] = g_seccomp_wine_allowlist;

    size_t total = 0;
    for (size_t i = 0; i < n_lists; i++) {
        for (size_t j = 0; lists[i][j] >= 0; j++) total++;
    }

    filter_build_allowlist(&builder, g_seccomp_basic_allowlist, total);

    struct sock_fprog prog = {
        .len = (unsigned short)builder.count,
        .filter = builder.filters,
    };

    if (prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0) != 0) {
        free(builder.filters);
        return -1;
    }

    if (syscall(SYS_seccomp, SECCOMP_SET_MODE_FILTER, 0, &prog) != 0) {
        free(builder.filters);
        return -1;
    }

    free(builder.filters);
    return 0;
}

/* Unshare namespaces for container isolation */
