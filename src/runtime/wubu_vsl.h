/*
 * wubu_vsl.h  --  WuBuOS Virtualization Substrate Layer (VSL)
 *
 * VSL is WuBuOS's "Proton"  --  a lightweight Linux VM that runs under
 * WuBuOS ring-0. It provides:
 *
 *   - Linux binary execution (ELF loader)
 *   - Linux syscall bridge (syscalls → WuBuOS kernel calls)
 *   - Driver passthrough (Vulkan, CUDA, networking, GPU)
 *   - Near-native performance via direct hardware access
 *
 * Architecture:
 *   WuBuOS (ring-0) owns all hardware
 *   VSL runs a minimal Linux kernel in a lightweight VM
 *   Linux apps run inside VSL, thinking they have full Linux
 *   VSL translates Linux syscalls to WuBuOS kernel calls
 *   Hardware drivers (Vulkan, CUDA) are accessed directly  --  no emulation
 *
 * This is the "Proton within Proton" design:
 *   - WuBuOS is the host (like SteamOS)
 *   - VSL is the compatibility layer (like Proton/Wine)
 *   - Linux apps run inside VSL (like Windows games in Proton)
 *   - But VSL also provides Linux kernel services to WuBuOS itself
 *
 * Memory layout:
 *   WuBuOS kernel: 0x00000000 - 0x00FFFFFF (16MB, identity-mapped)
 *   VSL Linux kernel: 0x01000000 - 0x01FFFFFF (16MB, VM-mapped)
 *   VSL user space: 0x02000000 - 0x7FFFFFFF (user VM)
 *   WuBuOS user space: 0x80000000 - 0xFFFFFFFF (user native)
 *   Shared memory (WuBuOS ↔ VSL): 0xFF000000 - 0xFF0FFFFF (1MB)
 *
 * Interrupt routing:
 *   Hardware interrupts → WuBuOS IDT first
 *   WuBuOS forwards to VSL if interrupt belongs to VSL device
 *   VSL has its own IDT for Linux syscall handling (int 0x80, syscall/sysret)
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

/* Maximum VSL processes */
#define VSL_MAX_PROCS       256
#define VSL_MAX_FDS         1024
#define VSL_MAX_MMAPS       512

/* VSL syscall numbers (Linux x86_64 ABI) */
#define VSL_SYS_READ        0
#define VSL_SYS_WRITE       1
#define VSL_SYS_OPEN        2
#define VSL_SYS_CLOSE       3
#define VSL_SYS_STAT        4
#define VSL_SYS_FSTAT       5
#define VSL_SYS_LSEEK       8
#define VSL_SYS_MMAP        9
#define VSL_SYS_MUNMAP      11
#define VSL_SYS_BRK         12
#define VSL_SYS_IOCTL       16
#define VSL_SYS_ACCESS      21
#define VSL_SYS_PIPE        22
#define VSL_SYS_SELECT      23
#define VSL_SYS_SCHED_YIELD 24
#define VSL_SYS_FORK        57
#define VSL_SYS_VFORK       58
#define VSL_SYS_EXECVE      59
#define VSL_SYS_EXIT        60
#define VSL_SYS_WAIT4       61
#define VSL_SYS_KILL        62
#define VSL_SYS_GETPID      39
#define VSL_SYS_GETPPID     110
#define VSL_SYS_GETUID      102
#define VSL_SYS_GETGID      104
#define VSL_SYS_CLONE       56
#define VSL_SYS_PREAD64     17
#define VSL_SYS_PWRITE64    18
#define VSL_SYS_READV       19
#define VSL_SYS_WRITEV      20
#define VSL_SYS_DUP         32
#define VSL_SYS_DUP2        33
#define VSL_SYS_FCNTL       72
#define VSL_SYS_FSYNC       74
#define VSL_SYS_TRUNCATE    76
#define VSL_SYS_FTRUNCATE   77
#define VSL_SYS_GETCWD      79
#define VSL_SYS_CHDIR       80
#define VSL_SYS_RENAME      82
#define VSL_SYS_MKDIR       83
#define VSL_SYS_RMDIR       84
#define VSL_SYS_CREAT       85
#define VSL_SYS_UNLINK      87
#define VSL_SYS_SYMLINK     88
#define VSL_SYS_READLINK    89
#define VSL_SYS_CHMOD       90
#define VSL_SYS_CHOWN       92
#define VSL_SYS_LSTAT       6
#define VSL_SYS_POLL        7
#define VSL_SYS_EPOLL_CREATE 213
#define VSL_SYS_EPOLL_CTL   233
#define VSL_SYS_EPOLL_WAIT  232
#define VSL_SYS_SOCKET      41
#define VSL_SYS_CONNECT     42
#define VSL_SYS_ACCEPT      43
#define VSL_SYS_SENDTO      44
#define VSL_SYS_RECVFROM    45
#define VSL_SYS_SHUTDOWN    48
#define VSL_SYS_BIND        49
#define VSL_SYS_LISTEN      50
#define VSL_SYS_GETSOCKNAME 51
#define VSL_SYS_GETPEERNAME 52
#define VSL_SYS_SETSOCKOPT  54
#define VSL_SYS_FUTEX       202
#define VSL_SYS_RT_SIGACTION 13
#define VSL_SYS_RT_SIGPROCMASK 14
#define VSL_SYS_CLOCK_GETTIME 228
#define VSL_SYS_EXIT_GROUP  231
#define VSL_SYS_OPENAT      257
#define VSL_SYS_NEWFSTATAT  262
#define VSL_SYS_UNLINKAT    263
#define VSL_SYS_FACCESSAT   269
#define VSL_SYS_PIPE2       293
#define VSL_SYS_CLONE3      435
#define VSL_SYS_WAITPID     VSL_SYS_WAIT4  /* waitpid maps to wait4 */
/* VSL_SYS_PIPE2 already defined as 293 above */
#define VSL_SYS_SOCKETPAIR  53

