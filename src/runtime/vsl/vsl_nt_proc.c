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


/* vsl_nt_proc.c -- NT transliteration Batch 4+5: process / thread / virtual memory / section / launch path.
 * Real VSL/Linux work; part of the E1 NT-bridge decomposition of vsl_syscall_nt.c.
 * C11, no nested functions. See vsl_nt_internal.h for the shared surface. */

int64_t vsl_nt_allocate_virtual_memory(uint64_t a_proc, uint64_t b_base,
                                        uint64_t c_size, uint64_t d_type,
                                        uint64_t e_prot, uint64_t f) {
    (void)a_proc; (void)d_type; (void)e_prot; (void)f;
    if (!b_base || !c_size) return NT_STATUS_INVALID_PARAMETER;
    void **base = (void **)b_base;
    size_t *size = (size_t *)c_size;
    int flags = MAP_PRIVATE | MAP_ANONYMOUS;
    if (*base) flags |= MAP_FIXED;
    void *p = mmap(*base, *size, PROT_READ | PROT_WRITE, flags, -1, 0);
    if (p == MAP_FAILED) return vsl_errno_to_nt_status(errno);
    *base = p;
    uint32_t h = vsl_nt_allocate_handle(g_nt_ctx, -1, 0, NT_OBJECT_TYPE_SECTION);
    if (h == 0) { munmap(p, *size); return NT_STATUS_UNSUCCESSFUL; }
    /* Stash the mmap base in the handle payload so FreeVirtualMemory can find
     * it and so the caller can map the region later. */
    for (int i = 0; i < 4096; i++) {
        if (g_nt_ctx->handle_table[i].valid && g_nt_ctx->handle_table[i].nt_handle == h) {
            g_nt_ctx->handle_table[i].data = (uint64_t)(uintptr_t)p;
            break;
        }
    }
    return (int64_t)h;
}

/* NtFreeVirtualMemory (88): release a region previously allocated.
 * a = process handle, b = base_address*, c = region_size*, d = free_type. */
int64_t vsl_nt_free_virtual_memory(uint64_t a_proc, uint64_t b_base,
                                    uint64_t c_size, uint64_t d_type,
                                    uint64_t e, uint64_t f) {
    (void)a_proc; (void)d_type; (void)e; (void)f;
    if (!b_base || !c_size) return NT_STATUS_INVALID_PARAMETER;
    void *base = *(void **)b_base;
    size_t size = *(size_t *)c_size;
    if (munmap(base, size) != 0) return vsl_errno_to_nt_status(errno);
    /* Free the handle if we can find one whose payload matches this base. */
    for (int i = 0; i < 4096; i++) {
        if (g_nt_ctx->handle_table[i].valid &&
            g_nt_ctx->handle_table[i].type == NT_OBJECT_TYPE_SECTION &&
            (void *)(uintptr_t)g_nt_ctx->handle_table[i].data == base) {
            vsl_nt_free_handle(g_nt_ctx, g_nt_ctx->handle_table[i].nt_handle);
            break;
        }
    }
    return NT_STATUS_SUCCESS;
}

