#ifndef WUBU_VSL_NT_INTERNAL_H
#define WUBU_VSL_NT_INTERNAL_H
/* Expose BSD/GNU extras (MADV_*, etc.) even when the Makefile sets
 * -D_POSIX_C_SOURCE. Must precede any system include. */
#define _DEFAULT_SOURCE
/* Shared surface for the E1 NT-bridge decomposition of vsl_syscall_nt.c.
 * Each vsl_nt_<subsys>.c implements a batch of transliterated NT handlers;
 * this header exposes the shared statics/helpers/types every submodule needs,
 * plus the handler prototypes and the per-module dispatch registration hooks.
 * C11, opaque where possible. */
#include "vsl_nt_bridge.h"
#include "vsl_syscall_internal.h"
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <dirent.h>
#include <pthread.h>
#include <semaphore.h>
#include <sys/mman.h>
#include <sys/eventfd.h>
#include <sys/uio.h>
#include <sys/statvfs.h>
#include <sys/timerfd.h>
#include <fcntl.h>
#include <sched.h>
#include <time.h>
#include <linux/futex.h>
#include <sys/syscall.h>
#include <limits.h>

/* Matches the NT-bridge function-pointer type defined in vsl_syscall_table.c. */
typedef int64_t (*vsl_syscall_fn_t)(uint64_t, uint64_t, uint64_t,
                                    uint64_t, uint64_t, uint64_t);

/* -- Shared statics owned by the facade (vsl_syscall_nt.c) -- */
extern vsl_nt_bridge_ctx_t *g_nt_ctx;
extern char g_nt_reg_root[512];
extern uint64_t g_nt_uuid_seed;

/* Atom table (batch 1) */
#define NT_ATOM_MAX   1024
#define NT_ATOM_NAME  256
typedef struct {
    char    name[NT_ATOM_NAME];
    uint32_t atom;
    bool    used;
} nt_atom_entry_t;
extern nt_atom_entry_t g_nt_atoms[NT_ATOM_MAX];
extern uint32_t        g_nt_atom_next;

/* Job table (batch 2) */
#define NT_JOB_MAX 256
typedef struct {
    uint32_t job_id;
    pid_t    pgid;
    bool     used;
    uint64_t basic_limit;  /* last NtSetInformationJobObject limit (class 0) */
} nt_job_entry_t;
extern nt_job_entry_t g_nt_jobs[NT_JOB_MAX];
extern uint32_t g_nt_job_next;

/* Live forked child processes (job sentinels, NtCreateProcess children, ...).
 * Recorded at fork() time so vsl_nt_bridge_shutdown can unconditionally reap
 * them even if the owning handle was closed mid-session — prevents leaking
 * sleeping pause() children when the caller (e.g. a regression test) exits. */
#define NT_CHILD_MAX 512
extern pid_t g_nt_child_pids[NT_CHILD_MAX];
extern int   g_nt_child_count;
/* Record a forked child pid (called from the parent branch right after fork). */
void vsl_nt_track_child(pid_t pid);
extern uint64_t g_nt_luid_counter;

/* Token / security subsystem (real privilege enforcement, not a stub).
 * A token holds a real set of privileges (LUID + attributes) and groups
 * (SIDs). NtAccessCheck actually compares a required privilege set against
 * the token's held privileges and returns ACCESS_DENIED when not satisfied. */
#define NT_TOKEN_MAX        512
#define NT_PRIV_MAX         64   /* privileges held by one token */
#define NT_GROUP_MAX        64   /* SIDs/groups held by one token */
typedef struct {
    uint32_t  luid_low;
    uint32_t  luid_high;
    uint32_t  attr;       /* SE_PRIVILEGE_ENABLED / _DISABLED / _REMOVED */
} nt_privilege_t;

typedef struct {
    uint32_t  sid;        /* simplified 32-bit SID for the bridge */
    uint32_t  attr;       /* SE_GROUP_ENABLED / _MANDATORY / _OWNER */
} nt_group_t;

