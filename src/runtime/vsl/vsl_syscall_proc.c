/*
 * vsl_syscall_proc.c  --  VSL Process Management + Identity/Credential Syscalls
 * Fork, clone, exec, wait, exit, kill, getpid, getuid/gid, setuid/gid, etc.
 */

#include "vsl_syscall_internal.h"
#include <sched.h>
#include <linux/sched.h>

/* ====================================================================
 * PROCESS MANAGEMENT SYSCALLS
 * ==================================================================== */

int64_t vsl_sys_nosys(uint64_t a, uint64_t b, uint64_t c,
                      uint64_t d, uint64_t e, uint64_t f) {
    (void)a; (void)b; (void)c; (void)d; (void)e; (void)f;
    return -38; /* ENOSYS */
}

int64_t vsl_sys_exit(uint64_t code, uint64_t b, uint64_t c,
                      uint64_t d, uint64_t e, uint64_t f) {
    (void)b; (void)c; (void)d; (void)e; (void)f;
    VSL_PROC *p = vsl_get_process(g_vsl.current_pid);
    if (p) {
        p->state = VSL_PROC_ZOMBIE;
        p->exit_code = (int)(code & 0xFF);
    }
    return 0;
}

int64_t vsl_sys_getpid(uint64_t a, uint64_t b, uint64_t c,
                        uint64_t d, uint64_t e, uint64_t f) {
    (void)a; (void)b; (void)c; (void)d; (void)e; (void)f;
    return (int64_t)g_vsl.current_pid;
}

int64_t vsl_sys_getppid(uint64_t a, uint64_t b, uint64_t c,
                         uint64_t d, uint64_t e, uint64_t f) {
    (void)a; (void)b; (void)c; (void)d; (void)e; (void)f;
    VSL_PROC *p = vsl_get_process(g_vsl.current_pid);
    return p ? (int64_t)p->ppid : 0;
}

int64_t vsl_sys_fork(uint64_t a, uint64_t b, uint64_t c,
                      uint64_t d, uint64_t e, uint64_t f) {
    (void)b; (void)c; (void)d; (void)e; (void)f;
    pid_t host_pid = fork();
    if (host_pid < 0) return -errno;
    if (host_pid == 0) {
        g_vsl.current_pid = (uint32_t)host_pid;
        return 0;
    }
    int vsl_child = register_child_pid(host_pid, g_vsl.current_pid);
    if (vsl_child < 0) {
        kill(host_pid, SIGKILL);
        waitpid(host_pid, NULL, 0);
        return -1;
    }
    return (int64_t)vsl_child;
}

int64_t vsl_sys_clone(uint64_t flags, uint64_t stack, uint64_t ptid,
                       uint64_t ctid, uint64_t tls, uint64_t f) {
    (void)f;
    int clone_flags = (int)flags;
    void *child_stack = stack ? (void *)stack : NULL;
    int *parent_tidptr = ptid ? (int *)ptid : NULL;
    int *child_tidptr = ctid ? (int *)ctid : NULL;
    void *tls_ptr = tls ? (void *)tls : NULL;

    pid_t host_pid = (pid_t)syscall(SYS_clone, clone_flags, child_stack,
                                     parent_tidptr, child_tidptr, tls_ptr);
    if (host_pid < 0) return -errno;
    if (host_pid == 0) {
        g_vsl.current_pid = (uint32_t)host_pid;
        return 0;
    }
    int vsl_child = register_child_pid(host_pid, g_vsl.current_pid);
    if (vsl_child < 0) {
        kill(host_pid, SIGKILL);
        waitpid(host_pid, NULL, 0);
        return -1;
    }
    return (int64_t)vsl_child;
}

