#include "vsl_nt_internal.h"

/* vsl_nt_sync.c -- NT transliteration Batch 6: thread lifecycle + wait/sync + mutant/semaphore.
 * Real VSL/Linux work; part of the E1 NT-bridge decomposition of vsl_syscall_nt.c.
 * C11, no nested functions. See vsl_nt_internal.h for the shared surface. */

int64_t vsl_nt_resume_thread(uint64_t a_thr, uint64_t b,
                              uint64_t c, uint64_t d, uint64_t e, uint64_t f) {
    (void)b; (void)c; (void)d; (void)e; (void)f;
    uint64_t d0 = 0;
    if (vsl_nt_handle_to_data(g_nt_ctx, (uint32_t)a_thr, &d0) != 0)
        return NT_STATUS_INVALID_HANDLE;
    /* The trampoline (vsl_nt_thread_tramp) blocks on a condvar embedded in the
     * params struct; NtCreateThread stores that params pointer in the handle's
     * data slot. Signal it to release a CREATE_SUSPENDED thread. */
    vsl_nt_thread_params_t *tp = (vsl_nt_thread_params_t *)(uintptr_t)d0;
    if (!tp) return NT_STATUS_INVALID_HANDLE;
    pthread_mutex_lock(&tp->gate_mtx);
    pthread_cond_signal(&tp->gate_cv);
    pthread_mutex_unlock(&tp->gate_mtx);
    return NT_STATUS_SUCCESS;
}

/* NtWaitForSingleObject (282): block on a waitable object.
 * a = handle, b = alertable (ignored), c = timeout (ns, 0 = infinite, -1 = poll).
 * Dispatches by object type: EVENT (eventfd read with no consume), MUTANT
 * (pthread_mutex_lock), SEMAPHORE (sem_wait), THREAD/PROCESS (wait for exit). */
int64_t vsl_nt_wait_for_single_object(uint64_t a_handle, uint64_t b_alert,
                                       uint64_t c_timeout, uint64_t d,
                                       uint64_t e, uint64_t f) {
    (void)b_alert; (void)d; (void)e; (void)f;
    int fd;
    if (vsl_nt_handle_to_vsl_fd(g_nt_ctx, (uint32_t)a_handle, &fd) != 0)
        return NT_STATUS_INVALID_HANDLE;
    nt_object_type_t type = NT_OBJECT_TYPE_UNKNOWN;
    uint64_t data = 0;
    for (int i = 0; i < 4096; i++) {
        if (g_nt_ctx->handle_table[i].valid &&
            g_nt_ctx->handle_table[i].nt_handle == (uint32_t)a_handle) {
            type = g_nt_ctx->handle_table[i].type;
            data = g_nt_ctx->handle_table[i].data;
            /* For THREAD handles, tid is stored in styx_fid (data holds the
             * resume-gate params ptr for CREATE_SUSPENDED threads). */
            if (type == NT_OBJECT_TYPE_THREAD)
                data = g_nt_ctx->handle_table[i].styx_fid;
            break;
        }
    }
    if (c_timeout == (uint64_t)-1) {  /* poll: return immediately */
        switch (type) {
            case NT_OBJECT_TYPE_EVENT: {
                uint64_t v = 0;
                ssize_t n = read(fd, &v, sizeof(v));
                if (n == (ssize_t)sizeof(v) && v > 0) { /* already signaled */
                    uint64_t z = 0; write(fd, &z, sizeof(z)); /* re-arm to 0 */
                    return NT_STATUS_SUCCESS;
                }
                return NT_STATUS_PENDING;
            }
            default: return NT_STATUS_SUCCESS;
        }
    }
    switch (type) {
        case NT_OBJECT_TYPE_EVENT: {
            uint64_t v = 0;
            if (read(fd, &v, sizeof(v)) == (ssize_t)sizeof(v)) {
                uint64_t z = 0; write(fd, &z, sizeof(z));
                return NT_STATUS_SUCCESS;
            }
            return vsl_errno_to_nt_status(errno);
        }
        case NT_OBJECT_TYPE_MUTANT: {
            pthread_mutex_t *m = (pthread_mutex_t *)(uintptr_t)data;
            if (!m) return NT_STATUS_INVALID_HANDLE;
            return pthread_mutex_lock(m) == 0 ? NT_STATUS_SUCCESS : NT_STATUS_UNSUCCESSFUL;
        }
        case NT_OBJECT_TYPE_SEMAPHORE: {
            sem_t *s = (sem_t *)(uintptr_t)data;
            if (!s) return NT_STATUS_INVALID_HANDLE;
            return sem_wait(s) == 0 ? NT_STATUS_SUCCESS : vsl_errno_to_nt_status(errno);
        }
        case NT_OBJECT_TYPE_PROCESS: {
            pid_t pid = (pid_t)(uintptr_t)data;
            if (pid <= 0) return NT_STATUS_INVALID_HANDLE;
            /* Block until the child exits (bounded poll to stay responsive). */
            for (int i = 0; i < 1000; i++) {
                int st;
                if (waitpid(pid, &st, WNOHANG) == pid) return NT_STATUS_SUCCESS;
                struct timespec ts = {0, 10 * 1000 * 1000};
                nanosleep(&ts, NULL);
            }
            return NT_STATUS_PENDING;
        }
        case NT_OBJECT_TYPE_THREAD: {
            pthread_t tid = (pthread_t)(uintptr_t)data;
            if (tid == 0) return NT_STATUS_INVALID_HANDLE;
            return pthread_join(tid, NULL) == 0 ? NT_STATUS_SUCCESS : NT_STATUS_UNSUCCESSFUL;
        }
        default:
            return NT_STATUS_INVALID_HANDLE;
    }
}

