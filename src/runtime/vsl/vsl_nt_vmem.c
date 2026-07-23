/*
 * vsl_nt_vmem.c -- WuBuOS NT transliteration: Virtual Memory.
 *
 * Split from vsl_nt_proc.c (which mixed 5 NT subsystems in one TU).
 * Self-contained: real VSL/Linux work; dispatched via vsl_nt_vmem_register().
 * C11, opaque structs, minimal includes -- shares the vsl_nt_internal.h surface.
 */

#include "vsl_nt_internal.h"

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

int64_t vsl_nt_flush_virtual_memory(uint64_t a_proc, uint64_t b_base,
                                    uint64_t c_size, uint64_t d, uint64_t e, uint64_t f) {
    (void)a_proc; (void)d; (void)e; (void)f;
    if (!b_base || !c_size) return NT_STATUS_INVALID_PARAMETER;
    if (msync((void *)(uintptr_t)b_base, (size_t)c_size, MS_SYNC) != 0)
        return vsl_errno_to_nt_status(errno);
    return NT_STATUS_SUCCESS;
}

/* NtSuspendThread (264): SIGSTOP the thread's tid (stored in styx_fid). */

/* Register this module's NT handlers into the global dispatch table. */
void vsl_nt_vmem_register(vsl_syscall_fn_t *tbl, int size) {
    (void)size;
    tbl[24-1] = vsl_nt_allocate_virtual_memory;
    tbl[30-1] = vsl_nt_free_virtual_memory;
    tbl[283-1] = vsl_nt_reset_virtual_memory;
    tbl[243-1] = vsl_nt_flush_virtual_memory;
    tbl[278-1] = vsl_nt_lock_virtual_memory;
    tbl[80-1] = vsl_nt_protect_virtual_memory;
    tbl[35-1] = vsl_nt_query_virtual_memory;
    tbl[58-1] = vsl_nt_write_virtual_memory;
    tbl[478-1] = vsl_nt_unlock_virtual_memory;
    tbl[63-1] = vsl_nt_read_virtual_memory;
}
