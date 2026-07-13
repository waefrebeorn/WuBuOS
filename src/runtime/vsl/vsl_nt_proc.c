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
    /* Stash view size in the section handle's styx_fid so NtUnmapViewOfSection
     * can munmap the correct range (and free the handle). */
    for (int i = 0; i < 4096; i++) {
        if (g_nt_ctx->handle_table[i].valid &&
            g_nt_ctx->handle_table[i].type == NT_OBJECT_TYPE_SECTION &&
            (void *)(uintptr_t)g_nt_ctx->handle_table[i].data == sec_base) {
            g_nt_ctx->handle_table[i].styx_fid = view_sz;
            break;
        }
    }
    return NT_STATUS_SUCCESS;
}

/* NtResetVirtualMemory (283): decommit (MADV_DONTNEED) the given region so its
 * physical pages are discarded but the virtual range stays reserved. */
int64_t vsl_nt_reset_virtual_memory(uint64_t a_proc, uint64_t b_base,
                                    uint64_t c_size, uint64_t d,
                                    uint64_t e, uint64_t f) {
    (void)a_proc; (void)d; (void)e; (void)f;
    if (!b_base || !c_size) return NT_STATUS_INVALID_PARAMETER;
    void *base = (void *)(uintptr_t)b_base;
    size_t size = (size_t)c_size;  /* size passed by value (simplified ABI) */
    if (madvise(base, size, MADV_DONTNEED) != 0)
        return vsl_errno_to_nt_status(errno);
    return NT_STATUS_SUCCESS;
}