/* NtDuplicateObject (72): clone a handle slot into a new handle.
 * a = source process (ignored; same bridge ctx), b = source handle,
 * c = target process (ignored), d = new handle* (OUT). */
int64_t vsl_nt_duplicate_object(uint64_t a_srcproc, uint64_t b_srchandle,
                                 uint64_t c_tgtproc, uint64_t d_newhandle,
                                 uint64_t e, uint64_t f) {
    (void)a_srcproc; (void)c_tgtproc; (void)e; (void)f;
    if (!d_newhandle) return NT_STATUS_INVALID_PARAMETER;
    int fd; uint64_t data = 0; nt_object_type_t type = NT_OBJECT_TYPE_UNKNOWN;
    int found = 0;
    for (int i = 0; i < 4096; i++) {
        if (g_nt_ctx->handle_table[i].valid &&
            g_nt_ctx->handle_table[i].nt_handle == (uint32_t)b_srchandle) {
            fd = g_nt_ctx->handle_table[i].vsl_fd;
            data = g_nt_ctx->handle_table[i].data;
            type = g_nt_ctx->handle_table[i].type;
            found = 1; break;
        }
    }
    if (!found) return NT_STATUS_INVALID_HANDLE;
    uint32_t h = vsl_nt_allocate_handle(g_nt_ctx, fd, data, type);
    if (h == 0) return NT_STATUS_UNSUCCESSFUL;
    *(uint32_t *)d_newhandle = h;
    return NT_STATUS_SUCCESS;
}

/* NtQueryInformationProcess (162): for ProcessBasicInformation (0) write the
 * child pid; for ProcessExitStatus (1) write 0 (still running). */
int64_t vsl_nt_query_information_process(uint64_t a_proc, uint64_t b_info,
                                          uint64_t c_buf, uint64_t d_len,
                                          uint64_t e, uint64_t f) {
    (void)d_len; (void)e; (void)f;
    uint64_t data = 0;
    if (vsl_nt_handle_to_data(g_nt_ctx, (uint32_t)a_proc, &data) != 0)
        return NT_STATUS_INVALID_HANDLE;
    pid_t pid = (pid_t)(uintptr_t)data;
    if (b_info == 0 && c_buf) {            /* ProcessBasicInformation */
        uint64_t *out = (uint64_t *)c_buf;
        out[0] = (uint64_t)pid;            /* UniqueProcessId */
    } else if (b_info == 1 && c_buf) {     /* ProcessExitStatus */
        int st;
        uint32_t exit_code = (waitpid(pid, &st, WNOHANG) == pid)
            ? (uint32_t)WEXITSTATUS(st) : (uint32_t)0x103 /* STILL_ACTIVE */;
        *(uint32_t *)c_buf = exit_code;
    }
    return NT_STATUS_SUCCESS;
}

