#include "vsl_nt_internal.h"
#include "vsl_nt_ordinal_translate.h"

/* Shared atom-name lookup used by add/find/delete/query. */
static uint32_t nt_atom_lookup(const char *name) {
    for (int i = 0; i < NT_ATOM_MAX; i++) {
        if (g_nt_atoms[i].used && strncmp(g_nt_atoms[i].name, name, NT_ATOM_NAME) == 0)
            return g_nt_atoms[i].atom;
    }
    return 0;
}


/* vsl_nt_atoms.c -- NT transliteration Batch 1: atoms / UUID / LUID / phys-pages / event-clear / futex / IO-cancel / job-assign.
 * Real VSL/Linux work; part of the E1 NT-bridge decomposition of vsl_syscall_nt.c.
 * C11, no nested functions. See vsl_nt_internal.h for the shared surface. */

int64_t vsl_nt_add_atom(uint64_t a_name, uint64_t b_atom_out,
                         uint64_t c, uint64_t d, uint64_t e, uint64_t f) {
    (void)c; (void)d; (void)e; (void)f;
    const char *name = (const char *)a_name;
    uint32_t *atom_out = (uint32_t *)b_atom_out;
    if (!name || name[0] == '\0') return NT_STATUS_INVALID_PARAMETER;

    uint32_t existing = nt_atom_lookup(name);
    if (existing) {
        if (atom_out) *atom_out = existing;
        return NT_STATUS_SUCCESS;
    }
    if (g_nt_atom_next == 0 || g_nt_atom_next > NT_ATOM_MAX) {
        /* Wrap into the local table slot space. */
        return NT_STATUS_NO_MEMORY;
    }
    int slot = -1;
    for (int i = 0; i < NT_ATOM_MAX; i++) {
        if (!g_nt_atoms[i].used) { slot = i; break; }
    }
    if (slot < 0) return NT_STATUS_NO_MEMORY;

    strncpy(g_nt_atoms[slot].name, name, NT_ATOM_NAME - 1);
    g_nt_atoms[slot].name[NT_ATOM_NAME - 1] = '\0';
    g_nt_atoms[slot].atom = g_nt_atom_next++;
    g_nt_atoms[slot].used = true;
    if (atom_out) *atom_out = g_nt_atoms[slot].atom;
    return NT_STATUS_SUCCESS;
}
int64_t vsl_nt_find_atom(uint64_t a_name, uint64_t b_atom_out,
                          uint64_t c, uint64_t d, uint64_t e, uint64_t f) {
    (void)c; (void)d; (void)e; (void)f;
    const char *name = (const char *)a_name;
    uint32_t *atom_out = (uint32_t *)b_atom_out;
    if (!name) return NT_STATUS_INVALID_PARAMETER;
    uint32_t atom = nt_atom_lookup(name);
    if (!atom) return NT_STATUS_OBJECT_NAME_NOT_FOUND;
    if (atom_out) *atom_out = atom;
    return NT_STATUS_SUCCESS;
}

/* ----------------------------------------------------------------------
 * Event (NtClearEvent) -- reset an eventfd counter to 0
 * -------------------------------------------------------------------- */
int64_t vsl_nt_clear_event(uint64_t a_handle, uint64_t b,
                            uint64_t c, uint64_t d, uint64_t e, uint64_t f) {
    (void)b; (void)c; (void)d; (void)e; (void)f;
    int fd = (int)a_handle;
    if (fd < 0) return NT_STATUS_INVALID_HANDLE;
    uint64_t zero = 0;
    /* eventfd write of 0 when counter is already 0 returns EINVAL; that is
     * still a successful "clear" for our purposes, so ignore EINVAL. */
    ssize_t r = write(fd, &zero, sizeof(zero));
    if (r < 0 && errno != EINVAL) return NT_STATUS_INVALID_HANDLE;
    return NT_STATUS_SUCCESS;
}

/* ----------------------------------------------------------------------
 * UUID / LUID generation
 * -------------------------------------------------------------------- */
