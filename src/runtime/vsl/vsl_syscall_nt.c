/*
 * vsl_syscall_nt.c -- ReactOS NT syscall -> VSL transliteration layer (E1).
 *
 * FACADE of a decomposed dispatch table. Real handlers live in modules:
 *   vsl_nt_atoms.c (batch 1)   vsl_nt_job.c    (batch 2)
 *   vsl_nt_io.c    (batch 3)   vsl_nt_proc.c   (batches 4+5)
 *   vsl_nt_sync.c  (batch 6)   vsl_nt_registry.c (batch 7)
 * Each module registers its handlers into g_nt_dispatch[] via a
 * vsl_nt_<subsys>_register() call below. 58/297 NT syscalls transliterated;
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

/* Shared dispatch table. */
extern int64_t vsl_sys_nosys(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);

#define NT_TBL_SIZE 297
static vsl_syscall_fn_t g_nt_dispatch[NT_TBL_SIZE];

static void nt_dispatch_init(void) {
    for (int i = 0; i < NT_TBL_SIZE; i++) g_nt_dispatch[i] = vsl_sys_nosys;
    vsl_nt_atoms_register(g_nt_dispatch, NT_TBL_SIZE);
    vsl_nt_job_register(g_nt_dispatch, NT_TBL_SIZE);
    vsl_nt_io_register(g_nt_dispatch, NT_TBL_SIZE);
    vsl_nt_proc_register(g_nt_dispatch, NT_TBL_SIZE);
    vsl_nt_sync_register(g_nt_dispatch, NT_TBL_SIZE);
    vsl_nt_registry_register(g_nt_dispatch, NT_TBL_SIZE);
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
 * NT -> VSL syscall dispatch (the bridge entry point)
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