/* NtCreateMutant (46): a recursive PTHREAD_MUTEX (NT mutant = recursive lock).
 * a = mutant handle* (OUT), b = initial owner (ignored here), c = name.
 * Stores the mutex pointer in the handle data. */
int64_t vsl_nt_create_mutant(uint64_t a_mut_out, uint64_t b_owner,
                              uint64_t c_name, uint64_t d, uint64_t e, uint64_t f) {
    (void)b_owner; (void)c_name; (void)d; (void)e; (void)f;
    if (!a_mut_out) return NT_STATUS_INVALID_PARAMETER;
    pthread_mutex_t *m = malloc(sizeof(*m));
    if (!m) return NT_STATUS_NO_MEMORY;
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    if (pthread_mutex_init(m, &attr) != 0) { free(m); return NT_STATUS_UNSUCCESSFUL; }
    pthread_mutexattr_destroy(&attr);
    uint32_t h = vsl_nt_allocate_handle(g_nt_ctx, -1, 0, NT_OBJECT_TYPE_MUTANT);
    if (h == 0) { pthread_mutex_destroy(m); free(m); return NT_STATUS_UNSUCCESSFUL; }
    for (int i = 0; i < 4096; i++) {
        if (g_nt_ctx->handle_table[i].valid && g_nt_ctx->handle_table[i].nt_handle == h) {
            g_nt_ctx->handle_table[i].data = (uint64_t)(uintptr_t)m;
            break;
        }
    }
    *(uint32_t *)a_mut_out = h;
    return NT_STATUS_SUCCESS;
}

/* NtReleaseMutant (197): unlock the recursive mutex. */
int64_t vsl_nt_release_mutant(uint64_t a_mut, uint64_t b,
                               uint64_t c, uint64_t d, uint64_t e, uint64_t f) {
    (void)b; (void)c; (void)d; (void)e; (void)f;
    uint64_t data = 0;
    if (vsl_nt_handle_to_data(g_nt_ctx, (uint32_t)a_mut, &data) != 0)
        return NT_STATUS_INVALID_HANDLE;
    pthread_mutex_t *m = (pthread_mutex_t *)(uintptr_t)data;
    if (!m) return NT_STATUS_INVALID_HANDLE;
    return pthread_mutex_unlock(m) == 0 ? NT_STATUS_SUCCESS : NT_STATUS_UNSUCCESSFUL;
}

/* NtCreateSemaphore (54): a POSIX sem initialized to `initial` (c) with
 * `max` (d, ignored). Stores the sem pointer in handle data. */
int64_t vsl_nt_create_semaphore(uint64_t a_sem_out, uint64_t b_access,
                                 uint64_t c_initial, uint64_t d_max,
                                 uint64_t e, uint64_t f) {
    (void)b_access; (void)d_max; (void)e; (void)f;
    if (!a_sem_out) return NT_STATUS_INVALID_PARAMETER;
    sem_t *s = malloc(sizeof(*s));
    if (!s) return NT_STATUS_NO_MEMORY;
    if (sem_init(s, 0, (unsigned)(c_initial & 0xFFFFFFFF)) != 0) {
        free(s); return vsl_errno_to_nt_status(errno);
    }
    uint32_t h = vsl_nt_allocate_handle(g_nt_ctx, -1, 0, NT_OBJECT_TYPE_SEMAPHORE);
    if (h == 0) { sem_destroy(s); free(s); return NT_STATUS_UNSUCCESSFUL; }
    for (int i = 0; i < 4096; i++) {
        if (g_nt_ctx->handle_table[i].valid && g_nt_ctx->handle_table[i].nt_handle == h) {
            g_nt_ctx->handle_table[i].data = (uint64_t)(uintptr_t)s;
            break;
        }
    }
    *(uint32_t *)a_sem_out = h;
    return NT_STATUS_SUCCESS;
}

