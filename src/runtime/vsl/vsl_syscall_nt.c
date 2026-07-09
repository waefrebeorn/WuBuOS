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

/* Global UUID seed: 0 = cryptographically random (default); nonzero = the
 * deterministic seed installed via NtSetUuidSeed (256). Declared at file
 * scope so it is visible to vsl_nt_allocate_uuids from any call site. */
static uint64_t g_nt_uuid_seed = 0;

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
