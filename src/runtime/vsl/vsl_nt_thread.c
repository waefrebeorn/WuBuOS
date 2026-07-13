/*
 * vsl_nt_thread.c -- WuBuOS NT transliteration: Thread lifecycle.
 *
 * Split from vsl_nt_proc.c (which mixed 5 NT subsystems in one TU).
 * Self-contained: real VSL/Linux work; dispatched via vsl_nt_thread_register().
 * C11, opaque structs, minimal includes -- shares the vsl_nt_internal.h surface.
 */

#include "vsl_nt_internal.h"

/* Shared trampoline for NtCreateThread (defined here, declared in vsl_nt_internal.h). */
void *vsl_nt_thread_tramp(void *p) {
    vsl_nt_thread_params_t *tp = (vsl_nt_thread_params_t *)p;
    void *(*start)(void *) = tp->start;
    void *arg = tp->arg;
    if (tp->suspended) {
        pthread_mutex_lock(&tp->gate_mtx);
        pthread_cond_wait(&tp->gate_cv, &tp->gate_mtx);
        pthread_mutex_unlock(&tp->gate_mtx);
    }
    pthread_mutex_destroy(&tp->gate_mtx);
    pthread_cond_destroy(&tp->gate_cv);
    free(tp);
    return start ? start(arg) : NULL;
}


int64_t vsl_nt_create_thread(uint64_t a_thr_out, uint64_t b_access,
                              uint64_t c_objattr, uint64_t d_proc,
                              uint64_t e_start, uint64_t f_arg) {
    (void)b_access; (void)c_objattr; (void)d_proc;
    if (!a_thr_out || !e_start) return NT_STATUS_INVALID_PARAMETER;
    vsl_nt_thread_params_t *tp = malloc(sizeof(*tp));
    if (!tp) return NT_STATUS_NO_MEMORY;
    tp->start = (void *(*)(void *))(uintptr_t)e_start;
    tp->arg   = (void *)(uintptr_t)f_arg;
    /* CREATE_SUSPENDED (0x4 in NT create flags) — block until NtResumeThread. */
    tp->suspended = (b_access & 0x4) ? 1 : 0;
    pthread_mutex_init(&tp->gate_mtx, NULL);
    pthread_cond_init(&tp->gate_cv, NULL);
    pthread_t tid;
    if (pthread_create(&tid, NULL, vsl_nt_thread_tramp, tp) != 0) {
        pthread_mutex_destroy(&tp->gate_mtx);
        pthread_cond_destroy(&tp->gate_cv);
        free(tp);
        return NT_STATUS_UNSUCCESSFUL;
    }
    uint32_t h = vsl_nt_allocate_handle(g_nt_ctx, -1, 0, NT_OBJECT_TYPE_THREAD);
    if (h == 0) { pthread_detach(tid); free(tp); return NT_STATUS_UNSUCCESSFUL; }
    for (int i = 0; i < 4096; i++) {
        if (g_nt_ctx->handle_table[i].valid && g_nt_ctx->handle_table[i].nt_handle == h) {
            /* data = params ptr (for NtResumeThread gate); styx_fid = tid (for join). */
            g_nt_ctx->handle_table[i].data = (uint64_t)(uintptr_t)tp;
            g_nt_ctx->handle_table[i].styx_fid = (uint64_t)(uintptr_t)tid;
            break;
        }
    }
    *(uint32_t *)a_thr_out = h;
    return NT_STATUS_SUCCESS;
}

/* NtCreateProcess (50): fork a real child process and track it.
 * a = process handle* (OUT), f = section_handle (image; ignored — we model a
 * placeholder process the launcher can later NtAllocateVirtualMemory /
 * NtCreateThread inside, mirroring how Proton boots a wrapper process before
 * the real exe image is mapped). Returns SUCCESS; the child pid is stored. */

int64_t vsl_nt_suspend_thread(uint64_t a_thr, uint64_t b, uint64_t c,
                              uint64_t d, uint64_t e, uint64_t f) {
    (void)b; (void)c; (void)d; (void)e; (void)f;
    uint64_t tid = 0;
    if (vsl_nt_handle_to_data(g_nt_ctx, (uint32_t)a_thr, &tid) != 0)
        return NT_STATUS_INVALID_HANDLE;
    /* For THREAD handles tid is in styx_fid; for the resume-gate case data holds
     * the params ptr. Prefer styx_fid. */
    for (int i = 0; i < 4096; i++)
        if (g_nt_ctx->handle_table[i].valid &&
            g_nt_ctx->handle_table[i].nt_handle == (uint32_t)a_thr) {
            tid = g_nt_ctx->handle_table[i].styx_fid; break;
        }
    if (tid == 0) return NT_STATUS_INVALID_HANDLE;
    if (tgkill((pid_t)getpid(), (pid_t)tid, SIGSTOP) != 0)
        return vsl_errno_to_nt_status(errno);
    return NT_STATUS_SUCCESS;
}

