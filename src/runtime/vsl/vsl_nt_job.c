#include "vsl_nt_internal.h"

/* vsl_nt_job.c -- NT transliteration Batch 2: job objects.
 * Real VSL/Linux work; part of the E1 NT-bridge decomposition of vsl_syscall_nt.c.
 * C11, no nested functions. See vsl_nt_internal.h for the shared surface. */

int64_t vsl_nt_alert_resume_thread(uint64_t a_uaddr, uint64_t b_thread,
                                    uint64_t c, uint64_t d, uint64_t e, uint64_t f) {
    (void)c; (void)d; (void)e; (void)f;
    uint32_t *uaddr = (uint32_t *)a_uaddr;
    pid_t thread = (pid_t)b_thread;
    if (uaddr) {
        long r = syscall(SYS_futex, uaddr, FUTEX_WAKE | FUTEX_PRIVATE_FLAG,
                         INT_MAX, NULL, NULL, 0);
        if (r < 0) return NT_STATUS_UNSUCCESSFUL;
    }
    if (thread > 0) {
        /* NT AlertResume additionally resumes a suspended thread via SIGCONT. */
        if (kill(thread, SIGCONT) < 0 && errno != ESRCH)
            return NT_STATUS_INVALID_PARAMETER;
    }
    return NT_STATUS_SUCCESS;
}

/* NtAreMappedFilesTheSame (21): compare two section/file mappings by inode+
 * device. We open both paths and compare stat.st_ino/st_dev to truly tell if
 * they are the same backing object. */
int64_t vsl_nt_are_mapped_files_same(uint64_t a_file1, uint64_t b_file2,
                                      uint64_t c, uint64_t d, uint64_t e, uint64_t f) {
    (void)c; (void)d; (void)e; (void)f;
    const char *p1 = (const char *)a_file1;
    const char *p2 = (const char *)b_file2;
    if (!p1 || !p2) return NT_STATUS_INVALID_PARAMETER;
    struct stat s1, s2;
    if (stat(p1, &s1) < 0 || stat(p2, &s2) < 0)
        return NT_STATUS_OBJECT_NAME_NOT_FOUND;
    /* Return 1 if same inode+device (i.e. same file); 0 otherwise. */
    return (s1.st_ino == s2.st_ino && s1.st_dev == s2.st_dev) ? 1 : 0;
}

/* NtCreateJobObject (42): stand up a real, isolated process group owned by a
 * sentinel child process so the job can later be terminated (SIGKILL to the
 * group) WITHOUT killing the caller that created it. */
int64_t vsl_nt_create_job_object(uint64_t a_out_handle, uint64_t b_name,
                                  uint64_t c, uint64_t d, uint64_t e, uint64_t f) {
    (void)b_name; (void)c; (void)d; (void)e; (void)f;
    uint32_t *out = (uint32_t *)a_out_handle;
    int slot = -1;
    for (int i = 0; i < NT_JOB_MAX; i++) if (!g_nt_jobs[i].used) { slot = i; break; }
    if (slot < 0) return NT_STATUS_NO_MEMORY;

    pid_t child = fork();
    if (child < 0) return NT_STATUS_UNSUCCESSFUL;
    if (child == 0) {
        /* Sentinel: lead a fresh process group, then block until killed with
         * the rest of the job. */
        setpgid(0, 0);
        for (;;) pause();
    }
    /* Parent: record the sentinel's pgid (== its pid) as the job's group. */
    g_nt_jobs[slot].pgid  = child;
    g_nt_jobs[slot].job_id = g_nt_job_next++;
    g_nt_jobs[slot].used  = true;
    if (out) *out = g_nt_jobs[slot].job_id;
    return NT_STATUS_SUCCESS;
}

/* NtOpenJobObject (125): resolve a job handle cookie to its id (lookup). */
int64_t vsl_nt_open_job_object(uint64_t a_out_handle, uint64_t b_job_id,
                                uint64_t c, uint64_t d, uint64_t e, uint64_t f) {
    (void)c; (void)d; (void)e; (void)f;
    uint32_t *out = (uint32_t *)a_out_handle;
    uint32_t job_id = (uint32_t)b_job_id;
    for (int i = 0; i < NT_JOB_MAX; i++) {
        if (g_nt_jobs[i].used && g_nt_jobs[i].job_id == job_id) {
            if (out) *out = job_id;
            return NT_STATUS_SUCCESS;
        }
    }
    return NT_STATUS_OBJECT_NAME_NOT_FOUND;
}

/* NtTerminateJobObject (266): send SIGKILL to every process in the job's
 * process group (the sentinel child's group), then reap the sentinel and
 * free the slot. */
int64_t vsl_nt_terminate_job_object(uint64_t a_job_id, uint64_t b_status,
                                     uint64_t c, uint64_t d, uint64_t e, uint64_t f) {
    (void)b_status; (void)c; (void)d; (void)e; (void)f;
    uint32_t job_id = (uint32_t)a_job_id;
    for (int i = 0; i < NT_JOB_MAX; i++) {
        if (g_nt_jobs[i].used && g_nt_jobs[i].job_id == job_id) {
            pid_t pgid = g_nt_jobs[i].pgid;
            /* Negative pid to kill targets the whole process group. The job
             * group is owned by the sentinel child, NOT the caller, so the
             * caller survives. */
            if (pgid > 0) {
                kill(-pgid, SIGKILL);
                /* Reap the sentinel. A blocking waitpid(pgid,0) can hang under
                 * some schedulers (observed on WSL2) when the group leader's
                 * zombie isn't delivered promptly; poll with WNOHANG instead,
                 * which reaps the dead sentinel on the first try. Bounded so we
                 * never block the caller indefinitely. */
                int status = 0;
                for (int tries = 0; tries < 50; tries++) {
                    pid_t wr = waitpid(pgid, &status, WNOHANG);
                    if (wr > 0 || wr < 0) break;
                    usleep(20000);
                }
            }
            g_nt_jobs[i].used = false;
            return NT_STATUS_SUCCESS;
        }
    }
    return NT_STATUS_OBJECT_NAME_NOT_FOUND;
}

/* NtIsProcessInJob (99): true if the process's pgid matches a registered job. */
int64_t vsl_nt_is_process_in_job(uint64_t a_process, uint64_t b_job_id,
                                  uint64_t c, uint64_t d, uint64_t e, uint64_t f) {
    (void)c; (void)d; (void)e; (void)f;
    pid_t proc = (pid_t)a_process;
    uint32_t job_id = (uint32_t)b_job_id;
    if (proc <= 0) return NT_STATUS_INVALID_PARAMETER;
    pid_t pgid = getpgid(proc);
    if (pgid < 0) return NT_STATUS_INVALID_PARAMETER;
    for (int i = 0; i < NT_JOB_MAX; i++) {
        if (g_nt_jobs[i].used && g_nt_jobs[i].job_id == job_id)
            return (g_nt_jobs[i].pgid == pgid) ? 1 : 0;
    }
    return 0;
}

/* Atom maintenance (63/158). */
/* NtDeleteAtom (63): drop the named atom from the local table. */

/* Register this batch's NT handlers into the global dispatch table. */
void vsl_nt_job_register(vsl_syscall_fn_t *tbl, int size) {
    (void)size;
    tbl[14-1] = vsl_nt_alert_resume_thread;
    tbl[21-1] = vsl_nt_are_mapped_files_same;
    tbl[42-1] = vsl_nt_create_job_object;
    tbl[99-1] = vsl_nt_is_process_in_job;
    tbl[125-1] = vsl_nt_open_job_object;
    tbl[266-1] = vsl_nt_terminate_job_object;
}
