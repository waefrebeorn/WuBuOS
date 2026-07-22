/*
 * vsl_process.h  --  VSL Process Management API
 * Opaque struct pattern - only public API exposed
 */

#ifndef WUBUOS_VSL_PROCESS_H
#define WUBUOS_VSL_PROCESS_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* Forward declare opaque type */
struct VSL_PROC;
typedef struct VSL_PROC VSL_PROC;

/* VSL process states */
typedef enum {
    VSL_PROC_UNUSED = 0,
    VSL_PROC_READY,
    VSL_PROC_RUNNING,
    VSL_PROC_BLOCKED,
    VSL_PROC_ZOMBIE,
    VSL_PROC_DEAD,
} VSL_PROC_STATE;

/* Create a VSL process from an ELF binary.
 * elf_data: ELF binary in memory
 * elf_size: size of ELF binary
 * Returns PID, or -1 on error. */
int vsl_create_process(const void *elf_data, size_t elf_size);

/* Create a VSL process from a Mach-O binary.
 * macho_data: Mach-O binary in memory
 * macho_size: size of Mach-O binary
 * Returns PID, -1 on error, -2 if 32-bit unsupported. */
int vsl_create_process_macho(const void *macho_data, size_t macho_size);

/* Auto-detect binary type (ELF or Mach-O) and create process.
 * Returns PID or -1 on error. */
int vsl_create_process_any(const void *binary_data, size_t binary_size);

/* Destroy a VSL process.
 * Returns 0 on success. */
int vsl_destroy_process(uint32_t pid);

/* Get VSL process by PID. Returns NULL if not found. */
VSL_PROC *vsl_get_process(uint32_t pid);

/* List VSL processes.
 * Returns number of active processes. */
int vsl_list_processes(VSL_PROC *out, int max_count);

/* Get current VSL PID */
uint32_t vsl_get_current_pid(void);

/* Set current VSL PID (for fork/clone) */
void vsl_set_current_pid(uint32_t pid);

/* Process state accessors */
VSL_PROC_STATE vsl_proc_get_state(const VSL_PROC *proc);
uint32_t vsl_proc_get_pid(const VSL_PROC *proc);
uint32_t vsl_proc_get_ppid(const VSL_PROC *proc);
uint64_t vsl_proc_get_entry_point(const VSL_PROC *proc);
uint64_t vsl_proc_get_stack_pointer(const VSL_PROC *proc);
uint64_t vsl_proc_get_brk(const VSL_PROC *proc);
int vsl_proc_get_exit_code(const VSL_PROC *proc);

#endif /* WUBUOS_VSL_PROCESS_H */