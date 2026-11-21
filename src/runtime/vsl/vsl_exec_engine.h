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
 *
 * BOUNDED TLS MODEL (no glibc ABI, no ld.so): the HOST provides
 * one fixed-size TLS block per image (VSL_TLS_BLOCK_MAX bytes; main image is
 * seeded from VslImageCfg.tls_init, dependencies get a zeroed block).  An
 * STT_TLS symbol's st_value is an offset into that block.  Only three TLS
 * relocations are applied: R_X86_64_TPOFF64 writes the runtime address
 * tls_base + S + A (the guest dereferences it directly -- the host block base
 * plays the role of the thread pointer), R_X86_64_DTPMOD64 writes a bounded
 * module id (1 for the main image), and R_X86_64_DTPOFF32 writes S + A as a
 * 32-bit in-block offset.  A SYMBOL-LESS (STN_UNDEF) TPOFF64 -- the form real
 * glibc libc.so.6 uses for its own early TLS slots -- is bounded by the
 * DEFINED MODULE TLS BASE instead: S = 0, so the word written is tls_base + A.
 * Its addend must be a non-negative in-block offset that still holds one word;
 * out-of-range addends fail closed.  Every offset is range-checked against
 * the block.  STT_TLS symbols are NEVER accepted by the plain-address
 * resolver, and all other TLS models (TLSGD, TLSLD, GOTTPOFF, TPOFF32,
 * TLSDESC, ...) plus undefined/oversized TLS symbols are hard rejects (fail
 * closed).
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
 *     entries are read from the RELOCATED in-memory arrays.  Every target is
 *     BOUND to the declaring image's live mapped source window and charged to
 *     an explicit per-run call budget (VSL_EXEC_CTOR_CALL_BUDGET): a relocated
 *     target that lands outside that window -- e.g. a tampered array addend --
 *     fails closed with VSL_EXEC_ERR_RELOC before anything is called.  Both
 *     phases are guarded by the engine's fault handler; counts are reported
 *     in the result.  These are freestanding constructors, NOT glibc
 *     __libc_csu_init.
 * ELF symbol versioning (DT_VERSYM/DT_VERNEED/DT_VERDEF) is parsed from raw
 * gABI bytes.  A versioned import is satisfied ONLY by a definition carrying
 * the same version NAME; in the hardened mode (vsl_exec_image with all legacy
 * flags clear) the definition must ALSO come from the DT_VERNEED vn_file, so
 * a version required from libA cannot be satisfied by libB.  An undefined
 * WEAK symbol with no definition binds to address 0 in hardened mode; a
 * strong undefined symbol always fails closed.  Version tables are bounded
 * by EXPLICIT resource limits (per-image total records, raw chain-byte
 * budget, a compile-time VslVerCtx stack bound and per-array caps); the old
 * 32/32 caps are raised so a real-sized glibc table (41 VERDEFs) parses.  A
 * symbol-less TPOFF64 from a real glibc provider is now applied against the
 * defined module TLS base; 46 host IRELATIVE resolver entries (42 unique link
 * addresses) are registered for the same fixture as bounded stand-ins.  The
 * bounded host provider binds the full versioned ld.so surface -- all 19 UND
 * imports of a real glibc libc.so.6 (_dl_argv, __libc_stack_end,
 * __libc_enable_secure, _rtld_global, _rtld_global_ro, _dl_allocate_tls,
 * __tunable_get_val, ...) plus _dl_find_object@GLIBC_2.35 for the synthetic
 * gate -- ONLY on an exact DT_VERNEED name + vn_file match
 * (ld-linux-x86-64.so.2); a wrong vn_file or version fails closed.  With every
 * ld.so import bound, the merged-export ceiling was raised from 512 to a
 * FIXED 4096 (a real glibc libc.so.6 defines 3136 dynsyms) with an explicit
 * 2 MiB name/byte budget and a 2 MiB static-footprint assertion, so growth
 * stays bounded.  The real glibc fixture then advances past the cap: its
 * exports are merged and the main image's versioned imports bind from them.
 * Without the opt-in startup state the exact NEXT boundary is a guest fault
 * (VSL_EXEC_ERR_FAULT at 0xa0): __libc_start_main derefs the zeroed
 * _rtld_global._dl_rtld_map (l_info[DT_INIT] at +0xa0) -- a verified negative,
 * not a silent skip.  A vn_file retarget to libc.so.6 restores the _dl_argv
 * fault earlier.  With vsl_dyn_set_glibc_startup(1) that state is modelled:
 * a bounded link_map model is published as _rtld_global._dl_rtld_map and a
 * bounded x86-64 TCB (guards from AT_RANDOM) is installed as %fs, with
 * TPOFF64 words expressed relative to it.  RELR bitmaps now advance their
 * base per 63-slot word (real glibc's atexit list lives in a second word),
 * and the 42 IRELATIVE stand-ins are canonical callable host stubs rather
 * than a non-canonical sentinel (#GP).  A real glibc libc.so.6 then runs a
 * dynamically linked COPY guest to a correct exit(1) through the REAL
 * vsl_sys_exit; IFUNC semantics are now bounded too: a STT_GNU_IFUNC import
 * binds to a REAL host implementation selected by the exact (name, version,
 * vn_file) triple (memcpy/strlen/memset/puts), never to the defining object's
 * resolver address; unmodelled IFUNC symbols fail closed.  The remaining
 * boundaries are DSO DT_INIT/DT_FINI, the real allocator, a real puts/stdout
 * FILE state and full interpreter bootstrap (the real hello guest faults at
 * 0x3000008 after 3 syscalls) -- all still fail closed on guest fault.
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
#define VSL_TLS_BLOCK_MAX   256u   /* host-provided TLS bytes per image */
#define VSL_INTERP_PATH_MAX 256u   /* bounded PT_INTERP path length */
/* Fixed, explicit per-run budget for DT_INIT/DT_INIT_ARRAY/DT_FINI_ARRAY/
 * DT_FINI calls when init/fini handling is opted in.  A run that would
 * exceed it is rejected with VSL_EXEC_ERR_RELOC, never partially executed. */
#define VSL_EXEC_CTOR_CALL_BUDGET 64u

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
    VSL_EXEC_ERR_RELOCTYPE = -12, /* ET_DYN has unsupported relocation types */
    VSL_EXEC_ERR_INTERP    = -13  /* PT_INTERP path/rootfs/interpreter refused */
};

typedef struct {
    int      status;            /* VSL_EXEC_* */
    int      exit_code;         /* guest exit status, recorded by the REAL
                                 * vsl_sys_exit handler (code & 0xFF) */
    uint32_t pid;               /* VSL process pid the guest ran as */
    uint64_t syscalls_trapped;  /* guest syscalls dispatched through VSL */
    uint64_t fault_addr;        /* si_addr when status == ERR_FAULT */
    uint64_t fault_rip;         /* guest RIP at the exact faulting instruction */
    size_t   output_len;        /* bytes captured from guest fd 1+2 */
    char     output[VSL_EXEC_OUTPUT_MAX];
    /* ET_DYN diagnostics (zero for static ET_EXEC) */
    uint64_t load_bias;         /* bias applied to ET_DYN link addresses */
    uint32_t relocations_applied;/* relocations applied (all supported types) */
    uint32_t relocations_skipped;/* unsupported types -> RELOCTYPE error */
    uint32_t symbols_resolved;  /* symbol-based relocations resolved from
                                 * in-image definitions + host/dep table */
    /* Bounded TLS diagnostics (zero for static ET_EXEC / no TLS relocations) */
    uint32_t tls_relocations;   /* TPOFF64/DTPMOD64/DTPOFF32 relocations applied */
    uint64_t tls_base;          /* base of the host-provided TLS block (main image) */
    /* DT_NEEDED diagnostics (zero for static ET_EXEC / no dependencies) */
    uint32_t deps_loaded;       /* freestanding ET_DYN deps loaded, in order */
    /* Lazy-PLT / init-fini diagnostics (opt-in; zero by default) */
    uint32_t plt_slots_bound;   /* JUMP_SLOT GOTs given a lazy stub */
    uint32_t plt_resolves;      /* first-call resolver invocations this run */
    uint32_t init_called;       /* DT_INIT/INIT_ARRAY functions invoked */
    uint32_t fini_called;       /* DT_FINI_ARRAY/FINI functions invoked */
    uint32_t ctor_calls;        /* total calls charged to the per-run budget */
    /* PT_INTERP / guest-rootfs / auxv diagnostics (opt-in; zero otherwise) */
    uint32_t interp_present;    /* main image carries PT_INTERP */
    uint32_t interp_resolved;   /* bounded path resolved inside the rootfs */
    uint32_t interp_loaded;     /* interpreter ELF mapped + relocated */
    uint64_t interp_bias;       /* load bias applied to the interpreter */
    uint32_t auxv_entries;      /* auxv pairs built on the guest stack */
    char     interp_path[VSL_INTERP_PATH_MAX];   /* bounded path from PT_INTERP */
    char     interp_reason[128]; /* exact reason a glibc interpreter was refused */
    /* arch_prctl(ARCH_SET_FS) + TCB / real module-id TLS layout (opt-in) */
    uint64_t fs_base;           /* last ARCH_SET_FS base applied to the guest */
    uint64_t tcb_base;          /* address of the TCB header the engine seeded */
    uint32_t fs_syscalls;       /* arch_prctl calls trapped and handled this run */
    uint32_t tls_modules;       /* TLS modules with a real assigned module id */
    uint32_t rootfs_tried;      /* rootfs dirs searched for the interpreter */
    /* Exact first relocation failure (any image), empty on success.  Makes
     * a glibc-bootstrap negative verifiable down to the symbol/relocation. */
    char     reloc_failure[160];
    /* Bounded host backing for guest anonymous mmap (opt-in; zero otherwise).
     * A VSL mmap is only a registry entry; without backing a glibc malloc that
     * writes its top chunk to the returned address faults (0x3000008). */
    uint32_t mmap_backed;       /* guest mmaps materialized with host pages */
    uint64_t mmap_backed_bytes; /* total guest bytes backed this run */
    char     mmap_failure[160]; /* exact reason a guest mmap was refused */
    /* Bounded stdout/FILE model (opt-in; zero otherwise). */
    uint32_t stdout_writes;     /* bounded host stdout writes this run */
    uint64_t stdout_bytes;      /* exact bytes emitted by the bounded model */
    char     stdout_failure[160]; /* exact reason an unsupported shape refused */
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
/* Exact (name, DT_VERNEED version, DT_VERNEED vn_file) provider lookup used
 * to bind versioned ld.so imports from the bounded host table.  Returns 0
 * unless all three match exactly (differential gate: a wrong version or a
 * wrong vn_file fails closed). */
uint64_t vsl_dyn_host_symbol_ver(const char *name, const char *ver,
                                 const char *file);
uint64_t vsl_dyn_shim_call_count(void);   /* vsl_test_shim invocations */
uint64_t vsl_dyn_write_call_count(void);  /* host write-shim invocations */
uint64_t vsl_dyn_ifunc_call_count(void);  /* IRELATIVE host resolver calls */
/* Bounded stand-in value returned for the real-glibc IRELATIVE resolver
 * addresses registered in the host table.  The real resolvers pick guest
 * code at run time; the bounded model never runs guest bytes during
 * relocation.  The value is the address of a fixed host function that only
 * returns -- canonical and callable, so a real glibc indirect call through
 * a resolved GOT slot cannot #GP on a sentinel; it is NOT a faithful IFUNC
 * implementation (no member operations are modelled). */
uint64_t vsl_dyn_ifunc_libc_value(void);

/* Bounded IFUNC resolver selected by EXACT symbol name/version/vn_file.
 * An STT_GNU_IFUNC import is bound to the REAL host implementation returned
 * here (memcpy/strlen/memset/puts), never to the defining object's resolver
 * address.  Returns 0 when the (name, version, file) triple is not modelled:
 * the loader then fails closed -- no guessed implementation and no guest
 * resolver bytes run.  Versioned lookups require the exact version AND vn_file;
 * an unversioned lookup uses ver="" and file=NULL.  The counter reports how
 * many times a resolved host implementation was actually invoked by guest
 * code across runs (differential-gate evidence). */
uint64_t vsl_dyn_ifunc_resolve_named(const char *name, const char *ver,
                                     const char *file);
uint64_t vsl_dyn_ifunc_named_call_count(void);

/* Base of the HOST-provided TLS block used by the main image of the last
 * run (0 before any run).  Exposed so a gate can assert the exact address a
 * TPOFF64 relocation wrote into a guest GOT slot. */
uint64_t vsl_tls_base(void);

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
    /* Host-provided INITIAL bytes for this image's TLS block.  Copied into
     * the block at run start (rest zeroed); NULL/len 0 = all-zero block.
     * len > VSL_TLS_BLOCK_MAX is rejected with VSL_EXEC_ERR_ARGS. */
    const uint8_t *tls_init;
    size_t         tls_init_len;
    /* Guest rootfs directory used to resolve an absolute PT_INTERP path.
     * NULL = no rootfs (a PT_INTERP is then refused when interp handling is
     * enabled).  Appended at the END so older positional initialisers stay
     * source-compatible. */
    const char *rootfs;
    /* Ordered guest-rootfs search list (NULL-terminated), searched IN ORDER
     * for the PT_INTERP path; the first confinement-passing hit wins.  When
     * non-NULL this takes precedence over the single `rootfs` above and the
     * process-wide list.  Appended at the END for source compatibility. */
    const char *const *rootfs_dirs;
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

/* Bounded merged-export table controls.  The table is a FIXED static array of
 * VSL_DEP_EXPORT_CEILING entries (covers a real glibc libc.so.6's 3136 defined
 * dynsyms); a caller can only LOWER the effective cap, never grow the table.
 * vsl_dyn_set_export_cap(0) restores the ceiling; a value above the ceiling
 * is rejected (-1).  The two accessors report the last run's state so a gate
 * can prove the raised bound was actually used and stayed bounded. */
#define VSL_DEP_EXPORT_CEILING 4096u
int      vsl_dyn_set_export_cap(uint32_t cap);
uint32_t vsl_dyn_dep_export_cap(void);
uint32_t vsl_dyn_dep_export_count(void);

/* Opt-in bounded enhancements.  Both are OFF by default (pre-change
 * behaviour).  Enable before a run; they persist until changed.
 *   lazy_plt: DT_JMPREL JUMP_SLOTs point at generated stubs, bound on first
 *     call through the bounded resolver table (see header comment).
 *   init_fini: run DT_INIT/INIT_ARRAY before entry and DT_FINI_ARRAY/FINI on
 *     unload, for dependencies then the main image. */
void vsl_dyn_set_lazy_plt(int on);
void vsl_dyn_set_init_fini(int on);

/* Opt-in bounded PT_INTERP handling.  OFF by default (pre-change behavior:
 * PT_INTERP was ignored and the main e_entry was called directly).  When ON,
 * an ET_EXEC/ET_DYN image with PT_INTERP is handled as a real kernel would:
 *   - the path is parsed with strict bounds (absolute, no "..", NUL inside
 *     p_filesz, length <= VSL_INTERP_PATH_MAX);
 *   - the absolute path is re-rooted inside the allow-listed guest rootfs
 *     (per-image VslImageCfg.rootfs, or the process-wide one below) with
 *     realpath confinement -- nothing is ever read from the host root;
 *   - the interpreter is mapped and relocated with the SAME bounded loader
 *     as a DT_NEEDED dependency, and its DT_INIT/DT_FINI join the existing
 *     init/fini owner (interpreter first, deps leaf-first, main last; fini
 *     in reverse).  Control transfers to the interpreter's e_entry.
 *   - the guest stack carries the full auxv an ld.so needs: AT_PHDR,
 *     AT_PHENT, AT_PHNUM, AT_BASE (interpreter bias), AT_ENTRY (main entry),
 *     AT_PAGESZ, AT_RANDOM and AT_NULL.
 * A real glibc ld.so needs unmodeled %fs/TCB, TLS and versioned-symbol
 * machinery; when the bounded interpreter load fails the engine fails closed
 * with VSL_EXEC_ERR_INTERP and an exact reason in VslExecResult.interp_reason.
 */
void vsl_dyn_set_interp(int on);
/* OPT-IN: bind the main image's versioned libc imports from a bounded GUEST
 * libc shim (a DT_NEEDED ET_DYN provider with real DT_VERDEF records) while a
 * real ld.so is loaded as PT_INTERP.  OFF by default (pre-change behaviour:
 * a genuine ld-linux interpreter owns main relocation and VSL transfers
 * control to the interpreter's e_entry).  When ON, the ld.so is still mapped
 * and self-relocated inside the guest rootfs, but VSL owns main relocation,
 * loads the main DT_NEEDED graph from the per-image dep dirs, resolves the
 * versioned imports from that shim's guest bytes, and starts at AT_ENTRY. */
void vsl_dyn_set_interp_bind_main(int on);
/* Process-wide guest rootfs allow-list (NULL clears it).  A per-image cfg
 * rootfs takes precedence; both refuse paths escaping the directory. */
int  vsl_dyn_set_rootfs(const char *dir);
/* Ordered guest-rootfs search list (LD_LIBRARY_PATH-like).  Directories are
 * appended in call order and searched in that order for the PT_INTERP path;
 * the first confinement-passing hit wins.  Each dir is bounded and copied.
 * Returns 0 on success, -1 on a bad/oversized path or a full list. */
int  vsl_dyn_add_rootfs(const char *dir);
void vsl_dyn_clear_rootfs_list(void);
/* OPT-IN: resolve a plain-basename DT_NEEDED provider (e.g. libc.so.6) from
 * the configured guest rootfs directories in standard library sub-dirs, with
 * realpath confinement, instead of requiring a dep-search-dir allow-list.
 * This is how the real COPY-from-libc fixture finds its libc data provider.
 * OFF by default so the dep-dir-only loader and its fail-closed gates are
 * unchanged. */
int  vsl_dyn_set_libc_rootfs(int on);
/* Lifetime count of first-call PLT resolver invocations (for differential
 * gates).  Per-run count is in VslExecResult.plt_resolves. */
uint64_t vsl_dyn_plt_resolve_count(void);

/* Opt-in arch_prctl(ARCH_SET_FS)/TCB + real module-id TLS layout.  OFF by
 * default (pre-change behaviour).  When ON, a guest `arch_prctl` trapping
 * into the engine can set its own %fs base: the engine validates the base is
 * inside the VSL user window, seeds a minimal TCB header there (self pointer
 * at %fs:0, dtv pointer at %fs:8) and switches the running thread's FS base
 * for the duration of guest execution (host libc runs on the saved host FS).
 * ARCH_GET_FS returns the recorded guest base.  Unknown arch_prctl options
 * fail closed with -EINVAL. */
void vsl_dyn_set_fs_tls(int on);
/* Opt-in bounded glibc startup state, OFF by default.  Models the state a
 * kernel + interpreter install before __libc_start_main on the direct-entry
 * path: a fixed-size link_map model published as _rtld_global._dl_rtld_map
 * (DT_INIT/DT_INIT_ARRAY read NULL -- no interpreter constructor runs), and a
 * fixed-size x86-64 startup TCB (self, dtv, pthread self, stack/pointer
 * guards from AT_RANDOM) installed as %fs at guest entry.  TPOFF64 words are
 * expressed relative to that base so real glibc's %fs-relative TLS accesses
 * land in the bounded host TLS blocks.  All state is host static storage
 * with explicit bounds; unmodelled startup bytes still fault fail-closed. */
void vsl_dyn_set_glibc_startup(int on);
/* Address of the host TCB/dtv backing store used by the LAST run (for gates). */
uint64_t vsl_dyn_tcb_base(void);
/* Module id assigned at index idx in the last run's TLS layout, or 0. */
uint32_t vsl_dyn_module_id(uint32_t idx);

/* Bounded stdout/FILE stream model (opt-in, OFF by default).  Real glibc's
 * puts writes through an unmodelled FILE/stdout buffer whose internal write
 * count(ptr) can be garbage (a bogus 0x2502000 write followed by -EFAULT,
 * leaking ~12 KB of trailing bytes).  When ON, the engine binds the guest's
 * FUNC-typed puts import to a bounded HOST implementation selected by the
 * exact (name, DT_VERNEED version, vn_file) triple.  That implementation:
 *   - measures the NUL-terminated string with a bounded scan,
 *   - refuses any stream shape it cannot model (wrong fd, oversized string,
 *     missing terminator) with an exact diagnostic and emits NO bytes,
 *   - writes exactly the string bytes plus one newline, and reports the exact
 *     byte count.  No guest FILE state is read or written, so the exit flush
 *     has nothing pending and no bogus write/EFAULT can occur. */
#define VSL_STDOUT_WRITE_MAX 4096u
void     vsl_dyn_set_stdout_model(int on);
int      vsl_dyn_stdout_model(void);
/* Validate one stdout stream shape without emitting anything.  Returns 0
 * when the (fd,count) pair is modelable, -1 and an exact diagnostic otherwise.
 * Used by the fresh-process gate as a fail-closed negative control. */
int      vsl_stdout_shape_ok(uint64_t fd, uint64_t count,
                             char *diag, size_t diag_cap);

/* Bounded host backing for guest MAP_PRIVATE|MAP_ANONYMOUS mmap, OFF by
 * default (pre-change registry-only behaviour).  VSL's mmap syscall records a
 * range in the process registry but maps no host pages; a real glibc malloc
 * then faults writing its arena's top chunk at the returned address.  When
 * ON, a successful guest anonymous mmap is materialized as zeroed host pages
 * with MAP_FIXED_NOREPLACE at the SAME guest address.  Only anonymous private
 * maps are modelled; a file-backed / non-anonymous map keeps the legacy
 * registry-only semantics and is never counted as backed.
 *
 * The feature is bounded by a fixed 32-entry table, a 16 MiB per-map limit and
 * a 64 MiB total byte ceiling.  Any overrun, occupied host range, failed host
 * mmap or out-of-window range is FAIL-CLOSED: the registry entry is rolled
 * back, the guest sees -ENOMEM instead of an unbacked success, and the exact
 * reason is reported in VslExecResult.mmap_failure.  Backed host pages are
 * unmapped at the end of each run, so guest state cannot leak across runs. */
#define VSL_MMAP_BACK_CEILING (64ULL * 1024 * 1024)
#define VSL_MMAP_BACK_SINGLE  (16ULL * 1024 * 1024)
void     vsl_dyn_set_mmap_backing(int on);
/* Set the effective total byte budget (0..VSL_MMAP_BACK_CEILING).
 * Returns 0 on success, -1 for an out-of-range value.  0 refuses ALL backing. */
int      vsl_dyn_set_mmap_backing_max(uint64_t max_bytes);
uint64_t vsl_dyn_mmap_backing_max(void);

#endif /* WUBUOS_VSL_EXEC_ENGINE_H */