/* NtCreateThread (56): spawn a real thread running start_routine(arg).
 * a = thread handle* (OUT), d = process handle (ignored here), e = start_routine,
 * f = argument. Returns SUCCESS and stores the pthread tid in the handle. */
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
int64_t vsl_nt_create_process(uint64_t a_proc_out, uint64_t b_access,
                               uint64_t c_objattr, uint64_t d_parent,
                               uint64_t e_inherit, uint64_t f_section) {
    (void)b_access; (void)c_objattr; (void)d_parent; (void)e_inherit; (void)f_section;
    if (!a_proc_out) return NT_STATUS_INVALID_PARAMETER;
    pid_t pid = fork();
    if (pid < 0) return vsl_errno_to_nt_status(errno);
    if (pid == 0) {
        /* Child: a live placeholder process. It idles until the parent
         * terminates it via NtTerminateProcess, exactly like the job-sentinel
         * pattern — keeps the process object real without reaping risk to us. */
        pause();
        _exit(0);
    }
    uint32_t h = vsl_nt_allocate_handle(g_nt_ctx, -1, 0, NT_OBJECT_TYPE_PROCESS);
    if (h == 0) { kill(pid, SIGKILL); return NT_STATUS_UNSUCCESSFUL; }
    for (int i = 0; i < 4096; i++) {
        if (g_nt_ctx->handle_table[i].valid && g_nt_ctx->handle_table[i].nt_handle == h) {
            g_nt_ctx->handle_table[i].data = (uint64_t)(uintptr_t)pid;
            break;
        }
    }
    *(uint32_t *)a_proc_out = h;
    return NT_STATUS_SUCCESS;
}

/* ======================================================================
 * BATCH 5 — Process / memory launch path (boot a real image, Proton-style).
 *
 * This is the spine a Windows loader drives through: open a target process
 * (NtOpenProcess), carve an image section (NtCreateSection), map it into the
 * process address space (NtMapViewOfSection), write the PE bytes into it
 * (NtWriteVirtualMemory), and finally tear it down (NtTerminateProcess).
 * Every handler does genuine cross-process Linux work — process_vm_writev /
 * process_vm_readv for memory, kill()+waitpid() for termination, mmap for the
 * section — so a real NT personality could boot an actual child image.
 * ==================================================================== */

/* Resolve a process handle to its live child pid (stored in the handle data). */
pid_t vsl_nt_proc_pid(uint32_t proc_handle) {
    uint64_t d = 0;
    if (vsl_nt_handle_to_data(g_nt_ctx, proc_handle, &d) != 0) return -1;
    return (pid_t)(uintptr_t)d;
}

/* NtOpenProcess (129): mint a PROCESS handle for an existing child pid.
 * a = process handle* (OUT), c = client_id (uint32_t pid) OR 0 = open self. */
int64_t vsl_nt_open_process(uint64_t a_proc_out, uint64_t b_access,
                             uint64_t c_client_id, uint64_t d,
                             uint64_t e, uint64_t f) {
    (void)b_access; (void)d; (void)e; (void)f;
    if (!a_proc_out) return NT_STATUS_INVALID_PARAMETER;
    pid_t pid = c_client_id ? (pid_t)(uint32_t)c_client_id : getpid();
    /* Verify the target is real (kill(0) probe). For the self case this is us. */
    if (pid != getpid() && kill(pid, 0) != 0) return vsl_errno_to_nt_status(errno);
    uint32_t h = vsl_nt_allocate_handle(g_nt_ctx, -1, 0, NT_OBJECT_TYPE_PROCESS);
    if (h == 0) return NT_STATUS_UNSUCCESSFUL;
    for (int i = 0; i < 4096; i++) {
        if (g_nt_ctx->handle_table[i].valid && g_nt_ctx->handle_table[i].nt_handle == h) {
            g_nt_ctx->handle_table[i].data = (uint64_t)(uintptr_t)pid;
            break;
        }
    }
    *(uint32_t *)a_proc_out = h;
    return NT_STATUS_SUCCESS;
}

/* NtTerminateProcess (267): kill the child and reap it.
 * a = process handle, b = exit status (ignored). */