typedef struct {
    bool      used;
    uint32_t  token_id;       /* opaque cookie returned to callers */
    uint64_t  luid_low, luid_high;  /* token authentication LUID */
    uint32_t  session_id;
    nt_privilege_t priv[NT_PRIV_MAX];
    uint32_t  priv_count;
    nt_group_t     group[NT_GROUP_MAX];
    uint32_t  group_count;
    uint32_t  imp_level;      /* impersonation level */
    bool      restricted;
} nt_token_entry_t;
extern nt_token_entry_t g_nt_tokens[NT_TOKEN_MAX];
extern uint32_t g_nt_token_next;

/* Well-known privilege LUIDs (NT constant values, low part; high = 0).
 * These are the stable public LUID values Windows assigns each privilege. */
#define NT_PRIV_SE_CREATE_TOKEN           0x00000002
#define NT_PRIV_SE_ASSIGNPRIMARYTOKEN     0x00000003
#define NT_PRIV_SE_LOCK_MEMORY            0x00000004
#define NT_PRIV_SE_INCREASE_QUOTA         0x00000005
#define NT_PRIV_SE_UNSOLICITED_INPUT      0x00000006
#define NT_PRIV_SE_MACHINE_ACCOUNT        0x00000007
#define NT_PRIV_SE_TCB                    0x00000008
#define NT_PRIV_SE_SECURITY               0x00000009
#define NT_PRIV_SE_TAKE_OWNERSHIP         0x0000000A
#define NT_PRIV_SE_LOAD_DRIVER            0x0000000B
#define NT_PRIV_SE_SYSTEM_PROFILE         0x0000000C
#define NT_PRIV_SE_SYSTEMTIME             0x0000000D
#define NT_PRIV_SE_PROF_SINGLE_PROCESS    0x0000000E
#define NT_PRIV_SE_INC_BASE_PRIORITY      0x0000000F
#define NT_PRIV_SE_CREATE_PAGEFILE        0x00000010
#define NT_PRIV_SE_CREATE_PERMANENT       0x00000011
#define NT_PRIV_SE_BACKUP                 0x00000012
#define NT_PRIV_SE_RESTORE                0x00000013
#define NT_PRIV_SE_SHUTDOWN               0x00000014
#define NT_PRIV_SE_DEBUG                  0x00000015
#define NT_PRIV_SE_AUDIT                  0x00000016
#define NT_PRIV_SE_SYSTEM_ENVIRONMENT     0x00000017
#define NT_PRIV_SE_CHANGE_NOTIFY          0x00000018
#define NT_PRIV_SE_REMOTE_SHUTDOWN        0x00000019
#define NT_PRIV_SE_UNDOCK                 0x0000001A
#define NT_PRIV_SE_SYNC_AGENT             0x0000001B
#define NT_PRIV_SE_ENABLE_DELEGATION      0x0000001C
#define NT_PRIV_SE_MANAGE_VOLUME          0x0000001D
#define NT_PRIV_SE_IMPERSONATE            0x0000001E
#define NT_PRIV_SE_CREATE_GLOBAL          0x0000001F
#define NT_PRIV_SE_TRUSTED_CREDMAN_ACCESS 0x00000020
#define NT_PRIV_SE_RELABEL                0x00000021
#define NT_PRIV_SE_INCREASE_WORKING_SET   0x00000022
#define NT_PRIV_SE_TIME_ZONE              0x00000023
#define NT_PRIV_SE_CREATE_SYMBOLIC_LINK   0x00000024

/* Privilege attribute flags (NT). */
#define NT_PRIV_ATTR_ENABLED      0x00000002
#define NT_PRIV_ATTR_ENABLED_BY_DEFAULT 0x00000001
#define NT_PRIV_ATTR_REMOVED      0x00000004
#define NT_PRIV_ATTR_USED_FOR_ACCESS 0x80000000