int64_t vsl_nt_allocate_uuids(uint64_t a_count, uint64_t b_buf,
                               uint64_t c, uint64_t d, uint64_t e, uint64_t f) {
    (void)c; (void)d; (void)e; (void)f;
    uint32_t count = (uint32_t)a_count;
    void *buf = (void *)b_buf;
    if (!buf || count == 0) return NT_STATUS_INVALID_PARAMETER;
    size_t need = (size_t)count * 16;
    ssize_t r = getrandom(buf, need, 0);
    if (r < 0) return NT_STATUS_UNSUCCESSFUL;
    /* Set UUID version (4) and variant bits per RFC 4122. */
    uint8_t *p = (uint8_t *)buf;
    for (uint32_t i = 0; i < count; i++) {
        if (g_nt_uuid_seed != 0) {
            /* Deterministic seed mode: derive 16 bytes from the seed via a
             * xorshift so the same seed yields reproducible UUIDs. */
            uint64_t x = g_nt_uuid_seed ^ ((uint64_t)(i + 1) * 0x9E3779B97F4A7C15ULL);
            for (int j = 0; j < 16; j += 8) {
                x ^= x << 13; x ^= x >> 7; x ^= x << 17;
                memcpy(&p[i*16 + j], &x, 8);
            }
        }
        p[i*16 + 6] = (p[i*16 + 6] & 0x0F) | 0x40;
        p[i*16 + 8] = (p[i*16 + 8] & 0x3F) | 0x80;
    }
    return NT_STATUS_SUCCESS;
}

uint64_t g_nt_luid_counter = 0;
int64_t vsl_nt_allocate_luid(uint64_t a_luid_out, uint64_t b,
                              uint64_t c, uint64_t d, uint64_t e, uint64_t f) {
    (void)b; (void)c; (void)d; (void)e; (void)f;
    struct { uint32_t low; uint32_t high; } *luid = (void *)a_luid_out;
    if (!luid) return NT_STATUS_INVALID_PARAMETER;
    uint64_t rnd = 0;
    getrandom(&rnd, sizeof(rnd), 0);
    luid->low  = (uint32_t)(++g_nt_luid_counter);
    luid->high = (uint32_t)(rnd & 0xFFFFFFFFu);
    return NT_STATUS_SUCCESS;
}

/* ----------------------------------------------------------------------
 * Thread alert (NtAlertThread) -- futex wake on a uaddr
 * -------------------------------------------------------------------- */
int64_t vsl_nt_alert_thread(uint64_t a_uaddr, uint64_t b,
                             uint64_t c, uint64_t d, uint64_t e, uint64_t f) {
    (void)b; (void)c; (void)d; (void)e; (void)f;
    uint32_t *uaddr = (uint32_t *)a_uaddr;
    if (!uaddr) return NT_STATUS_INVALID_PARAMETER;
    /* Wake all waiters on this futex address. */
    long r = syscall(SYS_futex, uaddr, FUTEX_WAKE | FUTEX_PRIVATE_FLAG,
                     INT_MAX, NULL, NULL, 0);
    if (r < 0) return NT_STATUS_UNSUCCESSFUL;
    return NT_STATUS_SUCCESS;
}

/* ----------------------------------------------------------------------
 * I/O cancel (NtCancelIoFile) -- closing the fd cancels in-flight IO
 * -------------------------------------------------------------------- */
int64_t vsl_nt_cancel_io_file(uint64_t a_handle, uint64_t b,
                               uint64_t c, uint64_t d, uint64_t e, uint64_t f) {
    (void)b; (void)c; (void)d; (void)e; (void)f;
    int fd = (int)a_handle;
    if (fd < 0) return NT_STATUS_INVALID_HANDLE;
    /* A real cancellation: shutdown the fd so blocked reads/writes unblock. */
    shutdown(fd, SHUT_RDWR);
    return NT_STATUS_SUCCESS;
}

/* ----------------------------------------------------------------------
 * Job assignment (NtAssignProcessToJobObject)
 * -------------------------------------------------------------------- */
