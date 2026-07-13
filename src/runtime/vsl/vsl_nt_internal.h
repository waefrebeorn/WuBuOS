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
} nt_job_entry_t;
extern nt_job_entry_t g_nt_jobs[NT_JOB_MAX];
extern uint32_t g_nt_job_next;
extern uint64_t g_nt_luid_counter;

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

#endif /* WUBU_VSL_NT_INTERNAL_H */