/* VSL process states */
typedef enum {
    VSL_PROC_UNUSED = 0,
    VSL_PROC_READY,
    VSL_PROC_RUNNING,
    VSL_PROC_BLOCKED,
    VSL_PROC_ZOMBIE,
    VSL_PROC_DEAD,
} VSL_PROC_STATE;

/* VSL process context */
typedef struct {
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
} VSL_PROC;

/* VSL file descriptor */
typedef struct {
    int              fd;
    uint32_t         flags;
    uint32_t         mode;
    int              vsl_fd;         /* VSL internal FD */
    char             path[256];
} VSL_FD;

/* VSL mmap region */
typedef struct {
    uint64_t         addr;
    size_t           size;
    int              prot;
    int              flags;
    int              fd;
    uint64_t         offset;
} VSL_MMAP;

/* VSL driver types */
typedef enum {
    VSL_DRV_NONE = 0,
    VSL_DRV_GPU_VULKAN,     /* Vulkan GPU passthrough */
    VSL_DRV_GPU_CUDA,       /* CUDA GPU passthrough */
    VSL_DRV_NET,            /* Network interface */
    VSL_DRV_BLOCK,          /* Block device */
    VSL_DRV_INPUT,          /* Keyboard/mouse */
    VSL_DRV_DISPLAY,        /* Display output */
    VSL_DRV_AUDIO,          /* Audio device */
    VSL_DRV_USB,            /* USB controller */
    VSL_DRV_PCI,            /* PCI bus */
} VSL_DRV_TYPE;

/* VSL driver state */
typedef struct {
    VSL_DRV_TYPE     type;
    bool             active;
    uint64_t         io_base;        /* I/O port or MMIO base */
    uint64_t         mem_base;       /* Memory-mapped region */
    size_t           mem_size;
    uint32_t         irq;            /* IRQ number */
    void            *priv;           /* Driver-private data */
} VSL_DRV;

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

    /* Shared memory region (WuBuOS ↔ VSL) */
    volatile uint64_t *shared_cmd;    /* Command from WuBuOS */
    volatile uint64_t *shared_arg;    /* Argument */
    volatile uint64_t *shared_ret;    /* Return value */
    volatile uint64_t *shared_status; /* Status flags */
} VSL_STATE;

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

/* -- VSL API: Process Management --------------------------------- */

