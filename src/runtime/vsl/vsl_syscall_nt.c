/*
 * vsl_syscall_nt.c -- ReactOS NT syscall -> VSL transliteration layer (E1).
 *
 * FACADE of a decomposed dispatch table. Real handlers live in modules:
 *   vsl_nt_atoms.c (batch 1)   vsl_nt_job.c    (batch 2)
 *   vsl_nt_io.c    (batch 3)   vsl_nt_vmem.c / vsl_nt_process.c / vsl_nt_thread.c
 *                          / vsl_nt_section.c / vsl_nt_timer.c  (batches 4+5)
 *   vsl_nt_sync.c  (batch 6)   vsl_nt_registry.c (batch 7)
 * Each module registers its handlers into g_nt_dispatch[] via a
 * vsl_nt_<subsys>_register() call below. 117/297 NT syscalls transliterated;
 * the rest fall through to vsl_sys_nosys (VSL_NT_MAP_STUB).
 *
 * Reference study: reactos-study/reactos/ntoskrnl/ + dll/ntdll/
 * C11, no nested functions.
 */

#include "vsl_nt_bridge.h"
#include "vsl_syscall_internal.h"
#include "vsl_nt_internal.h"
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdint.h>
#include <errno.h>
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

/* Global UUID seed: 0 = cryptographically random (default); nonzero = the
 * deterministic seed installed via NtSetUuidSeed (256). */
uint64_t g_nt_uuid_seed = 0;

/* Active bridge context (set in vsl_nt_bridge_init). */
vsl_nt_bridge_ctx_t *g_nt_ctx = NULL;

/* Registry root: per-process directory tree implementing the NT registry as
 * real files (key = dir, value = file). */
char g_nt_reg_root[512] = {0};

/* Atom table (batch 1) */
#define NT_ATOM_MAX   1024
#define NT_ATOM_NAME  256

nt_atom_entry_t g_nt_atoms[NT_ATOM_MAX];
uint32_t        g_nt_atom_next = 0x0001;

/* Job table (batch 2) */
nt_job_entry_t g_nt_jobs[NT_JOB_MAX];

uint32_t g_nt_job_next = 0x1000;

extern uint64_t g_nt_luid_counter;

/* Token table (real privilege enforcement). */
nt_token_entry_t g_nt_tokens[NT_TOKEN_MAX];

uint32_t g_nt_token_next = 0x2000;

/* Shared dispatch table. */
extern int64_t vsl_sys_nosys(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);

#define NT_TBL_SIZE 490
static vsl_syscall_fn_t g_nt_dispatch[NT_TBL_SIZE];

static void nt_dispatch_init(void) {
    for (int i = 0; i < NT_TBL_SIZE; i++) g_nt_dispatch[i] = vsl_sys_nosys;
    vsl_nt_atoms_register(g_nt_dispatch, NT_TBL_SIZE);
    vsl_nt_job_register(g_nt_dispatch, NT_TBL_SIZE);
    vsl_nt_io_register(g_nt_dispatch, NT_TBL_SIZE);
    vsl_nt_vmem_register(g_nt_dispatch, NT_TBL_SIZE);
    vsl_nt_process_register(g_nt_dispatch, NT_TBL_SIZE);
    vsl_nt_thread_register(g_nt_dispatch, NT_TBL_SIZE);
    vsl_nt_section_register(g_nt_dispatch, NT_TBL_SIZE);
    vsl_nt_timer_register(g_nt_dispatch, NT_TBL_SIZE);
    vsl_nt_sync_register(g_nt_dispatch, NT_TBL_SIZE);
    vsl_nt_registry_register(g_nt_dispatch, NT_TBL_SIZE);
    vsl_nt_token_register(g_nt_dispatch, NT_TBL_SIZE);
    vsl_nt_misc_register(g_nt_dispatch, NT_TBL_SIZE);
    
    /* Windows 11 (24H2) extended syscalls */
    vsl_nt_wnf_register(g_nt_dispatch, NT_TBL_SIZE);
    vsl_nt_enclave_register(g_nt_dispatch, NT_TBL_SIZE);
    vsl_nt_partition_register(g_nt_dispatch, NT_TBL_SIZE);
    vsl_nt_ioring_register(g_nt_dispatch, NT_TBL_SIZE);
    vsl_nt_worker_register(g_nt_dispatch, NT_TBL_SIZE);
    vsl_nt_ktm_register(g_nt_dispatch, NT_TBL_SIZE);
    vsl_nt_alpc_register(g_nt_dispatch, NT_TBL_SIZE);
    vsl_nt_misc_w11_register(g_nt_dispatch, NT_TBL_SIZE);
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
    /* Registry root: /tmp/wubu_nt_reg_<pid>. Real files back the NT registry. */
    snprintf(g_nt_reg_root, sizeof(g_nt_reg_root), "/tmp/wubu_nt_reg_%d", (int)getpid());
    mkdir(g_nt_reg_root, 0755);
    return 0;
}

/* Live-child tracker (see vsl_nt_internal.h). */
pid_t g_nt_child_pids[NT_CHILD_MAX];
int   g_nt_child_count = 0;

void vsl_nt_track_child(pid_t pid) {
    if (pid <= 0 || g_nt_child_count >= NT_CHILD_MAX) return;
    g_nt_child_pids[g_nt_child_count++] = pid;
}