int64_t vsl_sys_vfork(uint64_t a, uint64_t b, uint64_t c,
                       uint64_t d, uint64_t e, uint64_t f) {
    (void)b; (void)c; (void)d; (void)e; (void)f;
    int clone_flags = CLONE_VM | CLONE_VFORK | SIGCHLD;
    pid_t host_pid = (pid_t)syscall(SYS_clone, clone_flags, NULL, NULL, NULL, NULL);
    if (host_pid < 0) return -errno;
    if (host_pid == 0) {
        g_vsl.current_pid = (uint32_t)host_pid;
        return 0;
    }
    int vsl_child = register_child_pid(host_pid, g_vsl.current_pid);
    if (vsl_child < 0) {
        kill(host_pid, SIGKILL);
        waitpid(host_pid, NULL, 0);
        return -1;
    }
    return (int64_t)vsl_child;
}

int64_t vsl_sys_execve(uint64_t path, uint64_t argv, uint64_t envp,
                        uint64_t d, uint64_t e, uint64_t f) {
    (void)d; (void)e; (void)f;
    const char *pathname = (const char *)path;
    if (!pathname) return -2;
    char **host_argv = NULL;
    int argc = 0;
    if (argv) {
        uint64_t *vsl_argv = (uint64_t *)argv;
        while (vsl_argv[argc]) argc++;
        host_argv = (char **)calloc((size_t)argc + 1, sizeof(char *));
        if (!host_argv) return -12;
        for (int i = 0; i < argc; i++)
            host_argv[i] = (char *)(uintptr_t)vsl_argv[i];
    }
    execve(pathname, host_argv, (char *const *)(uintptr_t)envp);
    free(host_argv);
    return -errno;
}

int64_t vsl_sys_wait4(uint64_t pid, uint64_t status, uint64_t options,
                       uint64_t rusage, uint64_t e, uint64_t f) {
    (void)e; (void)f;
    struct rusage ru;
    int host_status = 0;
    pid_t host_pid = (pid == (uint64_t)(-1)) ? -1 : (pid_t)(int)pid;
    pid_t result = syscall(SYS_wait4, host_pid, &host_status, (int)options, &ru);
    if (result < 0) return -errno;
    if (status && result > 0) {
        int *out = (int *)status;
        *out = host_status;
    }
    if (rusage) {
        memcpy((void *)rusage, &ru, sizeof(struct rusage));
    }
    return (int64_t)result;
}

int64_t vsl_sys_waitpid(uint64_t pid, uint64_t status, uint64_t options,
                         uint64_t d, uint64_t e, uint64_t f) {
    return vsl_sys_wait4(pid, status, options, 0, e, f);
}

int64_t vsl_sys_kill(uint64_t pid, uint64_t sig, uint64_t c,
                      uint64_t d, uint64_t e, uint64_t f) {
    (void)c; (void)d; (void)e; (void)f;
    int rc = kill((pid_t)pid, (int)sig);
    return rc < 0 ? -errno : (int64_t)rc;
}

int64_t vsl_sys_exit_group(uint64_t code, uint64_t b, uint64_t c,
                            uint64_t d, uint64_t e, uint64_t f) {
    return vsl_sys_exit(code, b, c, d, e, f);
}

/* ====================================================================
 * IDENTITY & CREDENTIAL SYSCALLS
 * ==================================================================== */

int64_t vsl_sys_getuid(uint64_t a, uint64_t b, uint64_t c,
                        uint64_t d, uint64_t e, uint64_t f) {
    (void)a; (void)b; (void)c; (void)d; (void)e; (void)f;
    VSL_PROC *p = vsl_get_process(g_vsl.current_pid);
    return p ? (int64_t)p->uid : (int64_t)getuid();
}

int64_t vsl_sys_getgid(uint64_t a, uint64_t b, uint64_t c,
                        uint64_t d, uint64_t e, uint64_t f) {
    (void)a; (void)b; (void)c; (void)d; (void)e; (void)f;
    VSL_PROC *p = vsl_get_process(g_vsl.current_pid);
    return p ? (int64_t)p->gid : (int64_t)getgid();
}

int64_t vsl_sys_geteuid(uint64_t a, uint64_t b, uint64_t c,
                         uint64_t d, uint64_t e, uint64_t f) {
    (void)a; (void)b; (void)c; (void)d; (void)e; (void)f;
    VSL_PROC *p = vsl_get_process(g_vsl.current_pid);
    return p ? (int64_t)p->euid : (int64_t)geteuid();
}