/*
 * Create a VSL process from an ELF binary.
 * elf_data: ELF binary in memory
 * elf_size: size of ELF binary
 * Returns PID, or -1 on error.
 */
int vsl_create_process(const void *elf_data, size_t elf_size);

/*
 * Destroy a VSL process.
 * Returns 0 on success.
 */
int vsl_destroy_process(uint32_t pid);

/*
 * Get VSL process by PID.
 */
VSL_PROC *vsl_get_process(uint32_t pid);

/*
 * List VSL processes.
 * Returns number of active processes.
 */
int vsl_list_processes(VSL_PROC *out, int max_count);

/* -- VSL API: Syscall Bridge ------------------------------------- */

/*
 * Handle a VSL syscall.
 * rax = syscall number, rdi-r9 = arguments
 * Returns result in rax.
 */
int64_t vsl_syscall(uint64_t num, uint64_t rdi, uint64_t rsi,
                    uint64_t rdx, uint64_t r10, uint64_t r8, uint64_t r9);

/*
 * Dispatch syscall by number.
 * This is the main syscall handler called from VSL interrupt context.
 */
int64_t vsl_syscall_dispatch(uint64_t num, uint64_t *regs);

/* -- VSL API: Memory Management ---------------------------------- */

/*
 * Map memory in VSL address space.
 * Returns virtual address, or 0 on failure.
 */
uint64_t vsl_mmap(uint64_t addr, size_t size, int prot, int flags,
                  int fd, uint64_t offset);

/*
 * Unmap memory in VSL address space.
 */
int vsl_munmap(uint64_t addr, size_t size);

/*
 * Set VSL heap break (brk).
 * Returns new brk, or -1 on failure.
 */
int64_t vsl_brk(uint64_t new_brk);

/* -- VSL API: File Operations ------------------------------------ */

/*
 * Open a file in VSL.
 * Returns FD number, or -1 on error.
 */
int vsl_open(const char *path, int flags, int mode);

/*
 * Close a file in VSL.
 */
int vsl_close(int fd);

/*
 * Read from a file in VSL.
 */
int64_t vsl_read(int fd, void *buf, size_t count);

/*
 * Write to a file in VSL.
 */
int64_t vsl_write(int fd, const void *buf, size_t count);

/*
 * Seek in a file.
 */
int64_t vsl_lseek(int fd, int64_t offset, int whence);

/* -- VSL API: Driver Management ---------------------------------- */

/*
 * Register a VSL driver.
 * Returns driver ID, or -1 on error.
 */
int vsl_register_driver(VSL_DRV_TYPE type, uint64_t io_base,
                        uint64_t mem_base, size_t mem_size, uint32_t irq);

/*
 * Activate a VSL driver.
 */
int vsl_activate_driver(int drv_id);

/*
 * Deactivate a VSL driver.
 */
int vsl_deactivate_driver(int drv_id);

/*
 * Check if a driver type is active.
 */
bool vsl_driver_active(VSL_DRV_TYPE type);

/*
 * Get driver by type.
 */
VSL_DRV *vsl_get_driver(VSL_DRV_TYPE type);

/* -- VSL API: Shared Memory -------------------------------------- */

/*
 * Send command to VSL via shared memory.
 * Blocks until VSL acknowledges.
 */
int vsl_send_cmd(uint64_t cmd, uint64_t arg);

/*
 * Read VSL response from shared memory.
 */
uint64_t vsl_read_response(void);

/*
 * Check VSL status.
 */
uint64_t vsl_get_status(void);

/* -- VSL API: ELF Loading ---------------------------------------- */

/*
 * Parse and validate an ELF64 binary.
 * Returns 0 on success, fills out_entry with entry point.
 */
int vsl_elf_validate(const void *elf_data, size_t elf_size,
                     uint64_t *out_entry);

/*
 * Load an ELF64 binary into VSL address space.
 * Returns entry point, or 0 on failure.
 */
uint64_t vsl_elf_load(const void *elf_data, size_t elf_size);

/*
 * Get ELF interpreter (dynamic linker) path.
 * Returns path in buf, or -1 if statically linked.
 */
int vsl_elf_interpreter(const void *elf_data, size_t elf_size,
                        char *buf, size_t buf_size);

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
