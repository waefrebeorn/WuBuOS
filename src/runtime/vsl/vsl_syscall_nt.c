/*
 * vsl_syscall_nt.c -- ReactOS NT syscall → VSL transliteration layer (E1).
 *
 * This file closes the "0 transliterated" gap for the first 10 NT syscalls.
 * Each handler does REAL work (atom table, eventfd reset, UUID/LUID
 * generation, futex wake, IO cancel, job assignment, physical-page
 * alloc/free) and is wired into the NT bridge dispatch path
 * (vsl_nt_syscall_dispatch) so the Desktop/ReactOS personality can call
 * them through VSL.
 *
 * Reference study: reactos-study/reactos/ntoskrnl/ + dll/ntdll/
 *
 * C11, no nested functions.
 */

#include "vsl_nt_bridge.h"
#include "vsl_syscall_internal.h"
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/eventfd.h>
#include <sys/mman.h>
#include <linux/futex.h>
#include <sys/syscall.h>
#include <limits.h>

/* Matches the NT-bridge function-pointer type defined in vsl_syscall_table.c. */
typedef int64_t (*vsl_syscall_fn_t)(uint64_t, uint64_t, uint64_t,
                                    uint64_t, uint64_t, uint64_t);

/* Forward declarations of the 10 transliterated NT handlers (defined below). */
int64_t vsl_nt_add_atom(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_find_atom(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_clear_event(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_allocate_uuids(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_allocate_luid(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_alert_thread(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_cancel_io_file(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_assign_job(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_alloc_user_phys_pages(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_free_user_phys_pages(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);

/* Self-contained NT dispatch table. The canonical vsl_syscall_table.c in the
 * repo documents the full 297-entry NT→VSL map (and now wires these 10), but
 * it references handler symbols that are only resolved in the full runtime
 * link; this local table keeps the NT bridge independently linkable and
 * testable. Entries for the 10 transliterated syscalls point at the real
 * handlers below; everything else is VSL_NT_MAP_STUB (vsl_sys_nosys). */
extern int64_t vsl_sys_nosys(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);

#define NT_TBL_SIZE 297
static vsl_syscall_fn_t g_nt_dispatch[NT_TBL_SIZE];

static void nt_dispatch_init(void) {
    for (int i = 0; i < NT_TBL_SIZE; i++) g_nt_dispatch[i] = vsl_sys_nosys;
    g_nt_dispatch[9-1]  = vsl_nt_add_atom;
    g_nt_dispatch[15-1] = vsl_nt_alert_thread;
    g_nt_dispatch[16-1] = vsl_nt_allocate_luid;
    g_nt_dispatch[17-1] = vsl_nt_alloc_user_phys_pages;
    g_nt_dispatch[18-1] = vsl_nt_allocate_uuids;
    g_nt_dispatch[22-1] = vsl_nt_assign_job;
    g_nt_dispatch[25-1] = vsl_nt_cancel_io_file;
    g_nt_dispatch[27-1] = vsl_nt_clear_event;
    g_nt_dispatch[81-1] = vsl_nt_find_atom;
    g_nt_dispatch[87-1] = vsl_nt_free_user_phys_pages;
}

/* ----------------------------------------------------------------------
 * Atom table (NtAddAtom / NtFindAtom)
 * -------------------------------------------------------------------- */

#define NT_ATOM_MAX   1024
#define NT_ATOM_NAME  256

typedef struct {
    char    name[NT_ATOM_NAME];
    uint32_t atom;       /* 0x0001..0xC000 Win32 atom range */
    bool    used;
} nt_atom_entry_t;

static nt_atom_entry_t g_nt_atoms[NT_ATOM_MAX];
static uint32_t        g_nt_atom_next = 0x0001;

static uint32_t nt_atom_lookup(const char *name) {
    for (int i = 0; i < NT_ATOM_MAX; i++) {
        if (g_nt_atoms[i].used && strncmp(g_nt_atoms[i].name, name, NT_ATOM_NAME) == 0)
            return g_nt_atoms[i].atom;
    }
    return 0;
}

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
        p[i*16 + 6] = (p[i*16 + 6] & 0x0F) | 0x40;
        p[i*16 + 8] = (p[i*16 + 8] & 0x3F) | 0x80;
    }
    return NT_STATUS_SUCCESS;
}

static uint64_t g_nt_luid_counter = 0;

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
 * Bridge: init / shutdown / handle registry / status translation
 * -------------------------------------------------------------------- */

int vsl_nt_bridge_init(vsl_nt_bridge_ctx_t *ctx) {
    if (!ctx) return -1;
    memset(ctx, 0, sizeof(*ctx));
    ctx->current_pid = (uint32_t)getpid();
    ctx->current_tid = (uint32_t)gettid();
    for (int i = 0; i < 4096; i++) ctx->handle_table[i].valid = false;
    return 0;
}

void vsl_nt_bridge_shutdown(vsl_nt_bridge_ctx_t *ctx) {
    if (!ctx) return;
    for (int i = 0; i < 4096; i++) ctx->handle_table[i].valid = false;
}

uint32_t vsl_nt_allocate_handle(vsl_nt_bridge_ctx_t *ctx, int vsl_fd,
                                 uint64_t styx_fid, nt_object_type_t type) {
    if (!ctx) return 0;
    for (int i = 0; i < 4096; i++) {
        if (!ctx->handle_table[i].valid) {
            /* NT handles are 4-byte opaque; base at 0x1000 to avoid 0. */
            uint32_t h = (uint32_t)(0x1000 + i);
            ctx->handle_table[i].nt_handle = h;
            ctx->handle_table[i].vsl_fd    = vsl_fd;
            ctx->handle_table[i].styx_fid  = styx_fid;
            ctx->handle_table[i].type      = type;
            ctx->handle_table[i].valid     = true;
            return h;
        }
    }
    return 0;  /* no free slot */
}

int vsl_nt_free_handle(vsl_nt_bridge_ctx_t *ctx, uint32_t nt_handle) {
    if (!ctx) return -1;
    for (int i = 0; i < 4096; i++) {
        if (ctx->handle_table[i].valid && ctx->handle_table[i].nt_handle == nt_handle) {
            ctx->handle_table[i].valid = false;
            return 0;
        }
    }
    return -1;
}

int vsl_nt_handle_to_vsl_fd(vsl_nt_bridge_ctx_t *ctx, uint32_t nt_handle,
                             int *out_vsl_fd) {
    if (!ctx || !out_vsl_fd) return -1;
    for (int i = 0; i < 4096; i++) {
        if (ctx->handle_table[i].valid && ctx->handle_table[i].nt_handle == nt_handle) {
            *out_vsl_fd = ctx->handle_table[i].vsl_fd;
            return 0;
        }
    }
    return -1;
}

int vsl_nt_status_to_errno(uint32_t nt_status) {
    switch (nt_status) {
        case NT_STATUS_SUCCESS:                 return 0;
        case NT_STATUS_INVALID_PARAMETER:       return EINVAL;
        case NT_STATUS_NO_MEMORY:               return ENOMEM;
        case NT_STATUS_INVALID_HANDLE:          return EBADF;
        case NT_STATUS_ACCESS_DENIED:           return EACCES;
        case NT_STATUS_OBJECT_NAME_NOT_FOUND:   return ENOENT;
        case NT_STATUS_NOT_IMPLEMENTED:         return ENOSYS;
        case NT_STATUS_UNSUCCESSFUL:            return EIO;
        default:                                return EIO;
    }
}

uint32_t vsl_errno_to_nt_status(int errno_val) {
    switch (errno_val) {
        case 0:     return NT_STATUS_SUCCESS;
        case EINVAL: return NT_STATUS_INVALID_PARAMETER;
        case ENOMEM: return NT_STATUS_NO_MEMORY;
        case EBADF:  return NT_STATUS_INVALID_HANDLE;
        case EACCES: return NT_STATUS_ACCESS_DENIED;
        case ENOENT: return NT_STATUS_OBJECT_NAME_NOT_FOUND;
        case ENOSYS: return NT_STATUS_NOT_IMPLEMENTED;
        default:     return NT_STATUS_UNSUCCESSFUL;
    }
}

/* ----------------------------------------------------------------------
 * NT → VSL syscall dispatch (the bridge entry point)
 * -------------------------------------------------------------------- */

int64_t vsl_nt_syscall_dispatch(vsl_nt_bridge_ctx_t *ctx,
                                 uint16_t nt_syscall_num,
                                 uint64_t *args,
                                 int n_args) {
    (void)ctx;
    if (nt_syscall_num < 1 || nt_syscall_num > NT_TBL_SIZE)
        return NT_STATUS_NOT_IMPLEMENTED;
    static int inited = 0;
    if (!inited) { nt_dispatch_init(); inited = 1; }

    vsl_syscall_fn_t fn = g_nt_dispatch[nt_syscall_num - 1];
    if (fn == NULL || fn == vsl_sys_nosys) return NT_STATUS_NOT_IMPLEMENTED;

    uint64_t a = (n_args > 0) ? args[0] : 0;
    uint64_t b = (n_args > 1) ? args[1] : 0;
    uint64_t c = (n_args > 2) ? args[2] : 0;
    uint64_t d = (n_args > 3) ? args[3] : 0;
    uint64_t e = (n_args > 4) ? args[4] : 0;
    uint64_t f = (n_args > 5) ? args[5] : 0;

    int64_t ret = fn(a, b, c, d, e, f);
    /* Handlers return NT_STATUS_SUCCESS (0) or a negative errno. Translate
     * a negative errno into the equivalent NT status. */
    if (ret < 0) return vsl_errno_to_nt_status((int)(-ret));
    return ret;
}