int64_t vsl_sys_getegid(uint64_t a, uint64_t b, uint64_t c,
                         uint64_t d, uint64_t e, uint64_t f) {
    (void)a; (void)b; (void)c; (void)d; (void)e; (void)f;
    VSL_PROC *p = vsl_get_process(g_vsl.current_pid);
    return p ? (int64_t)p->egid : (int64_t)getegid();
}

int64_t vsl_sys_setuid(uint64_t uid, uint64_t b, uint64_t c,
                        uint64_t d, uint64_t e, uint64_t f) {
    (void)b; (void)c; (void)d; (void)e; (void)f;
    int rc = setuid((uid_t)uid);
    if (rc == 0) {
        VSL_PROC *p = vsl_get_process(g_vsl.current_pid);
        if (p) { p->uid = (uint32_t)uid; p->euid = (uint32_t)uid; }
    }
    return rc < 0 ? -errno : 0;
}

int64_t vsl_sys_setgid(uint64_t gid, uint64_t b, uint64_t c,
                        uint64_t d, uint64_t e, uint64_t f) {
    (void)b; (void)c; (void)d; (void)e; (void)f;
    int rc = setgid((gid_t)gid);
    if (rc == 0) {
        VSL_PROC *p = vsl_get_process(g_vsl.current_pid);
        if (p) { p->gid = (uint32_t)gid; p->egid = (uint32_t)gid; }
    }
    return rc < 0 ? -errno : 0;
}

int64_t vsl_sys_setreuid(uint64_t ruid, uint64_t euid, uint64_t c,
                          uint64_t d, uint64_t e, uint64_t f) {
    (void)c; (void)d; (void)e; (void)f;
    int rc = setreuid((uid_t)ruid, (uid_t)euid);
    if (rc == 0) {
        VSL_PROC *p = vsl_get_process(g_vsl.current_pid);
        if (p) {
            if (ruid != (uint64_t)-1) p->uid = (uint32_t)ruid;
            if (euid != (uint64_t)-1) p->euid = (uint32_t)euid;
            if (euid != (uint64_t)-1) p->suid = (uint32_t)euid;  /* reuid sets saved-set = euid */
        }
    }
    return rc < 0 ? -errno : 0;
}

int64_t vsl_sys_setregid(uint64_t rgid, uint64_t egid, uint64_t c,
                          uint64_t d, uint64_t e, uint64_t f) {
    (void)c; (void)d; (void)e; (void)f;
    int rc = setregid((gid_t)rgid, (gid_t)egid);
    if (rc == 0) {
        VSL_PROC *p = vsl_get_process(g_vsl.current_pid);
        if (p) {
            if (rgid != (uint64_t)-1) p->gid = (uint32_t)rgid;
            if (egid != (uint64_t)-1) p->egid = (uint32_t)egid;
            if (egid != (uint64_t)-1) p->sgid = (uint32_t)egid;  /* regid sets saved-set = egid */
        }
    }
    return rc < 0 ? -errno : 0;
}

int64_t vsl_sys_getresuid(uint64_t ruid, uint64_t euid, uint64_t suid,
                           uint64_t d, uint64_t e, uint64_t f) {
    (void)d; (void)e; (void)f;
    VSL_PROC *p = vsl_get_process(g_vsl.current_pid);
    uid_t r  = p ? (uid_t)p->uid  : getuid();
    uid_t eu = p ? (uid_t)p->euid : geteuid();
    uid_t s  = p ? (uid_t)p->suid : geteuid();   /* tracked saved-set, not host */
    if (ruid) *(uid_t *)ruid = r;
    if (euid) *(uid_t *)euid = eu;
    if (suid) *(uid_t *)suid = s;
    return 0;
}