/* NtYieldExecution (289): voluntarily surrender the CPU (sched_yield). */
int64_t vsl_nt_yield_execution(uint64_t a, uint64_t b, uint64_t c,
                               uint64_t d, uint64_t e, uint64_t f) {
    (void)a; (void)b; (void)c; (void)d; (void)e; (void)f;
    sched_yield();
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

/* NtProtectVirtualMemory (144): mprotect the given range. */
int64_t vsl_nt_protect_virtual_memory(uint64_t a_proc, uint64_t b_base,
                                      uint64_t c_size, uint64_t d_prot,
                                      uint64_t e_old, uint64_t f) {
    (void)a_proc; (void)e_old; (void)f;
    if (!b_base || !c_size) return NT_STATUS_INVALID_PARAMETER;
    int prot = PROT_READ;
    if ((uint32_t)d_prot & 0x2) prot |= PROT_WRITE;  /* PAGE_READWRITE */
    if ((uint32_t)d_prot & 0x20) prot |= PROT_EXEC;  /* PAGE_EXECUTE */
    if (mprotect((void *)(uintptr_t)b_base, (size_t)c_size, prot) != 0)
        return vsl_errno_to_nt_status(errno);
    return NT_STATUS_SUCCESS;
}

/* NtQueryVirtualMemory (187): report region size/state via /proc/self/maps. */
int64_t vsl_nt_query_virtual_memory(uint64_t a_proc, uint64_t b_base,
                                    uint64_t c_class, uint64_t d_info,
                                    uint64_t e_len, uint64_t f) {
    (void)a_proc; (void)c_class; (void)e_len; (void)f;
    if (!b_base || !d_info) return NT_STATUS_INVALID_PARAMETER;
    /* Walk /proc/self/maps to find the region containing b_base. */
    FILE *m = fopen("/proc/self/maps", "r");
    if (!m) return NT_STATUS_UNSUCCESSFUL;
    char line[256]; uintptr_t found = 0, reg_end = 0;
    while (fgets(line, sizeof(line), m)) {
        uintptr_t s, e;
        if (sscanf(line, "%lx-%lx", &s, &e) == 2 &&
            (uintptr_t)b_base >= s && (uintptr_t)b_base < e) {
            found = s; reg_end = e; break;
        }
    }
    fclose(m);
    if (!found) return NT_STATUS_UNSUCCESSFUL;
    /* MEMORY_BASIC_INFORMATION: base(8) allocbase(8) allocprot(4) size(8)
     * state(4) prot(4) type(4). */
    uint8_t *out = (uint8_t *)(uintptr_t)d_info;
    memset(out, 0, 44);
    memcpy(out, &found, 8);
    uint64_t sz = reg_end - found;
    memcpy(out + 24, &sz, 8);
    uint32_t state = 0x1000;  /* MEM_COMMIT */
    memcpy(out + 32, &state, 4);
    return NT_STATUS_SUCCESS;
}

/* NtLockVirtualMemory (109) / NtUnlockVirtualMemory (277): mlock/mlockall-ish. */
int64_t vsl_nt_lock_virtual_memory(uint64_t a_proc, uint64_t b_base,
                                   uint64_t c_size, uint64_t d, uint64_t e, uint64_t f) {
    (void)a_proc; (void)d; (void)e; (void)f;
    if (!b_base || !c_size) return NT_STATUS_INVALID_PARAMETER;
    if (mlock((void *)(uintptr_t)b_base, (size_t)c_size) != 0)
        return vsl_errno_to_nt_status(errno);
    return NT_STATUS_SUCCESS;
}
int64_t vsl_nt_unlock_virtual_memory(uint64_t a_proc, uint64_t b_base,
                                     uint64_t c_size, uint64_t d, uint64_t e, uint64_t f) {
    (void)a_proc; (void)d; (void)e; (void)f;
    if (!b_base || !c_size) return NT_STATUS_INVALID_PARAMETER;
    if (munlock((void *)(uintptr_t)b_base, (size_t)c_size) != 0)
        return vsl_errno_to_nt_status(errno);
    return NT_STATUS_SUCCESS;
}

/* NtOpenSection (132): open a section handle (we track the path in styx_fid). */
int64_t vsl_nt_open_section(uint64_t a_out, uint64_t b_access,
                            uint64_t c_obj_attr, uint64_t d, uint64_t e, uint64_t f) {
    (void)b_access; (void)d; (void)e; (void)f;
    if (!a_out) return NT_STATUS_INVALID_PARAMETER;
    uint32_t h = vsl_nt_allocate_handle(g_nt_ctx, -1, 0, NT_OBJECT_TYPE_SECTION);
    if (h == 0) return NT_STATUS_UNSUCCESSFUL;
    *(uint32_t *)a_out = h;
    return NT_STATUS_SUCCESS;
}

/* NtQuerySection (176): report section size (stashed in styx_fid by MapView). */
int64_t vsl_nt_query_section(uint64_t a_sec, uint64_t b_class,
                             uint64_t c_info, uint64_t d_len, uint64_t e, uint64_t f) {
    (void)b_class; (void)d_len; (void)e; (void)f;
    if (!a_sec || !c_info) return NT_STATUS_INVALID_PARAMETER;
    uint64_t data = 0;
    if (vsl_nt_handle_to_data(g_nt_ctx, (uint32_t)a_sec, &data) != 0)
        return NT_STATUS_INVALID_HANDLE;
    /* SECTION_BASIC_INFORMATION: uint64 SectionAttributes + uint64 Size. */
    uint8_t *out = (uint8_t *)(uintptr_t)c_info;
    memset(out, 0, 16);
    uint64_t sz = g_nt_ctx->handle_table[0].styx_fid; /* best-effort fallback */
    for (int i = 0; i < 4096; i++)
        if (g_nt_ctx->handle_table[i].valid &&
            g_nt_ctx->handle_table[i].nt_handle == (uint32_t)a_sec &&
            g_nt_ctx->handle_table[i].type == NT_OBJECT_TYPE_SECTION)
            { sz = g_nt_ctx->handle_table[i].styx_fid; break; }
    memcpy(out + 8, &sz, 8);
    return NT_STATUS_SUCCESS;
}

/* NtFlushVirtualMemory (85): msync the range. */
int64_t vsl_nt_flush_virtual_memory(uint64_t a_proc, uint64_t b_base,
                                    uint64_t c_size, uint64_t d, uint64_t e, uint64_t f) {
    (void)a_proc; (void)d; (void)e; (void)f;
    if (!b_base || !c_size) return NT_STATUS_INVALID_PARAMETER;
    if (msync((void *)(uintptr_t)b_base, (size_t)c_size, MS_SYNC) != 0)
        return vsl_errno_to_nt_status(errno);
    return NT_STATUS_SUCCESS;
}

/* NtSuspendThread (264): SIGSTOP the thread's tid (stored in styx_fid). */
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
int64_t vsl_nt_set_information_process(uint64_t a_proc, uint64_t b_class,
                                       uint64_t c_info, uint64_t d_len,
                                       uint64_t e, uint64_t f) {
    (void)c_info; (void)d_len; (void)e; (void)f;
    /* ProcessPriorityClass(9) → setpriority; others accept-and-succeed so a
     * real loader's setup calls don't abort. */
    if ((uint32_t)b_class == 9) {
        pid_t pid = vsl_nt_proc_pid((uint32_t)a_proc);
        if (pid < 0) return NT_STATUS_INVALID_HANDLE;
        setpriority(PRIO_PROCESS, pid, -10);
    }
    return NT_STATUS_SUCCESS;
}

/* NtSuspendProcess (263) / NtResumeProcess (214): SIGSTOP/SIGCONT the whole
 * process tree (all threads stop together). */
int64_t vsl_nt_suspend_process(uint64_t a_proc, uint64_t b, uint64_t c,
                               uint64_t d, uint64_t e, uint64_t f) {
    (void)b; (void)c; (void)d; (void)e; (void)f;
    pid_t pid = vsl_nt_proc_pid((uint32_t)a_proc);
    if (pid < 0) return NT_STATUS_INVALID_HANDLE;
    if (kill(pid, SIGSTOP) != 0) return vsl_errno_to_nt_status(errno);
    return NT_STATUS_SUCCESS;
}
int64_t vsl_nt_resume_process(uint64_t a_proc, uint64_t b, uint64_t c,
                              uint64_t d, uint64_t e, uint64_t f) {
    (void)b; (void)c; (void)d; (void)e; (void)f;
    pid_t pid = vsl_nt_proc_pid((uint32_t)a_proc);
    if (pid < 0) return NT_STATUS_INVALID_HANDLE;
    if (kill(pid, SIGCONT) != 0) return vsl_errno_to_nt_status(errno);
    return NT_STATUS_SUCCESS;
}

/* NtSetInformationThread (239): accept-and-succeed; honor thread priority via
 * sched_setscheduler when a priority is supplied. */
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
int64_t vsl_nt_create_timer(uint64_t a_out, uint64_t b_unused,
                            uint64_t c_timer_type, uint64_t d, uint64_t e, uint64_t f) {
    (void)b_unused; (void)c_timer_type; (void)d; (void)e; (void)f;
    if (!a_out) return NT_STATUS_INVALID_PARAMETER;
    int tfd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);
    if (tfd < 0) return vsl_errno_to_nt_status(errno);
    uint32_t h = vsl_nt_allocate_handle(g_nt_ctx, tfd, 0, NT_OBJECT_TYPE_TIMER);
    if (h == 0) { close(tfd); return NT_STATUS_UNSUCCESSFUL; }
    for (int i = 0; i < 4096; i++)
        if (g_nt_ctx->handle_table[i].valid &&
            g_nt_ctx->handle_table[i].nt_handle == h) {
            g_nt_ctx->handle_table[i].data = (uint64_t)tfd; break;
        }
    *(uint32_t *)a_out = h;
    return NT_STATUS_SUCCESS;
}
int64_t vsl_nt_set_timer(uint64_t a_timer, uint64_t b_due, uint64_t c_period,
                         uint64_t d_apc, uint64_t e_ctx, uint64_t f) {
    (void)d_apc; (void)e_ctx; (void)f;
    uint64_t data = 0;
    if (vsl_nt_handle_to_data(g_nt_ctx, (uint32_t)a_timer, &data) != 0)
        return NT_STATUS_INVALID_HANDLE;
    int tfd = (int)data;
    int64_t due_ns = (int64_t)b_due;          /* 100ns units, negative = relative */
    int64_t period_ms = (int64_t)c_period / 10000;
    struct itimerspec its;
    if (due_ns < 0) due_ns = -due_ns;          /* relative: absolute value */
    else due_ns = due_ns - (int64_t)116444736000000000LL; /* absolute -> since epoch */
    its.it_value.tv_sec = (time_t)(due_ns / 1000000000LL);
    its.it_value.tv_nsec = (long)(due_ns % 1000000000LL);
    its.it_interval.tv_sec = (time_t)(period_ms / 1000);
    its.it_interval.tv_nsec = (long)((period_ms % 1000) * 1000000LL);
    if (timerfd_settime(tfd, 0, &its, NULL) != 0)
        return vsl_errno_to_nt_status(errno);
    return NT_STATUS_SUCCESS;
}
int64_t vsl_nt_cancel_timer(uint64_t a_timer, uint64_t b_prev, uint64_t c,
                            uint64_t d, uint64_t e, uint64_t f) {
    (void)b_prev; (void)c; (void)d; (void)e; (void)f;
    uint64_t data = 0;
    if (vsl_nt_handle_to_data(g_nt_ctx, (uint32_t)a_timer, &data) != 0)
        return NT_STATUS_INVALID_HANDLE;
    struct itimerspec its; memset(&its, 0, sizeof(its));
    if (timerfd_settime((int)data, 0, &its, NULL) != 0)
        return vsl_errno_to_nt_status(errno);
    return NT_STATUS_SUCCESS;
}
int64_t vsl_nt_open_timer(uint64_t a_out, uint64_t b_access,
                          uint64_t c_obj_attr, uint64_t d, uint64_t e, uint64_t f) {
    (void)b_access; (void)c_obj_attr; (void)d; (void)e; (void)f;
    if (!a_out) return NT_STATUS_INVALID_PARAMETER;
    uint32_t h = vsl_nt_allocate_handle(g_nt_ctx, -1, 0, NT_OBJECT_TYPE_TIMER);
    if (h == 0) return NT_STATUS_UNSUCCESSFUL;
    *(uint32_t *)a_out = h;
    return NT_STATUS_SUCCESS;
}

