/*
 * vsl_internal.h  --  VSL Internal State (private)
 * Single source of truth for all VSL type definitions
 * NOT exposed in public API
 */

#ifndef WUBUOS_VSL_INTERNAL_H
#define WUBUOS_VSL_INTERNAL_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <sys/types.h>

/* Include public opaque type declarations first */
#include "vsl/vsl_process.h"
#include "vsl/vsl_file.h"
#include "vsl/vsl_memory.h"
#include "vsl/vsl_driver.h"

/* -- VSL Configuration ------------------------------------------- */

#define VSL_VERSION_MAJOR   1
#define VSL_VERSION_MINOR   0

/* Memory regions */
#define VSL_KERNEL_BASE     0x01000000  /* 16MB: VSL Linux kernel */
#define VSL_KERNEL_SIZE     0x01000000
#define VSL_USER_BASE       0x02000000ULL  /* ~1.75GB: VSL user space */
#define VSL_USER_SIZE       0x7E000000ULL
#define VSL_SHARED_BASE     0xFF000000  /* 1MB: WuBuOS <-> VSL shared */
#define VSL_SHARED_SIZE     0x00100000

/* Maximum VSL resources */
#define VSL_MAX_PROCS       256
#define VSL_MAX_FDS         1024
#define VSL_MAX_MMAPS       512

/* Full VSL_PROC definition (internal - public header has opaque) */
struct VSL_PROC {
    uint32_t         pid;
    uint32_t         ppid;
    VSL_PROC_STATE   state;
    uint64_t         entry_point;    /* ELF entry point */
    uint64_t         stack_pointer;  /* User stack */
    uint64_t         brk;            /* Heap break */
    uint64_t         mmap_base;      /* mmap allocation base */
    int              exit_code;
    uint64_t         regs[16];       /* Saved registers (rax-r15) */
    uint64_t         rip;
    uint64_t         rsp;
    uint64_t         rbp;
    uint64_t         flags;
};

/* Full VSL_FD definition (internal - public header has opaque) */
struct VSL_FD {
    int              fd;
    uint32_t         flags;
    uint32_t         mode;
    int              vsl_fd;         /* Host FD */
    char             path[256];
};

/* Full VSL_MMAP definition (internal - public header has opaque) */
struct VSL_MMAP {
    uint64_t         addr;
    size_t           size;
    int              prot;
    int              flags;
    int              fd;
    uint64_t         offset;
};

/* Full VSL_DRV definition (internal - public header has opaque) */
struct VSL_DRV {
    VSL_DRV_TYPE     type;
    bool             active;
    uint64_t         io_base;
    uint64_t         mem_base;
    size_t           mem_size;
    uint32_t         irq;
    void            *priv;
};

/* VSL global state */
typedef struct {
    bool             active;
    uint32_t         version_major;
    uint32_t         version_minor;

    /* Memory */
    uint64_t         kernel_base;
    size_t           kernel_size;
    uint64_t         user_base;
    size_t           user_size;
    uint64_t         shared_base;
    size_t           shared_size;

    /* Processes */
    VSL_PROC         procs[VSL_MAX_PROCS];
    uint32_t         n_procs;
    uint32_t         current_pid;

    /* File descriptors */
    VSL_FD           fds[VSL_MAX_FDS];
    uint32_t         n_fds;

    /* mmap regions */
    VSL_MMAP         mmaps[VSL_MAX_MMAPS];
    uint32_t         n_mmaps;

    /* Drivers */
    VSL_DRV          drivers[16];
    uint32_t         n_drivers;

    /* Syscall statistics */
    uint64_t         syscall_count;
    uint64_t         syscall_errors;

    /* Shared memory region (WuBuOS <-> VSL) */
    volatile uint64_t *shared_cmd;
    volatile uint64_t *shared_arg;
    volatile uint64_t *shared_ret;
    volatile uint64_t *shared_status;
} VSL_STATE;

/* Global state */
extern VSL_STATE g_vsl;

/* Internal helper functions */
static int vsl_get_host_fd(int vsl_fd);
static int find_free_vsl_pid(void);
static int register_child_pid(pid_t child_host_pid, uint32_t parent_vsl_pid);
static int vsl_openat(int dirfd, const char *pathname, int flags, mode_t mode);

#endif /* WUBUOS_VSL_INTERNAL_H */