int64_t vsl_sys_getresgid(uint64_t rgid, uint64_t egid, uint64_t sgid,
                           uint64_t d, uint64_t e, uint64_t f) {
    (void)d; (void)e; (void)f;
    VSL_PROC *p = vsl_get_process(g_vsl.current_pid);
    gid_t r  = p ? (gid_t)p->gid  : getgid();
    gid_t eg = p ? (gid_t)p->egid : getegid();
    gid_t s  = p ? (gid_t)p->sgid : getegid();   /* tracked saved-set, not host */
    if (rgid) *(gid_t *)rgid = r;
    if (egid) *(gid_t *)egid = eg;
    if (sgid) *(gid_t *)sgid = s;
    return 0;
}

int64_t vsl_sys_setresuid(uint64_t ruid, uint64_t euid, uint64_t suid,
                           uint64_t d, uint64_t e, uint64_t f) {
    (void)d; (void)e; (void)f;
    int rc = setresuid((uid_t)ruid, (uid_t)euid, (uid_t)suid);
    if (rc == 0) {
        VSL_PROC *p = vsl_get_process(g_vsl.current_pid);
        if (p) {
            if (ruid != (uint64_t)-1) p->uid = (uint32_t)ruid;
            if (euid != (uint64_t)-1) p->euid = (uint32_t)euid;
            if (suid != (uint64_t)-1) p->suid = (uint32_t)suid;
        }
    }
    return rc < 0 ? -errno : 0;
}

int64_t vsl_sys_setresgid(uint64_t rgid, uint64_t egid, uint64_t sgid,
                           uint64_t d, uint64_t e, uint64_t f) {
    (void)d; (void)e; (void)f;
    int rc = setresgid((gid_t)rgid, (gid_t)egid, (gid_t)sgid);
    if (rc == 0) {
        VSL_PROC *p = vsl_get_process(g_vsl.current_pid);
        if (p) {
            if (rgid != (uint64_t)-1) p->gid = (uint32_t)rgid;
            if (egid != (uint64_t)-1) p->egid = (uint32_t)egid;
            if (sgid != (uint64_t)-1) p->sgid = (uint32_t)sgid;
        }
    }
    return rc < 0 ? -errno : 0;
}

int64_t vsl_sys_getgroups(uint64_t size, uint64_t list, uint64_t c,
                           uint64_t d, uint64_t e, uint64_t f) {
    (void)c; (void)d; (void)e; (void)f;
    int result = getgroups((int)size, (gid_t *)list);
    return result < 0 ? -errno : (int64_t)result;
}

int64_t vsl_sys_setgroups(uint64_t size, uint64_t list, uint64_t c,
                           uint64_t d, uint64_t e, uint64_t f) {
    (void)c; (void)d; (void)e; (void)f;
    int rc = setgroups((size_t)size, (const gid_t *)list);
    return rc < 0 ? -errno : 0;
}

int64_t vsl_sys_setpgid(uint64_t pid, uint64_t pgid, uint64_t c,
                         uint64_t d, uint64_t e, uint64_t f) {
    (void)c; (void)d; (void)e; (void)f;
    int rc = setpgid((pid_t)(int)pid, (pid_t)(int)pgid);
    if (rc == 0) {
        VSL_PROC *p = vsl_get_process(g_vsl.current_pid);
        if (p) p->pgid = (int)(int)pgid;
    }
    return rc < 0 ? -errno : 0;
}

int64_t vsl_sys_getpgid(uint64_t pid, uint64_t b, uint64_t c,
                         uint64_t d, uint64_t e, uint64_t f) {
    (void)b; (void)c; (void)d; (void)e; (void)f;
    pid_t result = getpgid((pid_t)(int)pid);
    return result < 0 ? -errno : (int64_t)result;
}

int64_t vsl_sys_setsid(uint64_t a, uint64_t b, uint64_t c,
                        uint64_t d, uint64_t e, uint64_t f) {
    (void)a; (void)b; (void)c; (void)d; (void)e; (void)f;
    pid_t result = setsid();
    if (result >= 0) {
        VSL_PROC *p = vsl_get_process(g_vsl.current_pid);
        if (p) { p->sesid = (int)result; p->pgid = (int)result; }
    }
    return result < 0 ? -errno : (int64_t)result;
}