/* Helper: does token `t` hold privilege LUID (low) as ENABLED? */
bool vsl_nt_token_has_priv(const nt_token_entry_t *t, uint32_t luid_low);
/* Helper: add/remove a privilege from a token (NtAdjustPrivilegesToken). */
int  vsl_nt_token_set_priv(nt_token_entry_t *t, uint32_t luid_low, uint32_t attr);
/* Thread params (batches 4/6) */
typedef struct {
    void *(*start)(void *);
    void *arg;
    int suspended;
    pthread_mutex_t gate_mtx;
    pthread_cond_t  gate_cv;
} vsl_nt_thread_params_t;
/* Trampoline used by vsl_nt_create_thread (proc module). */
void *vsl_nt_thread_tramp(void *p);

/* Process-handle -> pid resolver (proc module) */
pid_t vsl_nt_proc_pid(uint32_t proc_handle);

/* Process creation extended (batch 5) */
int64_t vsl_nt_create_process_ex(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);

/* Handle-registry helpers live in the facade (vsl_nt_bridge.h declares them). */

/* -- Handler prototypes (every transliterated NT handler) -- */
int64_t vsl_nt_add_atom(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_alert_resume_thread(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_alert_thread(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_alloc_user_phys_pages(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_allocate_luid(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_allocate_uuids(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_allocate_virtual_memory(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_are_mapped_files_same(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_assign_job(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_cancel_io_file(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_clear_event(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_close(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_create_event(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_create_job_object(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_create_key(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_create_mutant(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_create_process(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_create_section(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_create_semaphore(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_create_thread(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_delay_execution(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_delete_atom(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_delete_key(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_duplicate_object(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_enumerate_key(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_enumerate_value_key(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_find_atom(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_flush_write_buffer(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_free_user_phys_pages(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_free_virtual_memory(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_is_process_in_job(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_map_view_of_section(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_unmap_view_of_section(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_open_event(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_open_file(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_open_job_object(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_open_key(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_open_process(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_open_thread(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_query_information_atom(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_query_information_file(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_query_information_process(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_query_system_information(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_query_system_time(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_query_value_key(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_read_file(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_read_virtual_memory(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_release_mutant(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_release_semaphore(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_reset_event(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_resume_thread(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_set_event(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_set_uuid_seed(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_set_value_key(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_terminate_job_object(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_terminate_process(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_wait_for_single_object(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_write_file(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_write_virtual_memory(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_create_io_completion(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_open_io_completion(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_query_io_completion(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_set_io_completion(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_remove_io_completion(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_create_symbolic_link_object(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_open_symbolic_link_object(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_create_event_pair(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_open_event_pair(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_create_named_pipe_file(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_create_mailslot_file(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_device_io_control_file(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_fs_control_file(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_create_waitable_port(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_create_port(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_open_port(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_connect_port(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_listen_port(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_accept_connect_port(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_complete_connect_port(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_request_wait_reply_port(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_reply_port(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);

/* --- vsl_nt_misc.c (blitz) --- */
int64_t vsl_nt_add_boot_entry(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_add_driver_entry(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_delete_boot_entry(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_delete_driver_entry(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_modify_boot_entry(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_modify_driver_entry(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_enumerate_boot_entries(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_enumerate_driver_entries(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_query_boot_entry_order(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_query_boot_options(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_set_boot_entry_order(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_set_boot_options(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_query_driver_entry_order(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_set_driver_entry_order(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);

/* --- Ordinal 0 syscalls --- */
int64_t vsl_nt_flush_write_buffer(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_is_system_resume_automatic(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_test_alert(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_yield_execution(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_query_port_information_process(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_get_current_processor_number(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_load_driver(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_unload_driver(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_load_key2(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_load_key_ex(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_unload_key2(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_unload_key_ex(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_save_key_ex(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_save_merged_keys(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_rename_key(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_lock_registry_key(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_lock_product_activation_keys(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_compress_key(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_make_permanent_object(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_query_open_sub_keys_ex(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_query_security_object(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_create_debug_object(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_debug_active_process(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_debug_continue(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_remove_process_debug(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_wait_for_debug_event(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_set_information_debug_object(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_query_debug_filter_state(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_set_debug_filter_state(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_system_debug_control(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_query_default_locale(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_set_default_locale(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_query_default_ui_language(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_set_default_ui_language(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_query_install_ui_language(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_query_system_environment_value(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_query_system_environment_value_ex(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_set_system_environment_value(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_set_system_environment_value_ex(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_enumerate_system_environment_values_ex(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_set_system_information(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_set_system_time(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_shutdown_system(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_set_system_power_state(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_initiate_power_action(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_power_information(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_get_device_power_state(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_cancel_device_wakeup_request(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_request_device_wakeup(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_request_wakeup_latency(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_is_system_resume_automatic(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_plug_play_control(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_get_plug_play_event(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_create_paging_file(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_apphelp_cache_control(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_callback_return(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_continue(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_create_profile(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_start_profile(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_stop_profile(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_query_interval_profile(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_set_interval_profile(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_test_alert(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_trace_event(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_translate_file_path(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_get_context_thread(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_set_context_thread(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_raise_exception(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_raise_hard_error(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_set_default_hard_error_port(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_set_event_boost_priority(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_signal_and_wait_for_single_object(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_get_current_processor_number(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_query_port_information_process(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_wait_for_multiple_objects32(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_queue_apc_thread(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_register_thread_terminate_port(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_get_write_watch(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_reset_write_watch(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_map_user_physical_pages(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_map_user_physical_pages_scatter(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_set_ldt_entries(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_set_high_event_pair(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_set_low_event_pair(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_set_high_wait_low_event_pair(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_set_low_wait_high_event_pair(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_wait_low_event_pair(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_query_ea_file(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_set_ea_file(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_query_quota_information_file(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_set_quota_information_file(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_read_file_scatter(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_write_file_gather(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_query_information_port(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_query_information_token(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_query_volume_information_file(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_vdm_control(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_reply_wait_receive_port(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_reply_wait_receive_port_ex(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_reply_wait_reply_port(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_request_port(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_request_wait_reply_port(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_reply_port(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_reply_wait_receive_port_ex(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_secure_connect_port(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_create_keyed_event(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_open_keyed_event(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_release_keyed_event(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_wait_for_keyed_event(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_create_transaction(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_open_transaction(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_commit_transaction(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_rollback_transaction(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_query_information_transaction(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_set_information_transaction(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_create_key_transacted(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_open_key_transacted(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);

/* -- Per-module dispatch registration: each submodule registers its handlers
 *    into the shared g_nt_dispatch[] table. -- */
void vsl_nt_atoms_register(vsl_syscall_fn_t *tbl, int size);
void vsl_nt_job_register(vsl_syscall_fn_t *tbl, int size);
void vsl_nt_io_register(vsl_syscall_fn_t *tbl, int size);
void vsl_nt_vmem_register(vsl_syscall_fn_t *tbl, int size);
void vsl_nt_process_register(vsl_syscall_fn_t *tbl, int size);
void vsl_nt_thread_register(vsl_syscall_fn_t *tbl, int size);
void vsl_nt_section_register(vsl_syscall_fn_t *tbl, int size);
void vsl_nt_timer_register(vsl_syscall_fn_t *tbl, int size);
void vsl_nt_sync_register(vsl_syscall_fn_t *tbl, int size);
void vsl_nt_registry_register(vsl_syscall_fn_t *tbl, int size);
void vsl_nt_token_register(vsl_syscall_fn_t *tbl, int size);
void vsl_nt_misc_register(vsl_syscall_fn_t *tbl, int size);
void vsl_nt_misc_w11_register(vsl_syscall_fn_t *tbl, int size);
void vsl_nt_alpc_register(vsl_syscall_fn_t *tbl, int size);
void vsl_nt_wnf_register(vsl_syscall_fn_t *tbl, int size);
void vsl_nt_worker_register(vsl_syscall_fn_t *tbl, int size);
void vsl_nt_enclave_register(vsl_syscall_fn_t *tbl, int size);
void vsl_nt_ioring_register(vsl_syscall_fn_t *tbl, int size);
void vsl_nt_partition_register(vsl_syscall_fn_t *tbl, int size);
void vsl_nt_ktm_register(vsl_syscall_fn_t *tbl, int size);

#endif /* WUBU_VSL_NT_INTERNAL_H */