void vsl_nt_bridge_shutdown(vsl_nt_bridge_ctx_t *ctx) {
    if (!ctx) return;
    /* Reap every child process the bridge forked during the session so the
     * caller (e.g. the regression test) never leaks sleeping sentinel
     * processes that block in pause() forever. We reap from three sources:
     *  (a) explicitly tracked forked children (bulletproof, survives handle
     *      close mid-session),
     *  (b) job sentinels still in g_nt_jobs[],
     *  (c) live PROCESS/THREAD handles whose data field holds a child pid. */
    for (int i = 0; i < g_nt_child_count; i++) {
        pid_t pid = g_nt_child_pids[i];
        if (pid > 0) {
            kill(pid, SIGKILL);
            int status = 0; pid_t wr;
            for (int t = 0; t < 20; t++) {
                wr = waitpid(pid, &status, WNOHANG);
                if (wr > 0 || wr < 0) break;
                usleep(10000);
            }
        }
    }
    g_nt_child_count = 0;
    for (int i = 0; i < NT_JOB_MAX; i++) {
        if (g_nt_jobs[i].used && g_nt_jobs[i].pgid > 0) {
            kill(g_nt_jobs[i].pgid, SIGKILL);
            int status = 0; pid_t wr;
            for (int t = 0; t < 20; t++) {
                wr = waitpid(g_nt_jobs[i].pgid, &status, WNOHANG);
                if (wr > 0 || wr < 0) break;
                usleep(10000);
            }
        }
    }
    for (int i = 0; i < 4096; i++) {
        if (ctx->handle_table[i].valid &&
            (ctx->handle_table[i].type == NT_OBJECT_TYPE_PROCESS ||
             ctx->handle_table[i].type == NT_OBJECT_TYPE_THREAD)) {
            pid_t pid = (pid_t)ctx->handle_table[i].data;
            if (pid > 0) {
                kill(pid, SIGKILL);
                int status = 0; pid_t wr;
                for (int t = 0; t < 20; t++) {
                    wr = waitpid(pid, &status, WNOHANG);
                    if (wr > 0 || wr < 0) break;
                    usleep(10000);
                }
            }
        }
    }
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

/* Object-type <-> name mapping (used by NtQueryObject and for diagnostics). */
const char *vsl_nt_object_type_name(nt_object_type_t type) {
    switch (type) {
        case NT_OBJECT_TYPE_PROCESS:         return "Process";
        case NT_OBJECT_TYPE_THREAD:          return "Thread";
        case NT_OBJECT_TYPE_FILE:            return "File";
        case NT_OBJECT_TYPE_DIRECTORY:       return "Directory";
        case NT_OBJECT_TYPE_SYMBOLIC_LINK:   return "SymbolicLink";
        case NT_OBJECT_TYPE_EVENT:           return "Event";
        case NT_OBJECT_TYPE_MUTANT:          return "Mutant";
        case NT_OBJECT_TYPE_SEMAPHORE:       return "Semaphore";
        case NT_OBJECT_TYPE_TIMER:           return "Timer";
        case NT_OBJECT_TYPE_KEY:             return "Key";
        case NT_OBJECT_TYPE_SECTION:         return "Section";
        case NT_OBJECT_TYPE_JOB:             return "Job";
        case NT_OBJECT_TYPE_PORT:            return "Port";
        case NT_OBJECT_TYPE_TOKEN:           return "Token";
        default:                             return "Unknown";
    }
}

nt_object_type_t vsl_nt_object_type_from_name(const char *nt_type_name) {
    if (!nt_type_name) return NT_OBJECT_TYPE_UNKNOWN;
    /* Linear scan is fine: the enum has ~25 entries. */
    for (nt_object_type_t t = NT_OBJECT_TYPE_UNKNOWN;
         t <= NT_OBJECT_TYPE_WORK_ITEM; t = (nt_object_type_t)((int)t + 1)) {
        if (strcmp(vsl_nt_object_type_name(t), nt_type_name) == 0) return t;
    }
    return NT_OBJECT_TYPE_UNKNOWN;
}

/* ----------------------------------------------------------------------
 * NT -> VSL syscall dispatch (the bridge entry point)
 * -------------------------------------------------------------------- */

int64_t vsl_nt_syscall_dispatch(vsl_nt_bridge_ctx_t *ctx,
                                 uint16_t nt_syscall_num,
                                 uint64_t *args,
                                 int n_args) {
    (void)ctx;
    if (nt_syscall_num == 0) {
        /* Handle ordinal 0 syscalls specially */
        static int inited_zero = 0;
        static vsl_syscall_fn_t g_nt_dispatch_zero[6]; /* 6 syscalls with ordinal 0 */
        if (!inited_zero) {
            g_nt_dispatch_zero[0] = vsl_nt_flush_write_buffer;       /* NtFlushWriteBuffer (0) */
            g_nt_dispatch_zero[1] = vsl_nt_is_system_resume_automatic; /* NtIsSystemResumeAutomatic (0) */
            g_nt_dispatch_zero[2] = vsl_nt_test_alert;               /* NtTestAlert (0) */
            g_nt_dispatch_zero[3] = vsl_nt_yield_execution;          /* NtYieldExecution (0) */
            g_nt_dispatch_zero[4] = vsl_nt_query_port_information_process; /* NtQueryPortInformationProcess (0) */
            g_nt_dispatch_zero[5] = vsl_nt_get_current_processor_number;    /* NtGetCurrentProcessorNumber (0) */
            inited_zero = 1;
        }
        /* Since all ordinal 0, we need a way to distinguish - for now pick first available */
        for (int i = 0; i < 6; i++) {
            if (g_nt_dispatch_zero[i] && g_nt_dispatch_zero[i] != vsl_sys_nosys) {
                return g_nt_dispatch_zero[i](0, 0, 0, 0, 0, 0);
            }
        }
        return NT_STATUS_NOT_IMPLEMENTED;
    }
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