int64_t vsl_sys_getsid(uint64_t pid, uint64_t b, uint64_t c,
                        uint64_t d, uint64_t e, uint64_t f) {
    (void)b; (void)c; (void)d; (void)e; (void)f;
    pid_t result = getsid((pid_t)(int)pid);
    return result < 0 ? -errno : (int64_t)result;
}

int64_t vsl_sys_umask(uint64_t mask, uint64_t b, uint64_t c,
                       uint64_t d, uint64_t e, uint64_t f) {
    (void)b; (void)c; (void)d; (void)e; (void)f;
    mode_t old = umask((mode_t)mask);
    VSL_PROC *p = vsl_get_process(g_vsl.current_pid);
    if (p) p->umask = (mode_t)mask;
    return (int64_t)old;
}

/* ====================================================================
 * SYSTEM INFO SYSCALLS
 * ==================================================================== */

int64_t vsl_sys_uname(uint64_t buf, uint64_t b, uint64_t c,
                       uint64_t d, uint64_t e, uint64_t f) {
    (void)b; (void)c; (void)d; (void)e; (void)f;
    struct utsname u;
    int rc = uname(&u);
    if (rc == 0 && buf) memcpy((void *)buf, &u, sizeof(struct utsname));
    return rc < 0 ? -errno : 0;
}

int64_t vsl_sys_sysinfo(uint64_t info, uint64_t b, uint64_t c,
                         uint64_t d, uint64_t e, uint64_t f) {
    (void)b; (void)c; (void)d; (void)e; (void)f;
    struct sysinfo si;
    int rc = sysinfo(&si);
    if (rc == 0 && info) memcpy((void *)info, &si, sizeof(struct sysinfo));
    return rc < 0 ? -errno : 0;
}

int64_t vsl_sys_getrandom(uint64_t buf, uint64_t buflen, uint64_t flags,
                           uint64_t d, uint64_t e, uint64_t f) {
    (void)d; (void)e; (void)f;
    ssize_t result = getrandom((void *)buf, (size_t)buflen, (unsigned int)flags);
    return result < 0 ? -errno : (int64_t)result;
}

int64_t vsl_sys_getrlimit(uint64_t resource, uint64_t rlim, uint64_t c,
                           uint64_t d, uint64_t e, uint64_t f) {
    (void)c; (void)d; (void)e; (void)f;
    int rc = getrlimit((int)resource, (struct rlimit *)rlim);
    return rc < 0 ? -errno : 0;
}

int64_t vsl_sys_setrlimit(uint64_t resource, uint64_t rlim, uint64_t c,
                           uint64_t d, uint64_t e, uint64_t f) {
    (void)c; (void)d; (void)e; (void)f;
    int rc = setrlimit((int)resource, (const struct rlimit *)rlim);
    return rc < 0 ? -errno : 0;
}

int64_t vsl_sys_prlimit64(uint64_t pid, uint64_t resource, uint64_t new_limit,
                           uint64_t old_limit, uint64_t e, uint64_t f) {
    (void)e; (void)f;
    int rc = syscall(SYS_prlimit64, (pid_t)(int)pid, (unsigned int)resource,
                     (const struct rlimit64 *)new_limit,
                     (struct rlimit64 *)old_limit);
    return rc < 0 ? -errno : 0;
}

int64_t vsl_sys_alarm(uint64_t seconds, uint64_t b, uint64_t c,
                       uint64_t d, uint64_t e, uint64_t f) {
    (void)b; (void)c; (void)d; (void)e; (void)f;
    unsigned int result = alarm((unsigned int)seconds);
    return (int64_t)result;
}

int64_t vsl_sys_sched_yield(uint64_t a, uint64_t b, uint64_t c,
                             uint64_t d, uint64_t e, uint64_t f) {
    (void)a; (void)b; (void)c; (void)d; (void)e; (void)f;
    int rc = sched_yield();
    return rc < 0 ? -errno : 0;
}