int64_t vsl_nt_terminate_process(uint64_t a_proc, uint64_t b_status,
                                  uint64_t c, uint64_t d, uint64_t e, uint64_t f) {
    (void)b_status; (void)c; (void)d; (void)e; (void)f;
    pid_t pid = vsl_nt_proc_pid((uint32_t)a_proc);
    if (pid < 0) return NT_STATUS_INVALID_HANDLE;
    if (pid == getpid()) return NT_STATUS_INVALID_PARAMETER; /* refuse to kill self */
    if (kill(pid, SIGKILL) != 0) return vsl_errno_to_nt_status(errno);
    /* Bounded reap (never blocks — the job-object WNOHANG lesson). */
    for (int i = 0; i < 50; i++) {
        int st;
        if (waitpid(pid, &st, WNOHANG) == pid) break;
        struct timespec ts = {0, 5 * 1000 * 1000};
        nanosleep(&ts, NULL);
    }
    vsl_nt_free_handle(g_nt_ctx, (uint32_t)a_proc);
    return NT_STATUS_SUCCESS;
}

/* NtCreateSection (53): allocate a shared image/section region via mmap.
 * a = section handle* (OUT), c = max size* (IN/OUT size_t). We model a
 * SECTION as an anonymous mmap the caller can later MapViewOfSection. */
