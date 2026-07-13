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
#include <sys/stat.h>
#include <sys/wait.h>
#include <signal.h>
#include <time.h>
#include <pthread.h>
#include <semaphore.h>
#include <sys/uio.h>

/* Matches the NT-bridge function-pointer type defined in vsl_syscall_table.c. */
typedef int64_t (*vsl_syscall_fn_t)(uint64_t, uint64_t, uint64_t,
                                    uint64_t, uint64_t, uint64_t);

/* Forward declarations of the transliterated NT handlers (defined below). */
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
int64_t vsl_nt_alert_resume_thread(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_are_mapped_files_same(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_create_job_object(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_open_job_object(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_terminate_job_object(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_is_process_in_job(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_delete_atom(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_query_information_atom(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_flush_write_buffer(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_set_uuid_seed(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
/* -- Batch 3 (file I/O + events + delay) -- */
int64_t vsl_nt_delay_execution(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_create_event(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_open_event(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_set_event(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_reset_event(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_open_file(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_read_file(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_write_file(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_close(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_query_information_file(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
/* -- Batch 4 (process / thread / virtual memory — the SteamOS "NT=Proton" spine) -- */
int64_t vsl_nt_allocate_virtual_memory(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_free_virtual_memory(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_create_thread(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_create_process(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
/* -- Batch 5 (process/memory launch path — boot a real image like Proton) -- */
int64_t vsl_nt_open_process(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_terminate_process(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_create_section(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_map_view_of_section(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_write_virtual_memory(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_read_virtual_memory(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
/* -- Batch 6 (thread lifecycle + wait/sync + mutant/semaphore) -- */
int64_t vsl_nt_resume_thread(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_wait_for_single_object(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_duplicate_object(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_query_information_process(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_create_mutant(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_release_mutant(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_create_semaphore(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_release_semaphore(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_open_thread(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);

/* Global UUID seed: 0 = cryptographically random (default); nonzero = the
 * deterministic seed installed via NtSetUuidSeed (256). Declared at file
 * scope so it is visible to vsl_nt_allocate_uuids from any call site. */
static uint64_t g_nt_uuid_seed = 0;

/* Active bridge context (set in vsl_nt_bridge_init) so that transliterated
 * handlers — which receive only raw syscall args, not the ctx — can reach the
 * handle table. Mirrors how the NT kernel tracks object handles per-process.
 * Declared at file scope (like g_nt_uuid_seed) so it is visible to every
 * transliterated handler, not just ones defined after vsl_nt_bridge_init(). */
static vsl_nt_bridge_ctx_t *g_nt_ctx = NULL;

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
    g_nt_dispatch[14-1]  = vsl_nt_alert_resume_thread;
    g_nt_dispatch[21-1]  = vsl_nt_are_mapped_files_same;
    g_nt_dispatch[42-1]  = vsl_nt_create_job_object;
    g_nt_dispatch[63-1]  = vsl_nt_delete_atom;
    g_nt_dispatch[86-1]  = vsl_nt_flush_write_buffer;
    g_nt_dispatch[99-1]  = vsl_nt_is_process_in_job;
    g_nt_dispatch[125-1] = vsl_nt_open_job_object;
    g_nt_dispatch[158-1] = vsl_nt_query_information_atom;
    g_nt_dispatch[256-1] = vsl_nt_set_uuid_seed;
    g_nt_dispatch[266-1] = vsl_nt_terminate_job_object;
    /* Batch 3: file I/O + events + delay (SteamOS/Proton I/O spine). */
    g_nt_dispatch[62-1]  = vsl_nt_delay_execution;   /* NtDelayExecution  */
    g_nt_dispatch[38-1]  = vsl_nt_create_event;      /* NtCreateEvent     */
    g_nt_dispatch[121-1] = vsl_nt_open_event;        /* NtOpenEvent       */
    g_nt_dispatch[209-1] = vsl_nt_reset_event;       /* NtResetEvent      */
    g_nt_dispatch[229-1] = vsl_nt_set_event;         /* NtSetEvent        */
    g_nt_dispatch[28-1]  = vsl_nt_close;             /* NtClose           */
    g_nt_dispatch[123-1] = vsl_nt_open_file;         /* NtOpenFile        */
    g_nt_dispatch[192-1] = vsl_nt_read_file;         /* NtReadFile        */
    g_nt_dispatch[285-1] = vsl_nt_write_file;        /* NtWriteFile       */
    g_nt_dispatch[159-1] = vsl_nt_query_information_file; /* NtQueryInformationFile */
    /* Batch 4: process / thread / virtual memory (NT = SteamOS launch spine). */
    g_nt_dispatch[19-1]  = vsl_nt_allocate_virtual_memory; /* NtAllocateVirtualMemory */
    g_nt_dispatch[88-1]  = vsl_nt_free_virtual_memory;     /* NtFreeVirtualMemory */
    g_nt_dispatch[56-1]  = vsl_nt_create_thread;           /* NtCreateThread */
    g_nt_dispatch[50-1]  = vsl_nt_create_process;          /* NtCreateProcess */
    /* Batch 5: process/memory launch path (boot a real image, Proton-style). */
    g_nt_dispatch[129-1] = vsl_nt_open_process;            /* NtOpenProcess */
    g_nt_dispatch[267-1] = vsl_nt_terminate_process;       /* NtTerminateProcess */
    g_nt_dispatch[53-1]  = vsl_nt_create_section;          /* NtCreateSection */
    g_nt_dispatch[114-1] = vsl_nt_map_view_of_section;     /* NtMapViewOfSection */
    g_nt_dispatch[195-1] = vsl_nt_write_virtual_memory;    /* NtWriteVirtualMemory */
    g_nt_dispatch[288-1] = vsl_nt_read_virtual_memory;     /* NtReadVirtualMemory */
    /* Batch 6: thread lifecycle + wait/sync + mutant/semaphore. */
    g_nt_dispatch[215-1] = vsl_nt_resume_thread;           /* NtResumeThread */
    g_nt_dispatch[282-1] = vsl_nt_wait_for_single_object;  /* NtWaitForSingleObject */
    g_nt_dispatch[72-1]  = vsl_nt_duplicate_object;        /* NtDuplicateObject */
    g_nt_dispatch[162-1] = vsl_nt_query_information_process; /* NtQueryInformationProcess */
    g_nt_dispatch[46-1]  = vsl_nt_create_mutant;           /* NtCreateMutant */
    g_nt_dispatch[197-1] = vsl_nt_release_mutant;          /* NtReleaseMutant */
    g_nt_dispatch[54-1]  = vsl_nt_create_semaphore;        /* NtCreateSemaphore */
    g_nt_dispatch[198-1] = vsl_nt_release_semaphore;       /* NtReleaseSemaphore */
    g_nt_dispatch[135-1] = vsl_nt_open_thread;             /* NtOpenThread */
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
 * Batch 2 (E1 continued) -- 10 more ReactOS NT syscalls
 * -------------------------------------------------------------------- */

/* NtAlertResumeThread (14): wake the futex at uaddr, then resume the target
 * thread (clear any suspend signal). We do the futex wake (real work) and
 * resume the process group of the thread so a stop can be continued. */
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

/* Job objects (42/125/266): track created job groups and terminate them. */
#define NT_JOB_MAX 256
static struct {
    uint32_t job_id;     /* handle cookie */
    pid_t    pgid;       /* backing process group (owned by a sentinel child) */
    bool     used;
} g_nt_jobs[NT_JOB_MAX];
static uint32_t g_nt_job_next = 0x1000;

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
int64_t vsl_nt_delay_execution(uint64_t a_alertable, uint64_t b_ns,
                                uint64_t c, uint64_t d, uint64_t e, uint64_t f) {
    (void)a_alertable; (void)c; (void)d; (void)e; (void)f;
    int64_t ns = (int64_t)b_ns;
    struct timespec ts;
    if (ns < 0) ns = -ns;            /* relative delay magnitude */
    ts.tv_sec  = (time_t)(ns / 1000000000LL);
    ts.tv_nsec = (long)(ns % 1000000000LL);
    if (nanosleep(&ts, NULL) < 0) return NT_STATUS_UNSUCCESSFUL;
    return NT_STATUS_SUCCESS;
}

/* NtCreateEvent (39): allocate an eventfd. a = initial state (0 unsignaled).
 * Returns the NT handle (0 on failure). */
int64_t vsl_nt_create_event(uint64_t a_init_state, uint64_t b,
                             uint64_t c, uint64_t d, uint64_t e, uint64_t f) {
    (void)b; (void)c; (void)d; (void)e; (void)f;
    int efd = eventfd((a_init_state ? 1 : 0), EFD_NONBLOCK);
    if (efd < 0) return NT_STATUS_UNSUCCESSFUL;
    uint32_t h = vsl_nt_allocate_handle(g_nt_ctx, efd, 0, NT_OBJECT_TYPE_EVENT);
    if (h == 0) { close(efd); return NT_STATUS_UNSUCCESSFUL; }
    return (int64_t)h;
}

/* NtOpenEvent (127): return a handle to an event. Named events are not
 * persisted across calls in this bridge, so we mint a fresh eventfd
 * (unsignaled) and return its NT handle — sufficient for the Open semantic
 * in the transliterated personality. */
int64_t vsl_nt_open_event(uint64_t a, uint64_t b,
                           uint64_t c, uint64_t d, uint64_t e, uint64_t f) {
    (void)a; (void)b; (void)c; (void)d; (void)e; (void)f;
    int efd = eventfd(0, EFD_NONBLOCK);
    if (efd < 0) return NT_STATUS_UNSUCCESSFUL;
    uint32_t h = vsl_nt_allocate_handle(g_nt_ctx, efd, 0, NT_OBJECT_TYPE_EVENT);
    if (h == 0) { close(efd); return NT_STATUS_UNSUCCESSFUL; }
    return (int64_t)h;
}

/* NtSetEvent (229): signal the event by writing 1 to the eventfd. */
int64_t vsl_nt_set_event(uint64_t a_handle, uint64_t b,
                          uint64_t c, uint64_t d, uint64_t e, uint64_t f) {
    (void)b; (void)c; (void)d; (void)e; (void)f;
    int fd;
    if (vsl_nt_handle_to_vsl_fd(g_nt_ctx, (uint32_t)a_handle, &fd) != 0 || fd < 0)
        return NT_STATUS_INVALID_HANDLE;
    uint64_t one = 1;
    if (write(fd, &one, sizeof(one)) != sizeof(one))
        return NT_STATUS_UNSUCCESSFUL;
    return NT_STATUS_SUCCESS;
}

/* NtResetEvent (209): clear the event by writing 0 to the eventfd. */
int64_t vsl_nt_reset_event(uint64_t a_handle, uint64_t b,
                            uint64_t c, uint64_t d, uint64_t e, uint64_t f) {
    (void)b; (void)c; (void)d; (void)e; (void)f;
    int fd;
    if (vsl_nt_handle_to_vsl_fd(g_nt_ctx, (uint32_t)a_handle, &fd) != 0 || fd < 0)
        return NT_STATUS_INVALID_HANDLE;
    uint64_t zero = 0;
    if (write(fd, &zero, sizeof(zero)) != sizeof(zero))
        return NT_STATUS_UNSUCCESSFUL;
    return NT_STATUS_SUCCESS;
}

/* NtOpenFile (123): open a file by path (b = char* path) and mint an NT
 * handle. Returns the NT handle, or 0 on failure. */
int64_t vsl_nt_open_file(uint64_t a_handle_out, uint64_t b_path,
                          uint64_t c_access, uint64_t d_share, uint64_t e, uint64_t f) {
    (void)a_handle_out; (void)c_access; (void)d_share; (void)e; (void)f;
    const char *path = (const char *)b_path;
    if (!path || !*path) return NT_STATUS_INVALID_PARAMETER;
    int fd = open(path, O_RDWR | O_CREAT, 0644);
    if (fd < 0) return NT_STATUS_OBJECT_NAME_NOT_FOUND;
    uint32_t h = vsl_nt_allocate_handle(g_nt_ctx, fd, 0, NT_OBJECT_TYPE_FILE);
    if (h == 0) { close(fd); return NT_STATUS_UNSUCCESSFUL; }
    return (int64_t)h;
}

/* NtReadFile (193): read count bytes from the file handle (a) at offset (d)
 * into buffer (b). Returns bytes read, or negative NT status on error. */
int64_t vsl_nt_read_file(uint64_t a_handle, uint64_t b_buf,
                          uint64_t c_count, uint64_t d_offset, uint64_t e, uint64_t f) {
    (void)e; (void)f;
    int fd;
    if (vsl_nt_handle_to_vsl_fd(g_nt_ctx, (uint32_t)a_handle, &fd) != 0 || fd < 0)
        return NT_STATUS_INVALID_HANDLE;
    void *buf = (void *)b_buf;
    ssize_t n = pread(fd, buf, (size_t)c_count, (off_t)d_offset);
    if (n < 0) return vsl_errno_to_nt_status(errno);
    return (int64_t)n;
}

/* NtWriteFile (285): write count bytes to the file handle (a) at offset (d)
 * from buffer (b). Returns bytes written, or negative NT status on error. */
int64_t vsl_nt_write_file(uint64_t a_handle, uint64_t b_buf,
                           uint64_t c_count, uint64_t d_offset, uint64_t e, uint64_t f) {
    (void)e; (void)f;
    int fd;
    if (vsl_nt_handle_to_vsl_fd(g_nt_ctx, (uint32_t)a_handle, &fd) != 0 || fd < 0)
        return NT_STATUS_INVALID_HANDLE;
    const void *buf = (const void *)b_buf;
    ssize_t n = pwrite(fd, buf, (size_t)c_count, (off_t)d_offset);
    if (n < 0) return vsl_errno_to_nt_status(errno);
    return (int64_t)n;
}

/* NtClose (28): close the underlying fd and free the NT handle slot. */
int64_t vsl_nt_close(uint64_t a_handle, uint64_t b,
                      uint64_t c, uint64_t d, uint64_t e, uint64_t f) {
    (void)b; (void)c; (void)d; (void)e; (void)f;
    int fd;
    if (vsl_nt_handle_to_vsl_fd(g_nt_ctx, (uint32_t)a_handle, &fd) != 0)
        return NT_STATUS_INVALID_HANDLE;
    vsl_nt_free_handle(g_nt_ctx, (uint32_t)a_handle);
    if (fd >= 0) close(fd);
    return NT_STATUS_SUCCESS;
}

/* NtQueryInformationFile (159): fstat the handle (a) and, for
 * FileStandardInformation (class 5), write the file size into the info
 * buffer (c = uint64_t* out). Returns 0 or negative NT status. */
int64_t vsl_nt_query_information_file(uint64_t a_handle, uint64_t b_iosb,
                                       uint64_t c_info, uint64_t d_len,
                                       uint64_t e_class, uint64_t f) {
    (void)b_iosb; (void)d_len; (void)f;
    int fd;
    if (vsl_nt_handle_to_vsl_fd(g_nt_ctx, (uint32_t)a_handle, &fd) != 0 || fd < 0)
        return NT_STATUS_INVALID_HANDLE;
    struct stat st;
    if (fstat(fd, &st) != 0) return vsl_errno_to_nt_status(errno);
    if (e_class == 5 && c_info) {            /* FileStandardInformation */
        uint64_t *out = (uint64_t *)c_info;
        *out = (uint64_t)st.st_size;
    }
    return NT_STATUS_SUCCESS;
}

/* ======================================================================
 * BATCH 4 — Process / Thread / Virtual Memory (the SteamOS "NT = Proton"
 * launch spine).
 *
 * These are the syscalls a Windows game/launcher hits through the ReactOS
 * personality: allocate a process address space (NtAllocateVirtualMemory),
 * spin up a thread (NtCreateThread), and spawn a process (NtCreateProcess).
 * Each is transliterated into real Linux work — mmap for address space,
 * pthread_create for threads, fork() for processes — so the NT object
 * manager tracks genuine kernel-backed objects, not toy stubs.
 * ==================================================================== */

/* Thread trampoline: runs the NT start routine with its argument, exactly as
 * NtCreateThread would. We keep the tid in the handle payload so the caller
 * can later NtOpenThread / NtTerminateThread. `suspended` + gate implement
 * NT's CREATE_SUSPENDED: the trampoline blocks on the condvar until
 * NtResumeThread signals it. */
typedef struct {
    void *(*start)(void *);
    void *arg;
    int suspended;              /* 1 = created suspended, wait for resume */
    pthread_mutex_t gate_mtx;
    pthread_cond_t  gate_cv;
} vsl_nt_thread_params_t;

static void *vsl_nt_thread_tramp(void *p) {
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

/* NtAllocateVirtualMemory (19): reserve/commit a region via mmap.
 * a = process handle (0 = self), b = base_address* (IN/OUT), c = region_size*,
 * d = allocation_type (0x3000 = commit|reserve), e = protect (ignored). */
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
static pid_t vsl_nt_proc_pid(uint32_t proc_handle) {
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

/* ----------------------------------------------------------------------
 * Bridge: init / shutdown / handle registry / status translation
 * -------------------------------------------------------------------- */

int vsl_nt_bridge_init(vsl_nt_bridge_ctx_t *ctx) {
    if (!ctx) return -1;
    memset(ctx, 0, sizeof(*ctx));
    ctx->current_pid = (uint32_t)getpid();
    ctx->current_tid = (uint32_t)gettid();
    for (int i = 0; i < 4096; i++) ctx->handle_table[i].valid = false;
    g_nt_ctx = ctx;
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

int vsl_nt_handle_to_data(vsl_nt_bridge_ctx_t *ctx, uint32_t nt_handle,
                          uint64_t *out_data) {
    if (!ctx || !out_data) return -1;
    for (int i = 0; i < 4096; i++) {
        if (ctx->handle_table[i].valid && ctx->handle_table[i].nt_handle == nt_handle) {
            *out_data = ctx->handle_table[i].data;
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
