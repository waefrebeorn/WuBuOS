/*
 * vsl_process.c  --  VSL Process Management Implementation
 */

#define _GNU_SOURCE
#include "vsl/vsl_internal.h"
#include "vsl/vsl_process.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>

/* Global state (defined in vsl.c) */
extern VSL_STATE g_vsl;

/* -- PID Mapping -------------------------------------------------- */

static int find_free_vsl_pid(void) {
    for (uint32_t pid = 2; pid < 100000; pid++) {
        if (!vsl_get_process(pid)) return (int)pid;
    }
    return -1;
}

static int register_child_pid(pid_t child_host_pid, uint32_t parent_vsl_pid) {
    if (child_host_pid <= 0) return -1;
    if (g_vsl.n_procs >= VSL_MAX_PROCS) return -1;
    int vsl_pid = find_free_vsl_pid();
    if (vsl_pid < 0) return -1;
    VSL_PROC *proc = &g_vsl.procs[g_vsl.n_procs];
    memset(proc, 0, sizeof(*proc));
    proc->pid = (uint32_t)vsl_pid;
    proc->ppid = parent_vsl_pid;
    proc->state = VSL_PROC_READY;
    g_vsl.n_procs++;
    return vsl_pid;
}

/* -- Process Management ------------------------------------------- */

int vsl_create_process(const void *elf_data, size_t elf_size) {
    if (!g_vsl.active) return -1;
    if (g_vsl.n_procs >= VSL_MAX_PROCS) return -1;

    uint64_t entry;
    if (vsl_elf_validate(elf_data, elf_size, &entry) != 0) return -1;

    uint32_t pid = g_vsl.n_procs + 1;
    VSL_PROC *proc = &g_vsl.procs[g_vsl.n_procs];
    memset(proc, 0, sizeof(*proc));
    proc->pid = pid;
    proc->ppid = g_vsl.current_pid;
    proc->state = VSL_PROC_READY;
    proc->entry_point = entry;
    proc->stack_pointer = VSL_USER_BASE + VSL_USER_SIZE - 0x1000ULL;
    proc->brk = VSL_USER_BASE + 0x100000;
    proc->mmap_base = VSL_USER_BASE + 0x1000000;

    g_vsl.n_procs++;
    return (int)pid;
}

int vsl_destroy_process(uint32_t pid) {
    VSL_PROC *proc = vsl_get_process(pid);
    if (!proc) return -1;
    proc->state = VSL_PROC_DEAD;
    return 0;
}

VSL_PROC *vsl_get_process(uint32_t pid) {
    for (uint32_t i = 0; i < g_vsl.n_procs; i++) {
        if (g_vsl.procs[i].pid == pid) return &g_vsl.procs[i];
    }
    return NULL;
}

int vsl_list_processes(VSL_PROC *out, int max_count) {
    int count = 0;
    for (uint32_t i = 0; i < g_vsl.n_procs && count < max_count; i++) {
        if (g_vsl.procs[i].state != VSL_PROC_UNUSED &&
            g_vsl.procs[i].state != VSL_PROC_DEAD) {
            out[count++] = g_vsl.procs[i];
        }
    }
    return count;
}

uint32_t vsl_get_current_pid(void) {
    return g_vsl.current_pid;
}

void vsl_set_current_pid(uint32_t pid) {
    g_vsl.current_pid = pid;
}

/* -- Process Accessors -------------------------------------------- */

VSL_PROC_STATE vsl_proc_get_state(const VSL_PROC *proc) {
    return proc ? proc->state : VSL_PROC_UNUSED;
}

uint32_t vsl_proc_get_pid(const VSL_PROC *proc) {
    return proc ? proc->pid : 0;
}

uint32_t vsl_proc_get_ppid(const VSL_PROC *proc) {
    return proc ? proc->ppid : 0;
}

uint64_t vsl_proc_get_entry_point(const VSL_PROC *proc) {
    return proc ? proc->entry_point : 0;
}

uint64_t vsl_proc_get_stack_pointer(const VSL_PROC *proc) {
    return proc ? proc->stack_pointer : 0;
}

uint64_t vsl_proc_get_brk(const VSL_PROC *proc) {
    return proc ? proc->brk : 0;
}

int vsl_proc_get_exit_code(const VSL_PROC *proc) {
    return proc ? proc->exit_code : 0;
}