/* NtTerminateThread (268): pthread_cancel + detached join, or thr_exit. */

int64_t vsl_nt_terminate_thread(uint64_t a_thr, uint64_t b_status,
                                uint64_t c, uint64_t d, uint64_t e, uint64_t f) {
    (void)b_status; (void)c; (void)d; (void)e; (void)f;
    uint64_t tid = 0;
    for (int i = 0; i < 4096; i++)
        if (g_nt_ctx->handle_table[i].valid &&
            g_nt_ctx->handle_table[i].nt_handle == (uint32_t)a_thr) {
            tid = g_nt_ctx->handle_table[i].styx_fid; break;
        }
    if (tid == 0) return NT_STATUS_INVALID_HANDLE;
    if (pthread_cancel((pthread_t)tid) != 0)
        return NT_STATUS_UNSUCCESSFUL;
    return NT_STATUS_SUCCESS;
}

/* NtQueryInformationThread (163): report tid/exit-state basics. */

int64_t vsl_nt_query_information_thread(uint64_t a_thr, uint64_t b_class,
                                        uint64_t c_info, uint64_t d_len,
                                        uint64_t e, uint64_t f) {
    (void)b_class; (void)d_len; (void)e; (void)f;
    if (!a_thr || !c_info) return NT_STATUS_INVALID_PARAMETER;
    uint64_t tid = 0;
    for (int i = 0; i < 4096; i++)
        if (g_nt_ctx->handle_table[i].valid &&
            g_nt_ctx->handle_table[i].nt_handle == (uint32_t)a_thr) {
            tid = g_nt_ctx->handle_table[i].styx_fid; break;
        }
    if (tid == 0) return NT_STATUS_INVALID_HANDLE;
    /* THREAD_BASIC_INFORMATION: exit-status(4) tid(8) ... */
    uint8_t *out = (uint8_t *)(uintptr_t)c_info;
    memset(out, 0, 24);
    memcpy(out + 8, &tid, 8);
    return NT_STATUS_SUCCESS;
}

/* NtSetInformationProcess (238): honor ProcessBreakAway(7)/Process priority. */

int64_t vsl_nt_set_information_thread(uint64_t a_thr, uint64_t b_class,
                                      uint64_t c_info, uint64_t d_len,
                                      uint64_t e, uint64_t f) {
    (void)c_info; (void)d_len; (void)e; (void)f;
    if ((uint32_t)b_class == 0x20 /* ThreadPriority */ && c_info) {
        uint32_t pri = *(uint32_t *)c_info;
        uint64_t tid = 0;
        for (int i = 0; i < 4096; i++)
            if (g_nt_ctx->handle_table[i].valid &&
                g_nt_ctx->handle_table[i].nt_handle == (uint32_t)a_thr) {
                tid = g_nt_ctx->handle_table[i].styx_fid; break;
            }
        if (tid) {
            int nice = (pri > 15) ? -10 : (pri < 8 ? 10 : 0);
            setpriority(PRIO_PROCESS, (id_t)tid, nice);
        }
    }
    return NT_STATUS_SUCCESS;
}

/* NtCreateTimer (57) / NtSetTimer (254) / NtCancelTimer (26) / NtOpenTimer (138):
 * backed by Linux timerfd. a = handle* (out), d = timerfd stored in data. */

int64_t vsl_nt_yield_execution(uint64_t a, uint64_t b, uint64_t c,
                               uint64_t d, uint64_t e, uint64_t f) {
    (void)a; (void)b; (void)c; (void)d; (void)e; (void)f;
    sched_yield();
    return NT_STATUS_SUCCESS;
}

/* NtWriteVirtualMemory (195): write bytes into a process's address space.
 * a = process handle, b = base address, c = buffer, d = size*, e = bytes_written*.
 * Uses process_vm_writev for genuine cross-process writes. */

/* Register this module's NT handlers into the global dispatch table. */
void vsl_nt_thread_register(vsl_syscall_fn_t *tbl, int size) {
    (void)size;
    tbl[56-1] = vsl_nt_create_thread;
    tbl[163-1] = vsl_nt_query_information_thread;
    tbl[264-1] = vsl_nt_suspend_thread;
    tbl[268-1] = vsl_nt_terminate_thread;
    tbl[239-1] = vsl_nt_set_information_thread;
    tbl[289-1] = vsl_nt_yield_execution;
}
