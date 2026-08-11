/*
 * wubu_secmon.c — the kernel's syscall camera (call interception for the AGI).
 *
 * Implements the ptrace-syscall supervisor declared in wubu_secmon.h.
 * Every syscall the traced process makes trips a PTRACE_SYSCALL stop:
 * the monitor records (kind, nr, args) and streams it into the KV-FS
 * at /kv/agent/sys_<pid>/<seq> so the Brain (wubuwizard) can read the
 * game's full behavior stream over 9P.
 *
 * This is the interception layer the user demanded: wine/proton run on
 * our kernel, and the AGI sees every call they make.
 *
 * C11, opaque struct, minimal includes.
 */
#include "wubu_secmon.h"
#include "../kernel/wubu_kvfs.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <signal.h>
#ifndef SIGTRAP
#define SIGTRAP 5
#endif
#include <sys/ptrace.h>
#include <sys/user.h>
#include <sys/wait.h>
#include <sys/types.h>

struct wubu_secmon {
    pid_t       pid;            /* traced child (0 = none) */
    int         realm_id;
    uint64_t    n_captured;     /* total syscall events seen */
    uint64_t    n_kv_writes;    /* spans written to KV-FS */
    uint64_t    seq;            /* per-pid sequence */
    bool        kv_stream;      /* write into KV-FS? */
    bool        attached;
    char        kv_base_path[96]; /* /kv/agent/sys_<pid>/ */
};

/* Path prefix for the KV mount region that holds agent traces.
 * wubu_kvfs_namespace_init() mounts /kv/agent at blocks 2048..4095. */
#define KV_AGENT_PATH "/kv/agent"

wubu_secmon_t *wubu_secmon_create(int realm_id)
{
    wubu_secmon_t *m = calloc(1, sizeof(*m));
    if (!m) return NULL;
    m->realm_id = realm_id;
    m->kv_stream = true;
    return m;
}

void wubu_secmon_destroy(wubu_secmon_t *m)
{
    if (!m) return;
    if (m->attached && m->pid > 0) {
        /* best-effort detach: let the child continue */
        ptrace(PTRACE_DETACH, m->pid, NULL, NULL);
    }
    free(m);
}

/* ---- KV-FS write path ---- */
static void secmon_kv_write(wubu_secmon_t *m, int kind, long nr,
                            long a0, long a1, long a2, long ret)
{
    if (!m->kv_stream) return;
    if (!g_wubu_kvfs || !g_wubu_kv_base) return;   /* KV-FS not initialized */

    char path[128];
    float vec[6];
    snprintf(path, sizeof(path), "%s/sys_%d/%06llu",
             KV_AGENT_PATH, (int)m->pid, (unsigned long long)m->seq++);
    vec[0] = (float)kind;
    vec[1] = (float)nr;
    vec[2] = (float)a0;
    vec[3] = (float)a1;
    vec[4] = (float)(kind == 0 ? a2 : ret);
    vec[5] = (float)m->pid;

    int rc = wubu_kvfs_write(g_wubu_kvfs, path, g_wubu_kv_base, vec, 6);
    if (rc == 0) m->n_kv_writes++;
}

int wubu_secmon_attach(wubu_secmon_t *m, pid_t pid)
{
    if (!m) return -1;
    if (m->attached) wubu_secmon_detach(m);

    if (ptrace(PTRACE_ATTACH, pid, NULL, NULL) != 0) {
        return -1;  /* errno preserved */
    }

    int status = 0;
    waitpid(pid, &status, 0);
    if (!WIFSTOPPED(status)) {
        ptrace(PTRACE_DETACH, pid, NULL, NULL);
        return -1;
    }

    /* Enter syscall tracing. */
    if (ptrace(PTRACE_SYSCALL, pid, NULL, NULL) != 0) {
        ptrace(PTRACE_DETACH, pid, NULL, NULL);
        return -1;
    }

    m->pid = pid;
    m->attached = true;
    m->seq = 0;
    snprintf(m->kv_base_path, sizeof(m->kv_base_path), "%s/sys_%d", KV_AGENT_PATH, (int)pid);
    return 0;
}

int wubu_secmon_poll(wubu_secmon_t *m)
{
    if (!m || !m->attached || m->pid <= 0) return -1;

    int status = 0;
    pid_t r = waitpid(m->pid, &status, __WALL | WNOHANG);
    if (r == 0) return 0;             /* nothing pending */
    if (r < 0) return -1;
    if (WIFEXITED(status) || WIFSIGNALED(status)) {
        /* child done — capture complete */
        m->attached = false;
        return -1;
    }

    if (WIFSTOPPED(status)) {
        int sig = WSTOPSIG(status);
        if (sig == SIGTRAP) {
            /* Syscall entry/exit stop. */
            struct user_regs_struct regs;
            if (ptrace(PTRACE_GETREGS, m->pid, NULL, &regs) == 0) {
                /* On x86_64: entry stops have orig_rax = syscall nr,
                 * exit stops have orig_rax = -1. */
                long nr = (long)regs.orig_rax;
                if (nr >= 0) {
                    /* syscall ENTRY */
                    secmon_kv_write(m, 0, nr,
                                    (long)regs.rdi, (long)regs.rsi,
                                    (long)regs.rdx, 0);
                    m->n_captured++;
                } else {
                    /* syscall EXIT */
                    secmon_kv_write(m, 1, (long)regs.rax, 0, 0, 0,
                                    (long)regs.rax);
                    m->n_captured++;
                }
            }
        }
        /* Resume tracing. */
        ptrace(PTRACE_SYSCALL, m->pid, NULL, (void *)(sig == SIGTRAP ? 0 : sig));
        return 1;
    }
    return 0;
}

int wubu_secmon_wait(wubu_secmon_t *m)
{
    if (!m || !m->attached) return 0;
    uint64_t start = m->n_captured;
    for (;;) {
        int rc = wubu_secmon_poll(m);
        if (rc < 0) break;          /* child exited */
        if (rc == 0) usleep(1000);  /* nothing pending — wait */
    }
    return (int)(m->n_captured - start);
}

int wubu_secmon_detach(wubu_secmon_t *m)
{
    if (!m || !m->attached) return 0;
    ptrace(PTRACE_DETACH, m->pid, NULL, NULL);
    m->attached = false;
    m->pid = 0;
    return 0;
}

uint64_t wubu_secmon_count(const wubu_secmon_t *m) { return m ? m->n_captured : 0; }
uint64_t wubu_secmon_kv_writes(const wubu_secmon_t *m) { return m ? m->n_kv_writes : 0; }

void wubu_secmon_set_kv_stream(wubu_secmon_t *m, bool on)
{
    if (m) m->kv_stream = on;
}