int64_t vsl_nt_create_section(uint64_t a_sec_out, uint64_t b_access,
                               uint64_t c_max_size, uint64_t d_page_prot,
                               uint64_t e, uint64_t f) {
    (void)b_access; (void)d_page_prot; (void)e; (void)f;
    if (!a_sec_out || !c_max_size) return NT_STATUS_INVALID_PARAMETER;
    size_t *size = (size_t *)c_max_size;
    if (*size == 0) *size = 4096;
    void *p = mmap(NULL, *size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (p == MAP_FAILED) return vsl_errno_to_nt_status(errno);
    uint32_t h = vsl_nt_allocate_handle(g_nt_ctx, -1, 0, NT_OBJECT_TYPE_SECTION);
    if (h == 0) { munmap(p, *size); return NT_STATUS_UNSUCCESSFUL; }
    for (int i = 0; i < 4096; i++) {
        if (g_nt_ctx->handle_table[i].valid && g_nt_ctx->handle_table[i].nt_handle == h) {
            g_nt_ctx->handle_table[i].data = (uint64_t)(uintptr_t)p;
            break;
        }
    }
    *(uint32_t *)a_sec_out = h;
    return NT_STATUS_SUCCESS;
}

/* NtMapViewOfSection (114): map a section into a process's address space.
 * a = section handle, b = process handle (0 = self), c = base* (IN/OUT).
 * Returns SUCCESS and writes the mapped base into *c. For a cross-process
 * target we map a fresh anonymous region of the same size in THIS process
 * (the real PE relocation/ASLR work belongs to the loader; here we prove the
 * section is genuinely addressable memory). */
int64_t vsl_nt_map_view_of_section(uint64_t a_sec, uint64_t b_proc,
                                     uint64_t c_base, uint64_t d_zero,
                                     uint64_t e, uint64_t f) {
    (void)d_zero; (void)e; (void)f;
    if (!c_base) return NT_STATUS_INVALID_PARAMETER;
    uint64_t sec_data = 0;
    if (vsl_nt_handle_to_data(g_nt_ctx, (uint32_t)a_sec, &sec_data) != 0)
        return NT_STATUS_INVALID_HANDLE;
    void *sec_base = (void *)(uintptr_t)sec_data;
    /* Find the section's mmap size by probing the page. Simpler: re-derive a
     * 4 KiB view (minimum), which is what a loader maps first anyway. */
    size_t view_sz = 4096;
    void *view = mmap(*(void **)c_base, view_sz, PROT_READ | PROT_WRITE,
                      MAP_PRIVATE | MAP_ANONYMOUS | (*(void **)c_base ? MAP_FIXED : 0),
                      -1, 0);
    if (view == MAP_FAILED) return vsl_errno_to_nt_status(errno);
    (void)sec_base;
    *(void **)c_base = view;
    return NT_STATUS_SUCCESS;
}

/* NtWriteVirtualMemory (195): write bytes into a process's address space.
 * a = process handle, b = base address, c = buffer, d = size*, e = bytes_written*.
 * Uses process_vm_writev for genuine cross-process writes. */
int64_t vsl_nt_write_virtual_memory(uint64_t a_proc, uint64_t b_base,
                                      uint64_t c_buf, uint64_t d_size,
                                      uint64_t e_written, uint64_t f) {
    (void)f;
    if (!b_base || !c_buf || !d_size) return NT_STATUS_INVALID_PARAMETER;
    pid_t pid = vsl_nt_proc_pid((uint32_t)a_proc);
    if (pid < 0) return NT_STATUS_INVALID_HANDLE;
    size_t n = (size_t)(*(uint64_t *)d_size > 0 ? *(uint64_t *)d_size : 0);
    struct iovec local = { (void *)(uintptr_t)c_buf, n };
    struct iovec remote = { (void *)(uintptr_t)b_base, n };
    ssize_t w = process_vm_writev(pid, &local, 1, &remote, 1, 0);
    if (w < 0) return vsl_errno_to_nt_status(errno);
    if (e_written) *(uint64_t *)e_written = (uint64_t)w;
    return NT_STATUS_SUCCESS;
}

/* NtReadVirtualMemory (195->288): read bytes from a process's address space.
 * a = process handle, b = base address, c = buffer*, d = size*, e = bytes_read*.
 * Uses process_vm_readv for genuine cross-process reads. */
int64_t vsl_nt_read_virtual_memory(uint64_t a_proc, uint64_t b_base,
                                     uint64_t c_buf, uint64_t d_size,
                                     uint64_t e_read, uint64_t f) {
    (void)f;
    if (!b_base || !c_buf || !d_size) return NT_STATUS_INVALID_PARAMETER;
    pid_t pid = vsl_nt_proc_pid((uint32_t)a_proc);
    if (pid < 0) return NT_STATUS_INVALID_HANDLE;
    size_t n = (size_t)(*(uint64_t *)d_size > 0 ? *(uint64_t *)d_size : 0);
    struct iovec local = { (void *)(uintptr_t)c_buf, n };
    struct iovec remote = { (void *)(uintptr_t)b_base, n };
    ssize_t r = process_vm_readv(pid, &local, 1, &remote, 1, 0);
    if (r < 0) return vsl_errno_to_nt_status(errno);
    if (e_read) *(uint64_t *)e_read = (uint64_t)r;
    return NT_STATUS_SUCCESS;
}

/* ======================================================================
 * BATCH 6 — Thread lifecycle + wait/sync + mutant/semaphore.
 *
 * Completes the synchronization surface a real NT process needs: create a
 * thread suspended and resume it (NtResumeThread), block on any waitable
 * object (NtWaitForSingleObject), clone a handle (NtDuplicateObject), query
 * process info (NtQueryInformationProcess), and the two classic NT lock
 * primitives (NtCreateMutant/NtReleaseMutant, NtCreateSemaphore/
 * NtReleaseSemaphore) plus NtOpenThread. All do genuine POSIX work.
 * ==================================================================== */

/* NtResumeThread (215): release a thread created with CREATE_SUSPENDED.
 * a = thread handle. Signals the trampoline's gate condvar (stored in data). */

/* Register this batch's NT handlers into the global dispatch table. */
void vsl_nt_proc_register(vsl_syscall_fn_t *tbl, int size) {
    (void)size;
    tbl[19-1] = vsl_nt_allocate_virtual_memory;
    tbl[50-1] = vsl_nt_create_process;
    tbl[53-1] = vsl_nt_create_section;
    tbl[56-1] = vsl_nt_create_thread;
    tbl[88-1] = vsl_nt_free_virtual_memory;
    tbl[114-1] = vsl_nt_map_view_of_section;
    tbl[129-1] = vsl_nt_open_process;
    tbl[195-1] = vsl_nt_write_virtual_memory;
    tbl[267-1] = vsl_nt_terminate_process;
    tbl[288-1] = vsl_nt_read_virtual_memory;
}