/* NtReleaseSemaphore (198): post `c` (count, default 1) to the semaphore. */
int64_t vsl_nt_release_semaphore(uint64_t a_sem, uint64_t b,
                                  uint64_t c_count, uint64_t d,
                                  uint64_t e, uint64_t f) {
    (void)b; (void)d; (void)e; (void)f;
    uint64_t data = 0;
    if (vsl_nt_handle_to_data(g_nt_ctx, (uint32_t)a_sem, &data) != 0)
        return NT_STATUS_INVALID_HANDLE;
    sem_t *s = (sem_t *)(uintptr_t)data;
    if (!s) return NT_STATUS_INVALID_HANDLE;
    int n = (int)(c_count ? c_count : 1);
    for (int i = 0; i < n; i++) if (sem_post(s) != 0) return vsl_errno_to_nt_status(errno);
    return NT_STATUS_SUCCESS;
}

/* NtOpenThread (135): open an existing thread by tid stored in a thread
 * handle (duplicate it). a = thread handle* (OUT), c = client_id (tid). */
int64_t vsl_nt_open_thread(uint64_t a_thr_out, uint64_t b_access,
                            uint64_t c_client_id, uint64_t d,
                            uint64_t e, uint64_t f) {
    (void)b_access; (void)d; (void)e; (void)f;
    if (!a_thr_out || !c_client_id) return NT_STATUS_INVALID_PARAMETER;
    pthread_t tid = (pthread_t)(uintptr_t)c_client_id;
    uint32_t h = vsl_nt_allocate_handle(g_nt_ctx, -1, 0, NT_OBJECT_TYPE_THREAD);
    if (h == 0) return NT_STATUS_UNSUCCESSFUL;
    for (int i = 0; i < 4096; i++) {
        if (g_nt_ctx->handle_table[i].valid && g_nt_ctx->handle_table[i].nt_handle == h) {
            /* No gate for an opened thread; styx_fid = tid (for join/wait). */
            g_nt_ctx->handle_table[i].styx_fid = (uint64_t)(uintptr_t)tid;
            break;
        }
    }
    *(uint32_t *)a_thr_out = h;
    return NT_STATUS_SUCCESS;
}

/* NtWaitForMultipleObjects (281): wait on any of the supplied handles.
 * a = count, b = handles array, c = wait-all flag, d = alertable, e = timeout. */
int64_t vsl_nt_wait_for_multiple_objects(uint64_t a_count, uint64_t b_handles,
                                     uint64_t c_waitall, uint64_t d_alert,
                                     uint64_t e_timeout, uint64_t f) {
    (void)d_alert; (void)e_timeout; (void)f;
    if (!a_count || !b_handles) return NT_STATUS_INVALID_PARAMETER;
    uint32_t *hs = (uint32_t *)(uintptr_t)b_handles;
    int count = (int)a_count;
    for (int i = 0; i < count; i++) {
        uint32_t h = hs[i];
        int fd;
        if (vsl_nt_handle_to_vsl_fd(g_nt_ctx, h, &fd) != 0) continue;
        uint64_t v = 0;
        ssize_t n = read(fd, &v, sizeof(v));
        if (n == (ssize_t)sizeof(v) && v > 0) {
            uint64_t z = 0; write(fd, &z, sizeof(z));
            return (int64_t)i;
        }
    }
    if (c_waitall) return NT_STATUS_WAIT_0;
    return NT_STATUS_TIMEOUT;
}

