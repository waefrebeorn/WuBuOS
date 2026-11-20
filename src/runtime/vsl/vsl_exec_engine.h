/*
 * vsl_exec_engine.h  --  VSL in-process execution engine (L11)
 *
 * Runs a freestanding x86-64 Linux ELF (static ET_EXEC, or ET_DYN with
 * bounded dynamic relocations) inside the host process: segments are mapped
 * at their link addresses (ET_EXEC) or at a chosen in-window base (ET_DYN)
 * inside the VSL user window, control transfers to e_entry on a private
 * guest stack, and every syscall the guest issues is trapped by a seccomp
 * SECCOMP_RET_TRAP filter scoped to the VSL user window and dispatched
 * through the REAL vsl_syscall_dispatch() (Linux personality).
 * exit/exit_group unwind back to the caller via siglongjmp -- the host
 * process never dies.
 *
 * ET_DYN relocation support is deliberately BOUNDED, not a general dynamic
 * linker: R_X86_64_RELATIVE, R_X86_64_64, R_X86_64_GLOB_DAT,
 * R_X86_64_JUMP_SLOT, R_X86_64_COPY and R_X86_64_IRELATIVE are applied from
 * DT_RELA/DT_REL/DT_JMPREL.  Symbol tables/counts come from DT_HASH or
 * DT_GNU_HASH and are capped.  Undefined symbols resolve ONLY through a small
 * fixed host table (see vsl_dyn_host_symbol): a test syscall shim, a host
 * write() shim and two test data strings.  COPY may only source bytes from
 * host symbols that declare a byte limit; IRELATIVE resolvers must be listed
 * in a fixed host IFUNC table (no arbitrary guest code runs during relocation).
 * Definitions inside the image resolve to bias+st_value.  Unknown symbol
 * names, unregistered IFUNC resolvers and any other relocation type are hard
 * rejects (VSL_EXEC_ERR_RELOC / VSL_EXEC_ERR_RELOCTYPE) -- never guessed.
 * There is NO TLS model: TPOFF/DTPMOD/TLSDESC and STT_TLS symbols are all
 * rejected.  No glibc, no ld.so.
 *
 * Two bounded enhancements are available as OPT-IN flags (both OFF by
 * default, so every pre-existing guest behaviour is unchanged):
 *   - vsl_dyn_set_lazy_plt(1): DT_JMPREL JUMP_SLOT GOT entries are pointed at
 *     generated per-slot stubs instead of their resolved target.  The FIRST
 *     call through a slot enters a bounded resolver table, which verifies the
 *     slot index, patches that GOT entry to the real target and tail-jumps
 *     there; later calls go straight to the target.  The resolver table is
 *     capped and only resolves names already validated by the normal bounded
 *     symbol resolver.  No arbitrary guest code runs at resolve time.
 *   - vsl_dyn_set_init_fini(1): DT_INIT / DT_INIT_ARRAY are invoked after
 *     relocation and before the entry point, DT_FINI_ARRAY (reverse order) /
 *     DT_FINI on unload, across the main image and its DT_NEEDED deps.  Array
 *     entries are read from the RELOCATED in-memory arrays.  Both phases are
 *     guarded by the engine's fault handler; counts are reported in the
 *     result.  These are freestanding constructors, NOT glibc __libc_csu_init.
 * ELF symbol versioning (DT_VERSYM/DT_VERNEED/DT_VERDEF) is parsed from raw
 * gABI bytes.  A versioned import is satisfied ONLY by a definition carrying
 * the same version NAME; in the hardened mode (vsl_exec_image with all legacy
 * flags clear) the definition must ALSO come from the DT_VERNEED vn_file, so
 * a version required from libA cannot be satisfied by libB.  An undefined
 * WEAK symbol with no definition binds to address 0 in hardened mode; a
 * strong undefined symbol always fails closed.
 *
 * DT_NEEDED is supported ONLY in this bounded form: DT_NEEDED sonames are a
 * plain basename found in a bounded per-image search-directory list (or the
 * legacy process-wide allow-list via vsl_dyn_set_dep_search_dir()).  A dep
 * must be a freestanding ET_DYN guest object (no ld.so), mapped at its own
 * in-window bias.  Its exported dynamic symbols are merged into the bounded
 * symbol table (strong beats weak within a version, then first wins), so a
 * main image can import from it.  Counts, name lengths, depth, file sizes and
 * total mapped bytes are capped.  A missing, malformed, out-of-tree or
 * unknown dependency is a hard VSL_EXEC_ERR_RELOC; nothing is ever guessed
 * or loaded from the host system.  Loaded ET_DYN images must stay within
 * this contract.
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
    VSL_EXEC_ERR_SIGNAL    = -10, /* sigaction/setcontext setup failed */
    VSL_EXEC_ERR_RELOC     = -11, /* ET_DYN relocation table malformed */
    VSL_EXEC_ERR_RELOCTYPE = -12  /* ET_DYN has unsupported relocation types */
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
    /* ET_DYN diagnostics (zero for static ET_EXEC) */
    uint64_t load_bias;         /* bias applied to ET_DYN link addresses */
    uint32_t relocations_applied;/* relocations applied (all supported types) */
    uint32_t relocations_skipped;/* unsupported types -> RELOCTYPE error */
    uint32_t symbols_resolved;  /* symbol-based relocations resolved from
                                 * in-image definitions + host/dep table */
    /* DT_NEEDED diagnostics (zero for static ET_EXEC / no dependencies) */
    uint32_t deps_loaded;       /* freestanding ET_DYN deps loaded, in order */
    /* Lazy-PLT / init-fini diagnostics (opt-in; zero by default) */
    uint32_t plt_slots_bound;   /* JUMP_SLOT GOTs given a lazy stub */
    uint32_t plt_resolves;      /* first-call resolver invocations this run */
    uint32_t init_called;       /* DT_INIT/INIT_ARRAY functions invoked */
    uint32_t fini_called;       /* DT_FINI_ARRAY/FINI functions invoked */
} VslExecResult;

