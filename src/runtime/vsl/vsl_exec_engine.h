/*
 * vsl_exec_engine.h  --  VSL in-process execution engine (L11)
 *
 * Runs a STATIC freestanding x86-64 Linux ELF (ET_EXEC) inside the host
 * process: segments are mapped at their link addresses inside the VSL user
 * window, control transfers to e_entry on a private guest stack, and every
 * syscall the guest issues is trapped by a seccomp SECCOMP_RET_TRAP filter
 * scoped to the VSL user window and dispatched through the REAL
 * vsl_syscall_dispatch() (Linux personality).  exit/exit_group unwind back
 * to the caller via siglongjmp -- the host process never dies.
 *
 * Guest link addresses MUST fall inside [VSL_USER_BASE, VSL_USER_BASE +
 * VSL_USER_SIZE): that window is the seccomp trap scope, so guest code
 * anywhere else would run untrapped.  Build guests with e.g.
 *   gcc -nostdlib -static -ffreestanding -fno-pie -no-pie \
 *       -fno-stack-protector -Wl,-N -Wl,-Ttext-segment=0x40000000
 *
 * Output capture: host fds 1 and 2 are redirected to a memfd for the
 * duration of the run, so the REAL vsl_sys_write handler (which write()s
 * the host fd) lands in VslExecResult.output.  fd1+fd2 are merged.
 */
#ifndef WUBUOS_VSL_EXEC_ENGINE_H
#define WUBUOS_VSL_EXEC_ENGINE_H

#include <stdint.h>
#include <stddef.h>

#define VSL_EXEC_OUTPUT_MAX 65536

/* Run status / engine error codes (also the function return value). */
enum {
    VSL_EXEC_OK            = 0,   /* guest ran to exit/exit_group */
    VSL_EXEC_ERR_ARGS      = -1,  /* NULL args / empty image */
    VSL_EXEC_ERR_STATE     = -2,  /* VSL init failed or engine busy */
    VSL_EXEC_ERR_FORMAT    = -3,  /* not a static x86-64 ET_EXEC ELF */
    VSL_EXEC_ERR_WINDOW    = -4,  /* segments outside the VSL user window */
    VSL_EXEC_ERR_COLLISION = -5,  /* link range already mapped by host */
    VSL_EXEC_ERR_MAP       = -6,  /* mmap failed */
    VSL_EXEC_ERR_FILTER    = -7,  /* seccomp filter install failed */
    VSL_EXEC_ERR_ENTRY     = -8,  /* entry not inside a loaded exec segment */
    VSL_EXEC_ERR_FAULT     = -9,  /* guest SIGSEGV/SIGBUS (engine survived) */
    VSL_EXEC_ERR_SIGNAL    = -10  /* sigaction/setcontext setup failed */
};

typedef struct {
    int      status;            /* VSL_EXEC_* */
    int      exit_code;         /* guest exit status, recorded by the REAL
                                 * vsl_sys_exit handler (code & 0xFF) */
    uint32_t pid;               /* VSL process pid the guest ran as */
    uint64_t syscalls_trapped;  /* guest syscalls dispatched through VSL */
    uint64_t fault_addr;        /* si_addr when status == ERR_FAULT */
    size_t   output_len;        /* bytes captured from guest fd 1+2 */
    char     output[VSL_EXEC_OUTPUT_MAX];
} VslExecResult;

/* Run an in-memory ELF image to completion.  Returns VSL_EXEC_OK (0) when
 * the guest exited normally, a negative VSL_EXEC_ERR_* otherwise; *out
 * always carries the detailed status.  Single-threaded: one run at a time. */
int vsl_exec_elf_bytes(const void *elf_data, size_t elf_size,
                       VslExecResult *out);

/* Read a file and run it.  Same contract as vsl_exec_elf_bytes(). */
int vsl_exec_elf(const char *path, VslExecResult *out);

#endif /* WUBUOS_VSL_EXEC_ENGINE_H */