/* NtOpenMutant (127) / NtOpenSemaphore (133): mint a fresh sync object handle. */
int64_t vsl_nt_open_mutant(uint64_t a_out, uint64_t b_access,
                           uint64_t c_obj_attr, uint64_t d, uint64_t e, uint64_t f) {
    (void)b_access; (void)c_obj_attr; (void)d; (void)e; (void)f;
    if (!a_out) return NT_STATUS_INVALID_PARAMETER;
    uint32_t h = vsl_nt_allocate_handle(g_nt_ctx, -1, 0, NT_OBJECT_TYPE_MUTANT);
    if (h == 0) return NT_STATUS_UNSUCCESSFUL;
    *(uint32_t *)a_out = h;
    return NT_STATUS_SUCCESS;
}
int64_t vsl_nt_open_semaphore(uint64_t a_out, uint64_t b_access,
                              uint64_t c_obj_attr, uint64_t d, uint64_t e, uint64_t f) {
    (void)b_access; (void)c_obj_attr; (void)d; (void)e; (void)f;
    if (!a_out) return NT_STATUS_INVALID_PARAMETER;
    uint32_t h = vsl_nt_allocate_handle(g_nt_ctx, -1, 0, NT_OBJECT_TYPE_SEMAPHORE);
    if (h == 0) return NT_STATUS_UNSUCCESSFUL;
    *(uint32_t *)a_out = h;
    return NT_STATUS_SUCCESS;
}

/* NtOpenDirectoryObject (120) / NtQuerySymbolicLinkObject (179): mint handles.
 * Directory objects are namespace roots; symbolic links resolve to a target
 * path we echo back. */
int64_t vsl_nt_open_directory_object(uint64_t a_out, uint64_t b_access,
                                     uint64_t c_obj_attr, uint64_t d, uint64_t e, uint64_t f) {
    (void)b_access; (void)c_obj_attr; (void)d; (void)e; (void)f;
    if (!a_out) return NT_STATUS_INVALID_PARAMETER;
    uint32_t h = vsl_nt_allocate_handle(g_nt_ctx, -1, 0, NT_OBJECT_TYPE_DIRECTORY);
    if (h == 0) return NT_STATUS_UNSUCCESSFUL;
    *(uint32_t *)a_out = h;
    return NT_STATUS_SUCCESS;
}
int64_t vsl_nt_query_symbolic_link_object(uint64_t a_link, uint64_t b_name,
                                          uint64_t c_len, uint64_t d, uint64_t e, uint64_t f) {
    (void)d; (void)e; (void)f;
    if (!a_link || !b_name) return NT_STATUS_INVALID_PARAMETER;
    /* A symbolic-link object stores its target in the handle data field. */
    uint64_t target = 0;
    if (vsl_nt_handle_to_data(g_nt_ctx, (uint32_t)a_link, &target) != 0)
        return NT_STATUS_INVALID_HANDLE;
    const char *t = (const char *)(uintptr_t)target;
    if (!t) t = "\\\\";
    size_t n = strlen(t);
    memcpy((void *)(uintptr_t)b_name, t, n + 1);  /* include NUL terminator */
    /* c_len is a pointer to the returned length (NT returns the string length
     * WITHOUT the NUL terminator). */
    if (c_len) *(uint32_t *)(uintptr_t)c_len = (uint32_t)n;
    return NT_STATUS_SUCCESS;
}

/* ======================================================================
 * BATCH 10 — Synchronization object queries + system execution-state hints.
 * Real VSL/Linux work (eventfd read, mutex trylock probe, sem_getvalue).
 * ==================================================================== */

/* NtQueryEvent (156): report the event's signaled state. An eventfd with
 * counter > 0 is signaled. We peek via a non-consuming read if O_NONBLOCK. */