/* Run an in-memory ELF image to completion.  Returns VSL_EXEC_OK (0) when
 * the guest exited normally, a negative VSL_EXEC_ERR_* otherwise; *out
 * always carries the detailed status.  Single-threaded: one run at a time. */
int vsl_exec_elf_bytes(const void *elf_data, size_t elf_size,
                       VslExecResult *out);

/* Read a file and run it.  Same contract as vsl_exec_elf_bytes(). */
int vsl_exec_elf(const char *path, VslExecResult *out);

/* The bounded fixed host-symbol table used by the ET_DYN resolver.  Returns
 * the runtime address of a host-provided symbol, or 0 when the name is not
 * provided (the loader then fails closed).  Exposed so the differential
 * gate can assert the EXACT address written into a guest GOT entry. */
uint64_t vsl_dyn_host_symbol(const char *name);
uint64_t vsl_dyn_shim_call_count(void);   /* vsl_test_shim invocations */
uint64_t vsl_dyn_write_call_count(void);  /* host write-shim invocations */
uint64_t vsl_dyn_ifunc_call_count(void);  /* IRELATIVE host resolver calls */

/* Bounded DT_NEEDED dependency loading.  Set the ONLY directory from which
 * DT_NEEDED objects may be loaded (an allow-listed path in the repo/test
 * tree).  Each soname must be a plain basename inside it; paths that escape
 * the directory (../, symlinks, absolute names) are refused.  NULL (or an
 * empty string) clears the allow-list, so any DT_NEEDED then fails closed
 * with VSL_EXEC_ERR_RELOC.  Returns 0 on success, -1 on a bad directory. */
int vsl_dyn_set_dep_search_dir(const char *dir);
/* A per-image, isolated execution configuration.  Unlike the process-wide
 * vsl_dyn_set_dep_search_dir() allow-list, `dirs` belongs to ONE image (and
 * its DT_NEEDED graph); it is copied into per-run state at entry and never
 * mutates global state.  Both legacy flags default to 0 (hardened behavior):
 *   legacy_version_any_file: 1 = pre-change rule that matches a version NAME
 *     across ALL merged deps, ignoring DT_VERNEED vn_file.
 *   legacy_weak_undef_fail: 1 = pre-change rule that fails closed for an
 *     undefined weak symbol with no definition (instead of binding to 0).
 * Both flags exist only so differential gates can exercise the old loader on
 * byte-identical fixtures; hardened callers leave them 0. */
#define VSL_IMAGE_DIRS_MAX 8u
typedef struct {
    const char *const *dirs;          /* NULL-terminated; NULL = no dep dirs */
    int legacy_version_any_file;
    int legacy_weak_undef_fail;
} VslImageCfg;

/* Hardened per-image run.  Returns the same codes as vsl_exec_elf_bytes();
 * additionally VSL_EXEC_ERR_ARGS if the dir list exceeds VSL_IMAGE_DIRS_MAX
 * or contains an invalid/oversized path. */
int vsl_exec_image(const void *elf_data, size_t elf_size,
                   const VslImageCfg *cfg, VslExecResult *out);

/* Load-order view of the LAST run: soname at index idx, or NULL if absent. */
const char *vsl_dyn_dep_name(uint32_t idx);
/* Runtime address of a symbol exported by a loaded dependency, or 0 if the
 * name is not in the merged (bounded) dependency symbol table. */
uint64_t vsl_dyn_dep_symbol(const char *name);

/* Opt-in bounded enhancements.  Both are OFF by default (pre-change
 * behaviour).  Enable before a run; they persist until changed.
 *   lazy_plt: DT_JMPREL JUMP_SLOTs point at generated stubs, bound on first
 *     call through the bounded resolver table (see header comment).
 *   init_fini: run DT_INIT/INIT_ARRAY before entry and DT_FINI_ARRAY/FINI on
 *     unload, for dependencies then the main image. */
void vsl_dyn_set_lazy_plt(int on);
void vsl_dyn_set_init_fini(int on);
/* Lifetime count of first-call PLT resolver invocations (for differential
 * gates).  Per-run count is in VslExecResult.plt_resolves. */
uint64_t vsl_dyn_plt_resolve_count(void);

#endif /* WUBUOS_VSL_EXEC_ENGINE_H */
