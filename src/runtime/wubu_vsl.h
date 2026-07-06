/*
 * wubu_vsl.h  --  WuBuOS Virtualization Substrate Layer (VSL)
 * Main umbrella header - includes all VSL submodules
 */

#ifndef WUBUOS_VSL_H
#define WUBUOS_VSL_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* -- VSL Configuration ------------------------------------------- */

#define VSL_VERSION_MAJOR   1
#define VSL_VERSION_MINOR   0

/* Memory regions */
#define VSL_KERNEL_BASE     0x01000000  /* 16MB: VSL Linux kernel */
#define VSL_KERNEL_SIZE     0x01000000
#define VSL_USER_BASE       0x02000000ULL  /* ~1.75GB: VSL user space */
#define VSL_USER_SIZE       0x7E000000ULL
#define VSL_SHARED_BASE     0xFF000000  /* 1MB: WuBuOS ↔ VSL shared */
#define VSL_SHARED_SIZE     0x00100000

/* Maximum VSL resources */
#define VSL_MAX_PROCS       256
#define VSL_MAX_FDS         1024
#define VSL_MAX_MMAPS       512

/* VSL syscall numbers (see vsl_syscall_numbers.h for full list) */
#include "vsl/vsl_syscall_numbers.h"

/* -- Include submodule headers -- */
#include "vsl/vsl_syscall.h"
#include "vsl/vsl_process.h"
#include "vsl/vsl_memory.h"
#include "vsl/vsl_file.h"
#include "vsl/vsl_driver.h"
#include "vsl/vsl_shared.h"
#include "vsl/vsl_elf.h"

/* -- VSL API: Lifecycle ------------------------------------------ */

/*
 * Initialize VSL.
 * Sets up memory regions, IDT, process table, driver table.
 * Returns 0 on success.
 */
int vsl_init(void);

/*
 * Shutdown VSL.
 * Frees all resources, stops all VSL processes.
 */
void vsl_shutdown(void);

/*
 * Check if VSL is active.
 */
bool vsl_active(void);

/* -- VSL API: Diagnostics ---------------------------------------- */

/*
 * Get VSL info string.
 */
void vsl_info(char *buf, size_t buf_size);

/*
 * Dump VSL state for debugging.
 */
void vsl_dump_state(void);

/*
 * Get VSL statistics.
 */
void vsl_get_stats(uint64_t *out_syscalls, uint64_t *out_errors,
                   uint32_t *out_procs, uint32_t *out_drivers);

#endif /* WUBUOS_VSL_H */