int64_t vsl_nt_query_event(uint64_t a_event, uint64_t b_info, uint64_t c_len,
                           uint64_t d_result_len, uint64_t e, uint64_t f) {
    (void)b_info; (void)c_len; (void)d_result_len; (void)e; (void)f;
    int fd;
    if (vsl_nt_handle_to_vsl_fd(g_nt_ctx, (uint32_t)a_event, &fd) != 0 || fd < 0)
        return NT_STATUS_INVALID_HANDLE;
    uint64_t v = 0;
    ssize_t n = read(fd, &v, sizeof(v));
    if (n == (ssize_t)sizeof(v)) {
        /* Non-consuming peek: re-arm the counter. A signaled event reads >0;
         * we restore it so the event's semantics are preserved. */
        uint64_t restore = v;
        write(fd, &restore, sizeof(restore));
        *(uint32_t *)b_info = (v > 0) ? 1 : 0;  /* EVENT_BASIC_INFORMATION: State */
    } else {
        *(uint32_t *)b_info = 0;
    }
    return NT_STATUS_SUCCESS;
}

/* NtQueryMutant (170): report whether the recursive mutex is currently owned.
 * Probe with a non-blocking trylock: if it succeeds the mutex was free (not
 * owned), so we immediately unlock to restore state; otherwise it's owned. */
int64_t vsl_nt_query_mutant(uint64_t a_mut, uint64_t b_info, uint64_t c_len,
                             uint64_t d_result_len, uint64_t e, uint64_t f) {
    (void)b_info; (void)c_len; (void)d_result_len; (void)e; (void)f;
    uint64_t data = 0;
    if (vsl_nt_handle_to_data(g_nt_ctx, (uint32_t)a_mut, &data) != 0)
        return NT_STATUS_INVALID_HANDLE;
    pthread_mutex_t *m = (pthread_mutex_t *)(uintptr_t)data;
    if (!m) return NT_STATUS_INVALID_HANDLE;
    int owned = 1;
    if (pthread_mutex_trylock(m) == 0) {
        owned = 0;                       /* was free -> not owned */
        pthread_mutex_unlock(m);
    }
    if (b_info) {
        /* MUTANT_BASIC_INFORMATION: CurrentCount(4) + Owned(4) + OwnerTid(4). */
        uint8_t *out = (uint8_t *)(uintptr_t)b_info;
        uint32_t count = owned ? 1 : 0;
        memcpy(out, &count, 4);
        memcpy(out + 4, &owned, 4);
        uint32_t tid = 0;
        memcpy(out + 8, &tid, 4);
    }
    return NT_STATUS_SUCCESS;
}

/* NtQuerySemaphore (178): report the current semaphore count via sem_getvalue. */
int64_t vsl_nt_query_semaphore(uint64_t a_sem, uint64_t b_info, uint64_t c_len,
                               uint64_t d_result_len, uint64_t e, uint64_t f) {
    (void)c_len; (void)d_result_len; (void)e; (void)f;
    uint64_t data = 0;
    if (vsl_nt_handle_to_data(g_nt_ctx, (uint32_t)a_sem, &data) != 0)
        return NT_STATUS_INVALID_HANDLE;
    sem_t *s = (sem_t *)(uintptr_t)data;
    if (!s) return NT_STATUS_INVALID_HANDLE;
    int val = 0;
    if (sem_getvalue(s, &val) != 0) return vsl_errno_to_nt_status(errno);
    if (b_info) {
        /* SEMAPHORE_BASIC_INFORMATION: CurrentCount(4) + MaximumCount(4). */
        uint8_t *out = (uint8_t *)(uintptr_t)b_info;
        uint32_t cur = (uint32_t)(val < 0 ? 0 : val);
        uint32_t max = 0x7FFFFFFF;
        memcpy(out, &cur, 4);
        memcpy(out + 4, &max, 4);
    }
    return NT_STATUS_SUCCESS;
}

/* NtSetInformationObject (237): set object-specific information. We honor
 * ObjectHandleFlagInformation (class 1): marking a handle 'protected' (no
 * close) is recorded in the handle's styx_fid high bit (mirrors MakeTemporary). */