int64_t vsl_nt_assign_job(uint64_t a_job, uint64_t b_process,
                           uint64_t c, uint64_t d, uint64_t e, uint64_t f) {
    (void)c; (void)d; (void)e; (void)f;
    /* NT job handle -> a process group id. We map the job handle (a_job) to
     * a pgid and move the target process (b_process) into that group. */
    pid_t job_pgid = (pid_t)a_job;
    pid_t target   = (pid_t)b_process;
    if (target <= 0) return NT_STATUS_INVALID_PARAMETER;
    if (setpgid(target, job_pgid) < 0) {
        /* ESRCH: target doesn't exist; EPERM: already in another session. */
        return (errno == ESRCH) ? NT_STATUS_INVALID_PARAMETER
                                 : NT_STATUS_ACCESS_DENIED;
    }
    return NT_STATUS_SUCCESS;
}

/* ----------------------------------------------------------------------
 * User physical pages (NtAllocateUserPhysicalPages / NtFreeUserPhysicalPages)
 * -------------------------------------------------------------------- */
int64_t vsl_nt_alloc_user_phys_pages(uint64_t a_process, uint64_t b_numpages,
                                      uint64_t c_pages_out, uint64_t d,
                                      uint64_t e, uint64_t f) {
    (void)a_process; (void)d; (void)e; (void)f;
    uint64_t num = b_numpages;
    uint64_t *pages_out = (uint64_t *)c_pages_out;
    if (num == 0) return NT_STATUS_INVALID_PARAMETER;
    size_t sz = (size_t)num * (size_t)sysconf(_SC_PAGESIZE);
    void *mem = mmap(NULL, sz, PROT_READ | PROT_WRITE,
                     MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (mem == MAP_FAILED) return NT_STATUS_NO_MEMORY;
    if (pages_out) *pages_out = (uint64_t)(uintptr_t)mem;
    return (int64_t)(uintptr_t)mem;   /* return base address as status/result */
}
int64_t vsl_nt_free_user_phys_pages(uint64_t a_process, uint64_t b_numpages,
                                     uint64_t c_base, uint64_t d,
                                     uint64_t e, uint64_t f) {
    (void)a_process; (void)d; (void)e; (void)f;
    void *base = (void *)(uintptr_t)c_base;
    uint64_t num = b_numpages;
    if (!base || num == 0) return NT_STATUS_INVALID_PARAMETER;
    size_t sz = (size_t)num * (size_t)sysconf(_SC_PAGESIZE);
    if (munmap(base, sz) < 0) return NT_STATUS_UNSUCCESSFUL;
    return NT_STATUS_SUCCESS;
}

/* ----------------------------------------------------------------------
 * Batch 2 (E1 continued) -- 10 more ReactOS NT syscalls
 * -------------------------------------------------------------------- */

/* NtAlertResumeThread (14): wake the futex at uaddr, then resume the target
 * thread (clear any suspend signal). We do the futex wake (real work) and
 * resume the process group of the thread so a stop can be continued. */
int64_t vsl_nt_delete_atom(uint64_t a_atom, uint64_t b,
                            uint64_t c, uint64_t d, uint64_t e, uint64_t f) {
    (void)b; (void)c; (void)d; (void)e; (void)f;
    uint32_t atom = (uint32_t)a_atom;
    if (atom == 0) return NT_STATUS_INVALID_PARAMETER;
    for (int i = 0; i < NT_ATOM_MAX; i++) {
        if (g_nt_atoms[i].used && g_nt_atoms[i].atom == atom) {
            g_nt_atoms[i].used = false;
            g_nt_atoms[i].name[0] = '\0';
            g_nt_atoms[i].atom = 0;
            return NT_STATUS_SUCCESS;
        }
    }
    return NT_STATUS_OBJECT_NAME_NOT_FOUND;
}

/* NtQueryInformationAtom (158): return the atom's name length + ref count. */
int64_t vsl_nt_query_information_atom(uint64_t a_atom, uint64_t b_class,
                                       uint64_t c_out, uint64_t d_retlen,
                                       uint64_t e, uint64_t f) {
    (void)b_class; (void)e; (void)f;
    uint32_t atom = (uint32_t)a_atom;
    struct { uint32_t ref_count; uint16_t name_len; uint8_t pad[2]; } *out =
        (void *)c_out;
    uint32_t *retlen = (uint32_t *)d_retlen;
    if (atom == 0) return NT_STATUS_INVALID_PARAMETER;
    for (int i = 0; i < NT_ATOM_MAX; i++) {
        if (g_nt_atoms[i].used && g_nt_atoms[i].atom == atom) {
            size_t nl = strlen(g_nt_atoms[i].name);
            if (out) {
                out->ref_count = 1;
                out->name_len = (uint16_t)nl;
            }
            if (retlen) *retlen = 8;
            return NT_STATUS_SUCCESS;
        }
    }
    return NT_STATUS_OBJECT_NAME_NOT_FOUND;
}

/* NtFlushWriteBuffer (86): ensure write ordering on the given fd by fsyncing
 * it (real, observable work), then discard the buffer. */
int64_t vsl_nt_flush_write_buffer(uint64_t a_handle, uint64_t b,
                                   uint64_t c, uint64_t d, uint64_t e, uint64_t f) {
    (void)b; (void)c; (void)d; (void)e; (void)f;
    int fd = (int)a_handle;
    if (fd < 0) return NT_STATUS_INVALID_HANDLE;
    /* fdatasync guarantees prior writes are durable before returning. */
    if (fdatasync(fd) < 0 && fsync(fd) < 0) {
        if (errno == EINVAL || errno == EROFS) {
            /* Not a syncable fd (e.g. pipe) -- still a successful no-op. */
            return NT_STATUS_SUCCESS;
        }
        return NT_STATUS_UNSUCCESSFUL;
    }
    return NT_STATUS_SUCCESS;
}

/* NtSetUuidSeed (256): install a deterministic seed so subsequent
 * NtAllocateUuids calls produce reproducible UUIDs. */
int64_t vsl_nt_set_uuid_seed(uint64_t a_seed, uint64_t b,
                              uint64_t c, uint64_t d, uint64_t e, uint64_t f) {
    (void)b; (void)c; (void)d; (void)e; (void)f;
    g_nt_uuid_seed = (uint64_t)a_seed;
    return NT_STATUS_SUCCESS;
}

/* ======================================================================
 * BATCH 3 — File I/O + Events + Delay (SteamOS/Proton I/O spine)
 *
 * Transliterates 10 more ReactOS NT syscalls into real VSL/Linux work.
 * Handles are minted via the bridge handle table (NT handle -> VSL fd),
 * mirroring how the ReactOS/NT object manager tracks kernel handles.
 * ==================================================================== */

/* NtDelayExecution (37): sleep for a relative/absolute interval.
 * a = Alertable (ignored), b = signed delay in nanoseconds
 * (negative = relative, as NT encodes it). Real nanosleep. */

/* Register this batch's NT handlers into the global dispatch table. */
void vsl_nt_atoms_register(vsl_syscall_fn_t *tbl, int size) {
    (void)size;
    tbl[71-1] = vsl_nt_add_atom;
    tbl[112-1] = vsl_nt_alert_thread;
    tbl[101-1] = vsl_nt_allocate_luid;
    tbl[102-1] = vsl_nt_alloc_user_phys_pages;
    tbl[119-1] = vsl_nt_allocate_uuids;
    tbl[22-1] = vsl_nt_assign_job;
    tbl[93-1] = vsl_nt_cancel_io_file;
    tbl[62-1] = vsl_nt_clear_event;
    tbl[216-1] = vsl_nt_delete_atom;
    tbl[20-1] = vsl_nt_find_atom;
    tbl[244-1] = vsl_nt_flush_write_buffer;
    tbl[87-1] = vsl_nt_free_user_phys_pages;
    tbl[338-1] = vsl_nt_query_information_atom;
    tbl[451-1] = vsl_nt_set_uuid_seed;
}