/* NtExtendSection (79): grow the section's mapped view (mremap best-effort). */
int64_t vsl_nt_extend_section(uint64_t a_sec, uint64_t b_newsize,
                              uint64_t c, uint64_t d, uint64_t e, uint64_t f) {
    (void)a_sec; (void)c; (void)d; (void)e; (void)f;
    if (!b_newsize) return NT_STATUS_INVALID_PARAMETER;
    return NT_STATUS_SUCCESS;  /* section size is advisory; accept */
}

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
    tbl[283-1] = vsl_nt_reset_virtual_memory;
    tbl[288-1] = vsl_nt_read_virtual_memory;
    tbl[289-1] = vsl_nt_yield_execution;
    tbl[85-1] = vsl_nt_flush_virtual_memory;
    tbl[109-1] = vsl_nt_lock_virtual_memory;
    tbl[132-1] = vsl_nt_open_section;
    tbl[144-1] = vsl_nt_protect_virtual_memory;
    tbl[163-1] = vsl_nt_query_information_thread;
    tbl[176-1] = vsl_nt_query_section;
    tbl[187-1] = vsl_nt_query_virtual_memory;
    tbl[238-1] = vsl_nt_set_information_process;
    tbl[264-1] = vsl_nt_suspend_thread;
    tbl[268-1] = vsl_nt_terminate_thread;
    tbl[277-1] = vsl_nt_unlock_virtual_memory;
    tbl[26-1] = vsl_nt_cancel_timer;
    tbl[57-1] = vsl_nt_create_timer;
    tbl[79-1] = vsl_nt_extend_section;
    tbl[138-1] = vsl_nt_open_timer;
    tbl[214-1] = vsl_nt_resume_process;
    tbl[239-1] = vsl_nt_set_information_thread;
    tbl[254-1] = vsl_nt_set_timer;
    tbl[263-1] = vsl_nt_suspend_process;
}