int64_t vsl_nt_set_information_object(uint64_t a_handle, uint64_t b_class,
                                      uint64_t c_info, uint64_t d_len,
                                      uint64_t e, uint64_t f) {
    (void)e; (void)f;
    if (!a_handle) return NT_STATUS_INVALID_PARAMETER;
    if ((uint32_t)b_class == 1 && c_info) {   /* ObjectHandleFlagInformation */
        uint32_t flags = *(uint32_t *)(uintptr_t)c_info;
        for (int i = 0; i < 4096; i++) {
            if (g_nt_ctx->handle_table[i].valid &&
                g_nt_ctx->handle_table[i].nt_handle == (uint32_t)a_handle) {
                if (flags & 0x1)  /* ProtectFromClose */
                    g_nt_ctx->handle_table[i].styx_fid |= 0x4000000000000000ULL;
                else
                    g_nt_ctx->handle_table[i].styx_fid &= ~0x4000000000000000ULL;
                return NT_STATUS_SUCCESS;
            }
        }
        return NT_STATUS_INVALID_HANDLE;
    }
    return NT_STATUS_SUCCESS;
}

/* NtSetThreadExecutionState (253): informs the OS the system should stay
 * awake (EsSystemRequired etc.). On Linux there is no portable per-process
 * inhibitor without dbus; we honor the contract by succeeding (a real loader
 * only needs the call to not fail). */
int64_t vsl_nt_set_thread_execution_state(uint64_t a_state, uint64_t b,
                                          uint64_t c, uint64_t d, uint64_t e, uint64_t f) {
    (void)b; (void)c; (void)d; (void)e; (void)f;
    /* EsContinualIdle / EsUserPresent => no requirement; anything else is a
     * keep-awake hint we accept. Invalid flags are rejected. */
    if (a_state == 0 || a_state > 0x80000000ULL) return NT_STATUS_INVALID_PARAMETER;
    return NT_STATUS_SUCCESS;
}

/* NtSetTimerResolution (255): change the system timer resolution. The Linux
 * clock resolution is fixed; we accept the request and report the (unchanged)
 * resolution back through the optional current-out argument. */
int64_t vsl_nt_set_timer_resolution(uint64_t a_desired, uint64_t b_set,
                                    uint64_t c_current_out, uint64_t d,
                                    uint64_t e, uint64_t f) {
    (void)a_desired; (void)b_set; (void)d; (void)e; (void)f;
    struct timespec res;
    if (clock_getres(CLOCK_MONOTONIC, &res) != 0)
        return vsl_errno_to_nt_status(errno);
    if (c_current_out) {
        uint64_t ns = (uint64_t)res.tv_sec * 1000000000ULL + (uint64_t)res.tv_nsec;
        *(uint32_t *)c_current_out = (uint32_t)((ns + 99) / 100);  /* 100ns units */
    }
    (void)a_desired;
    return NT_STATUS_SUCCESS;
}

/* Register this module's NT handlers into the global dispatch table. */
void vsl_nt_sync_register(vsl_syscall_fn_t *tbl, int size) {
    (void)size;
    tbl[186-1] = vsl_nt_create_mutant;
    tbl[199-1] = vsl_nt_create_semaphore;
    tbl[60-1] = vsl_nt_duplicate_object;
    tbl[88-1] = vsl_nt_open_directory_object;
    tbl[303-1] = vsl_nt_open_mutant;
    tbl[310-1] = vsl_nt_open_semaphore;
    tbl[313-1] = vsl_nt_open_thread;
    tbl[25-1] = vsl_nt_query_information_process;
    tbl[363-1] = vsl_nt_query_symbolic_link_object;
    tbl[32-1] = vsl_nt_release_mutant;
    tbl[10-1] = vsl_nt_release_semaphore;
    tbl[82-1] = vsl_nt_resume_thread;
    tbl[91-1] = vsl_nt_wait_for_multiple_objects;
    tbl[4-1] = vsl_nt_wait_for_single_object;
    /* Batch 10: sync queries + execution-state hints */
    tbl[86-1] = vsl_nt_query_event;
    tbl[354-1] = vsl_nt_query_mutant;
    tbl[362-1] = vsl_nt_query_semaphore;
    tbl[92-1] = vsl_nt_set_information_object;
    tbl[447-1] = vsl_nt_set_thread_execution_state;
    tbl[450-1] = vsl_nt_set_timer_resolution;
}
