/*
 * vsl_exec_engine.c  --  VSL in-process execution engine (L11)
 *
 * VSL could LOAD binaries but never transferred control to them.  This leaf
 * closes that gap for static freestanding Linux ELF: real entry transfer,
 * real syscall trapping, real dispatch through the ONE Linux syscall table.
 *
 * Mechanism (all probed on this host before construction):
 *   1. Load: vsl_elf_validate() (single owner of format validation), then
 *      map the PT_LOAD union at the LINK addresses with mmap(MAP_FIXED)
 *      inside the VSL user window.  The legacy vsl_elf_load() relocates
 *      into VSL_USER_BASE + (vaddr & 0xFFFFFF) for inspection; execution
 *      needs the original absolute addresses, so the engine maps segments
 *      itself after the same validation.
 *   2. Trap: a seccomp-BPF filter returns SECCOMP_RET_TRAP for every
 *      syscall whose instruction_pointer lies in the VSL user window
 *      [VSL_USER_BASE, VSL_USER_BASE+VSL_USER_SIZE) and ALLOW otherwise.
 *      Scoping by IP is what makes in-process trapping possible: the VSL
 *      handlers themselves call host libc, and those syscalls (issued from
 *      libc text, outside the window) must run for real.  The filter is
 *      immutable once installed, so the window -- not a per-run range --
 *      is the trap scope; it is installed once per host process.
 *   3. SIGSYS (SA_SIGINFO): si_syscall gives the number.  NOTE: the kernel
 *      SIGSYS siginfo carries NO argument array (only si_call_addr /
 *      si_syscall / si_arch); the six arguments are read from the trapped
 *      ucontext registers RDI/RSI/RDX/R10/R8/R9.  Dispatch goes through
 *      vsl_syscall_dispatch() -> kernel interchange -> Linux table.
 *      The result is written to REG_RAX.  On this kernel (7.0) SIGSYS is
 *      delivered with RIP ALREADY advanced past the 2-byte syscall; older
 *      kernels point RIP at it.  The engine advances RIP only when the
 *      bytes at RIP are still 0F 05 -- correct under both behaviors.
 *   4. exit/exit_group: dispatched through the REAL vsl_sys_exit handler
 *      (records ZOMBIE + exit_code on the VSL_PROC), then siglongjmp back
 *      to the engine caller.  No host process death.
 *   5. Entry: getcontext/setcontext with REG_RSP pointing at a built
 *      argc/argv/envp/auxv image on a private guest stack, REG_RIP=e_entry.
 *   6. Output: fds 1/2 are dup2'd onto a memfd for the run, so the REAL
 *      vsl_sys_write (host write() on the fd) is captured verbatim.
 *
 * Guest SIGSEGV/SIGBUS is caught and reported as VSL_EXEC_ERR_FAULT; the
 * engine cleans up and stays runnable.
 */

#include "../../egojar.h"
#include "vsl/vsl_internal.h"
#include "vsl/vsl_syscall.h"
#include "vsl/vsl_syscall_numbers.h"
#include "vsl/vsl_elf.h"
#include "vsl/vsl_process.h"
#include "vsl/vsl_exec_engine.h"
#include "../wubu_kernel_xchg.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <setjmp.h>
#include <ucontext.h>
#include <linux/seccomp.h>
#include <linux/filter.h>
#include <linux/audit.h>

extern VSL_STATE g_vsl;

#define EX_WIN_LO  ((uint64_t)VSL_USER_BASE)
#define EX_WIN_HI  (EX_WIN_LO + (uint64_t)VSL_USER_SIZE)
#define EX_STACK_SIZE (1024 * 1024)

/* -- per-run state (single-threaded engine) ------------------------- */
static struct {
    volatile sig_atomic_t active;   /* guest is on the CPU */
    int running;                    /* a run owns the engine */
    sigjmp_buf jb;
    volatile int reason;            /* 1 = exit, 2 = fault */
    volatile uint64_t trapped;
    volatile uint64_t fault_addr;
} g_ex;

static int g_filter_installed;
static struct sigaction g_old_sigsys, g_old_sigsegv, g_old_sigbus;

/* -- seccomp filter: TRAP every syscall issued from the VSL window --- */
static int ex_install_filter(void) {
    if (g_filter_installed) return 0;

    /* seccomp_data layout: nr@0, arch@4, instruction_pointer@8 (u64),
     * args@16.  BPF loads are 32-bit, so the IP is tested as two halves;
     * the whole VSL window lives below 4GB, so a nonzero high half means
     * host code -> ALLOW. */
    struct sock_filter f[] = {
        BPF_STMT(BPF_LD|BPF_W|BPF_ABS, 4),                       /* arch */
        BPF_JUMP(BPF_JMP|BPF_JEQ|BPF_K, AUDIT_ARCH_X86_64, 1, 0),
        BPF_STMT(BPF_RET|BPF_K, SECCOMP_RET_KILL_PROCESS),
        BPF_STMT(BPF_LD|BPF_W|BPF_ABS, 12),                      /* ip hi */
        BPF_JUMP(BPF_JMP|BPF_JEQ|BPF_K, 0, 1, 0),
        BPF_STMT(BPF_RET|BPF_K, SECCOMP_RET_ALLOW),
        BPF_STMT(BPF_LD|BPF_W|BPF_ABS, 8),                       /* ip lo */
        BPF_JUMP(BPF_JMP|BPF_JGE|BPF_K, (uint32_t)EX_WIN_LO, 1, 0),
        BPF_STMT(BPF_RET|BPF_K, SECCOMP_RET_ALLOW),
        BPF_JUMP(BPF_JMP|BPF_JGT|BPF_K, (uint32_t)(EX_WIN_HI - 1), 0, 1),
        BPF_STMT(BPF_RET|BPF_K, SECCOMP_RET_ALLOW),
        BPF_STMT(BPF_RET|BPF_K, SECCOMP_RET_TRAP),
    };
    struct sock_fprog prog = {
        .len = (unsigned short)(sizeof(f) / sizeof(f[0])),
        .filter = f,
    };
    if (prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0) != 0) return -1;
    if (syscall(SYS_seccomp, SECCOMP_SET_MODE_FILTER, 0, &prog) != 0)
        return -1;
    g_filter_installed = 1;
    return 0;
}

/* -- signal handlers ------------------------------------------------- */
static void ex_sigsys(int sig, siginfo_t *si, void *ucv) {
    (void)sig;
    ucontext_t *uc = (ucontext_t *)ucv;
    if (!g_ex.active) {
        /* Not ours (no guest running): restore the previous disposition
         * and re-raise so the host's own policy applies. */
        sigaction(SIGSYS, &g_old_sigsys, NULL);
        raise(SIGSYS);
        return;
    }
    greg_t *g = uc->uc_mcontext.gregs;
    uint64_t nr = (uint64_t)si->si_syscall;
    /* SIGSYS siginfo has no si_args: the arguments are the trapped regs. */
    uint64_t regs[6] = {
        (uint64_t)g[REG_RDI], (uint64_t)g[REG_RSI], (uint64_t)g[REG_RDX],
        (uint64_t)g[REG_R10], (uint64_t)g[REG_R8],  (uint64_t)g[REG_R9],
    };

    if (nr == VSL_SYS_EXIT || nr == VSL_SYS_EXIT_GROUP) {
        /* The REAL handler records ZOMBIE + exit_code on the VSL_PROC;
         * then unwind to the engine caller instead of dying. */
        (void)vsl_syscall_dispatch(nr, regs);
        g_ex.trapped++;
        g_ex.reason = 1;
        siglongjmp(g_ex.jb, 1);
    }

    int64_t ret = vsl_syscall_dispatch(nr, regs);
    g_ex.trapped++;
    g[REG_RAX] = (greg_t)ret;
    /* Advance past the 2-byte syscall ONLY if RIP still points at it;
     * this kernel already delivers SIGSYS with RIP advanced. */
    unsigned char *rip = (unsigned char *)g[REG_RIP];
    if (rip[0] == 0x0f && rip[1] == 0x05)
        g[REG_RIP] = (greg_t)((uint64_t)g[REG_RIP] + 2);
}

static void ex_fault(int sig, siginfo_t *si, void *ucv) {
    (void)ucv;
    if (!g_ex.active) {
        struct sigaction *old = (sig == SIGSEGV) ? &g_old_sigsegv : &g_old_sigbus;
        sigaction(sig, old, NULL);
        raise(sig);
        return;
    }
    g_ex.fault_addr = (uint64_t)(uintptr_t)si->si_addr;
    g_ex.reason = 2;
    siglongjmp(g_ex.jb, 2);
}

/* -- host mapping collision check ------------------------------------ */
static int ex_range_free(uint64_t lo, uint64_t hi) {
    FILE *f = fopen("/proc/self/maps", "r");
    if (!f) return 0;
    char line[512];
    int free_ = 1;
    while (fgets(line, sizeof(line), f)) {
        unsigned long a, b;
        if (sscanf(line, "%lx-%lx", &a, &b) == 2) {
            if ((uint64_t)a < hi && (uint64_t)b > lo) { free_ = 0; break; }
        }
    }
    fclose(f);
    return free_;
}

/* -- initial guest stack: argc/argv/envp/auxv ------------------------ */
static uint64_t ex_build_stack(uint8_t *base, size_t size, uint64_t entry) {
    uint64_t top = (uint64_t)(uintptr_t)base + size;
    char *arg0 = (char *)(top - 16);
    memcpy(arg0, "vsl-guest", 10);
    uint64_t *p = (uint64_t *)arg0;
    *--p = 0;     *--p = 0;        /* auxv AT_NULL */
    *--p = 4096;  *--p = 6;        /* auxv AT_PAGESZ */
    *--p = entry; *--p = 9;        /* auxv AT_ENTRY */
    *--p = 0;                      /* envp terminator */
    *--p = 0;                      /* argv terminator */
    *--p = (uint64_t)(uintptr_t)arg0;  /* argv[0] */
    *--p = 1;                      /* argc */
    return (uint64_t)(uintptr_t)p & ~15ULL;
}

/* -- fixed host-provided ET_DYN symbols -------------------------------
 * The VSL loader is NOT a general dynamic linker.  Undefined symbols in an
 * ET_DYN image resolve ONLY from this small, fixed table (plus definitions
 * inside the image itself).  Every other name fails closed. */
static uint64_t g_dyn_shim_calls;
static uint64_t g_dyn_write_calls;
static const char g_dyn_test_data[] = "VSL-DYNSYM-DATA\n";

/* Host write shim: the guest calls this through a JUMP_SLOT GOT entry.  It
 * issues a plain host write() (fd 1 is the engine's capture memfd during a
 * run, so output lands in VslExecResult.output).  Being host code it is
 * outside the seccomp trap window -- no recursive trapping. */
static int64_t vsl_dyn_write_shim(int fd, const void *buf, size_t n) {
    g_dyn_write_calls++;
    return (int64_t)syscall(SYS_write, (int)fd, buf, n);
}

/* Test syscall shim: returns a stable magic and records the call so the gate
 * can prove the GOT entry really points HERE and not at some other code. */
static uint64_t vsl_dyn_test_shim_fn(void) {
    g_dyn_shim_calls++;
    return 0x56534c53484d4949ULL;                 /* "VSLSHMII" */
}

typedef struct { const char *name; uint64_t addr; } VslDynHostSym;
static const VslDynHostSym g_dyn_host_syms[] = {
    { "write",         (uint64_t)(uintptr_t)&vsl_dyn_write_shim },
    { "vsl_test_shim", (uint64_t)(uintptr_t)&vsl_dyn_test_shim_fn },
    { "vsl_test_data", (uint64_t)(uintptr_t)g_dyn_test_data },
};

uint64_t vsl_dyn_host_symbol(const char *name) {
    if (!name) return 0;
    for (size_t i = 0; i < sizeof(g_dyn_host_syms) / sizeof(g_dyn_host_syms[0]); i++)
        if (strcmp(name, g_dyn_host_syms[i].name) == 0)
            return g_dyn_host_syms[i].addr;
    return 0;
}
uint64_t vsl_dyn_shim_call_count(void)   { return g_dyn_shim_calls; }
uint64_t vsl_dyn_write_call_count(void)  { return g_dyn_write_calls; }

/* -- ET_DYN helpers -------------------------------------------------- */

/* Translate a link-time virtual address to a raw file offset by finding the
 * PT_LOAD segment that covers it.  `need` bytes must be present in the file
 * (not just in the zero-filled BSS tail). */
static int ex_vaddr_to_off(const uint8_t *f, size_t fsz,
                           uint16_t phnum, uint16_t phentsize, uint64_t phoff,
                           uint64_t vaddr, uint64_t need, uint64_t *out_off) {
    for (int i = 0; i < phnum; i++) {
        const uint8_t *cur = f + phoff + (uint64_t)i * phentsize;
        uint32_t t;
        uint64_t o, v, filesz;
        memcpy(&t, cur, 4);
        if (t != 1) continue;                       /* PT_LOAD */
        memcpy(&o, cur + 8, 8);
        memcpy(&v, cur + 16, 8);
        memcpy(&filesz, cur + 32, 8);
        if (vaddr >= v && vaddr - v <= filesz &&
            need <= filesz - (vaddr - v) &&
            o + (vaddr - v) + need <= fsz) {
            *out_off = o + (vaddr - v);
            return 0;
        }
    }
    return -1;
}

/* Dynamic tags / relocation types we understand.  The resolver is bounded:
 * symbol tables, string tables and relocation counts are capped, and only
 * the four relocation types below are applied.  Unknown symbol names and
 * unsupported types are rejected (fail closed) -- never guessed. */
#define EX_DT_NULL       0
#define EX_DT_PLTRELSZ   2
#define EX_DT_HASH       4
#define EX_DT_STRTAB     5
#define EX_DT_SYMTAB     6
#define EX_DT_RELA       7
#define EX_DT_RELASZ     8
#define EX_DT_RELAENT    9
#define EX_DT_STRSZ      10
#define EX_DT_SYMENT     11
#define EX_DT_REL        17
#define EX_DT_RELSZ      18
#define EX_DT_RELENT     19
#define EX_DT_PLTREL     20
#define EX_DT_JMPREL     23
#define EX_DT_GNU_HASH   0x6ffffef5ULL

#define EX_R_X86_64_64       1
#define EX_R_X86_64_GLOB_DAT 6
#define EX_R_X86_64_JUMP_SLOT 7
#define EX_R_X86_64_RELATIVE 8

#define EX_SYM_MAX   4096u   /* max dynamic symbols / relocations per table */
#define EX_NAME_MAX  128u    /* max resolved symbol name length */
#define EX_STRTAB_MAX (1u << 20)

/* Determine the dynamic symbol count from DT_HASH (nchain) or DT_GNU_HASH
 * (walk the longest chain).  A hash table is REQUIRED: without one we cannot
 * bound the symbol table and refuse to guess.  Returns 0 ok, -1 malformed. */
static int ex_symbol_count(const uint8_t *f, size_t fsz,
                           uint16_t phnum, uint16_t phentsize, uint64_t phoff,
                           uint64_t hash, uint64_t gnu_hash, uint32_t *out) {
    if (hash) {
        uint64_t off;
        if (ex_vaddr_to_off(f, fsz, phnum, phentsize, phoff, hash, 8, &off) != 0)
            return -1;
        uint32_t nchain;
        memcpy(&nchain, f + off + 4, 4);            /* DT_HASH: nbucket,nchain */
        if (nchain == 0 || nchain > EX_SYM_MAX) return -1;
        *out = nchain;
        return 0;
    }
    if (gnu_hash) {
        uint64_t off;
        if (ex_vaddr_to_off(f, fsz, phnum, phentsize, phoff, gnu_hash, 16, &off) != 0)
            return -1;
        uint32_t nbuckets, symoffset, bloom_size, bloom_shift;
        memcpy(&nbuckets,    f + off,      4);
        memcpy(&symoffset,   f + off + 4,  4);
        memcpy(&bloom_size,  f + off + 8,  4);
        memcpy(&bloom_shift, f + off + 12, 4);
        (void)bloom_shift;
        if (nbuckets == 0 || nbuckets > EX_SYM_MAX ||
            bloom_size == 0 || bloom_size > EX_SYM_MAX ||
            symoffset > EX_SYM_MAX) return -1;
        uint64_t fixed = 16 + (uint64_t)bloom_size * 8 + (uint64_t)nbuckets * 4;
        if (ex_vaddr_to_off(f, fsz, phnum, phentsize, phoff,
                            gnu_hash, fixed, &off) != 0) return -1;
        const uint8_t *buckets = f + off + 16 + (uint64_t)bloom_size * 8;
        uint32_t maxb = 0;
        for (uint32_t i = 0; i < nbuckets; i++) {
            uint32_t b;
            memcpy(&b, buckets + (uint64_t)i * 4, 4);
            if (b > maxb) maxb = b;
        }
        if (maxb < symoffset) { *out = symoffset; return 0; }
        if (maxb >= EX_SYM_MAX) return -1;
        uint64_t chain = gnu_hash + fixed;
        uint32_t idx = maxb - symoffset;
        for (uint32_t iter = 0; iter < EX_SYM_MAX; iter++, idx++) {
            if (idx >= EX_SYM_MAX) return -1;
            uint64_t coff;
            if (ex_vaddr_to_off(f, fsz, phnum, phentsize, phoff,
                                chain + (uint64_t)idx * 4, 4, &coff) != 0) return -1;
            uint32_t v;
            memcpy(&v, f + coff, 4);
            if (v & 1) {                            /* last chain entry */
                uint64_t n = (uint64_t)symoffset + idx + 1;
                if (n > EX_SYM_MAX) return -1;
                *out = (uint32_t)n;
                return 0;
            }
        }
        return -1;
    }
    return -1;
}

/* Resolve one dynamic symbol index to its runtime address.  Defined symbols
 * are bias + st_value; undefined names are looked up ONLY in the fixed host
 * table.  Returns 0 ok, -1 unknown symbol / malformed entry (fail closed). */
static int ex_resolve_symbol(const uint8_t *f, size_t fsz,
                             uint16_t phnum, uint16_t phentsize, uint64_t phoff,
                             uint64_t symtab, uint64_t syment,
                             uint64_t strtab, uint64_t strsz,
                             uint32_t symcount, uint32_t symidx,
                             uint64_t bias, uint64_t *out) {
    if (symidx >= symcount || symtab == 0 || syment < 24) return -1;
    uint64_t so;
    if (ex_vaddr_to_off(f, fsz, phnum, phentsize, phoff,
                        symtab + (uint64_t)symidx * syment, 24, &so) != 0) return -1;
    uint32_t st_name;
    uint8_t st_info;
    uint16_t st_shndx;
    uint64_t st_value;
    memcpy(&st_name,  f + so,      4);
    st_info = f[so + 4];
    memcpy(&st_shndx, f + so + 6,  2);
    memcpy(&st_value, f + so + 8,  8);
    if ((st_info & 0x0f) == 6) return -1;            /* STT_TLS has no plain addr */
    if (st_shndx != 0) {                             /* SHN_UNDEF == 0 */
        if (st_shndx == 0xfff1) { *out = st_value; return 0; } /* SHN_ABS */
        if (st_shndx >= 0xff00) return -1;           /* COMMON/XINDEX/processor */
        *out = bias + st_value;
        return 0;
    }
    if (strtab == 0 || strsz == 0 || strsz > EX_STRTAB_MAX || st_name >= strsz)
        return -1;
    uint64_t stroff;
    if (ex_vaddr_to_off(f, fsz, phnum, phentsize, phoff, strtab, strsz, &stroff) != 0)
        return -1;
    const char *name = (const char *)(f + stroff + st_name);
    size_t room = (size_t)(strsz - st_name);
    if (!memchr(name, 0, room) || strlen(name) > EX_NAME_MAX) return -1;
    uint64_t addr = vsl_dyn_host_symbol(name);
    if (addr == 0) return -1;                         /* unknown -> fail closed */
    *out = addr;
    return 0;
}

/* Apply one relocation.  Returns 0 ok, -1 malformed/unknown symbol,
 * 1 unsupported type (counted as skipped; caller rejects the whole image). */
static int ex_apply_one(const uint8_t *f, size_t fsz, uint64_t bias,
                        uint16_t phnum, uint16_t phentsize, uint64_t phoff,
                        uint64_t symtab, uint64_t syment,
                        uint64_t strtab, uint64_t strsz, uint32_t symcount,
                        uint64_t r_offset, uint64_t r_info, int64_t addend,
                        int has_addend, uint32_t *resolved) {
    uint32_t type   = (uint32_t)(r_info & 0xffffffffu);
    uint32_t symidx = (uint32_t)(r_info >> 32);
    uint64_t dest = bias + r_offset;
    if (dest < EX_WIN_LO || dest + 8 > EX_WIN_HI) return -1;
    uint64_t *where = (uint64_t *)(uintptr_t)dest;
    switch (type) {
    case EX_R_X86_64_RELATIVE:
        *where = has_addend ? bias + (uint64_t)addend : *where + bias;
        return 0;
    case EX_R_X86_64_64:
    case EX_R_X86_64_GLOB_DAT:
    case EX_R_X86_64_JUMP_SLOT: {
        uint64_t S;
        if (symidx == 0) {                           /* STN_UNDEF null symbol */
            S = 0;
        } else if (ex_resolve_symbol(f, fsz, phnum, phentsize, phoff,
                                     symtab, syment, strtab, strsz,
                                     symcount, symidx, bias, &S) != 0) {
            return -1;
        }
        uint64_t val = (type == EX_R_X86_64_64)
                     ? S + (has_addend ? (uint64_t)addend : *where)
                     : S;                            /* GLOB_DAT/JUMP_SLOT: S */
        *where = val;
        if (symidx != 0) (*resolved)++;
        return 0;
    }
    default:
        return 1;                                    /* unsupported -> skipped */
    }
}

/* Apply the dynamic relocations of an ET_DYN image after its PT_LOAD bytes
 * are in place.  Handles the bounded set above; every other type is counted
 * in *skipped and makes the caller reject the image (fail closed).
 * Returns 0 ok, -1 malformed/unknown symbol. */
static int ex_apply_relocations(const uint8_t *f, size_t fsz, uint64_t bias,
                                uint16_t phnum, uint16_t phentsize, uint64_t phoff,
                                uint32_t *out_applied, uint32_t *out_skipped,
                                uint32_t *out_resolved) {
    *out_applied = *out_skipped = *out_resolved = 0;

    /* Locate PT_DYNAMIC and read its file bytes. */
    uint64_t doff = 0, dsz = 0;
    for (int i = 0; i < phnum; i++) {
        const uint8_t *cur = f + phoff + (uint64_t)i * phentsize;
        uint32_t t;
        memcpy(&t, cur, 4);
        if (t != 2) continue;                       /* PT_DYNAMIC */
        memcpy(&doff, cur + 8, 8);
        memcpy(&dsz, cur + 32, 8);
        break;
    }
    if (dsz == 0) return 0;                         /* no dynamic section */
    if (doff > fsz || dsz > fsz - doff || dsz < 16) return -1;

    /* Parse d_tag/d_val pairs until DT_NULL. */
    uint64_t rela = 0, relasz = 0, relaent = 24;
    uint64_t rel  = 0, relsz  = 0, relent = 16;
    uint64_t jmprel = 0, pltrelsz = 0, pltrel = 0;
    uint64_t symtab = 0, strtab = 0, strsz = 0, syment = 24;
    uint64_t hash = 0, gnu_hash = 0;
    const uint8_t *d = f + doff;
    for (uint64_t k = 0; k + 16 <= dsz; k += 16) {
        uint64_t tag, val;
        memcpy(&tag, d + k, 8);
        memcpy(&val, d + k + 8, 8);
        if (tag == EX_DT_NULL) break;
        switch (tag) {
        case EX_DT_RELA:    rela = val;   break;
        case EX_DT_RELASZ:  relasz = val; break;
        case EX_DT_RELAENT: relaent = val;break;
        case EX_DT_REL:     rel = val;    break;
        case EX_DT_RELSZ:   relsz = val;  break;
        case EX_DT_RELENT:  relent = val; break;
        case EX_DT_JMPREL:  jmprel = val; break;
        case EX_DT_PLTRELSZ:pltrelsz = val;break;
        case EX_DT_PLTREL:  pltrel = val; break;
        case EX_DT_SYMTAB:  symtab = val; break;
        case EX_DT_STRTAB:  strtab = val; break;
        case EX_DT_STRSZ:   strsz = val;  break;
        case EX_DT_SYMENT:  syment = val; break;
        case EX_DT_HASH:    hash = val;   break;
        case EX_DT_GNU_HASH:gnu_hash = val;break;
        default: break;                              /* DT_NEEDED etc: ignored */
        }
    }

    /* Pre-scan: only images that ACTUALLY contain symbol-based relocations
     * need a bounded symbol table.  A pure RELATIVE image (the original
     * minimal ET_DYN fixture) has none and must keep working. */
    uint32_t symcount = 0;
    int need_syms = 0;
#define EX_SCAN_TABLE(TAB, SIZE, ENT, HAS_ADDEND) do {                        \
        if ((TAB) && (SIZE)) {                                                \
            uint64_t es = (ENT);                                              \
            if (es < (HAS_ADDEND ? 24u : 16u)) es = (HAS_ADDEND ? 24u : 16u); \
            uint64_t cnt = (SIZE) / es;                                       \
            if ((SIZE) % es || cnt > EX_SYM_MAX) return -1;                   \
            uint64_t off;                                                     \
            if (ex_vaddr_to_off(f, fsz, phnum, phentsize, phoff,             \
                                (TAB), (SIZE), &off) != 0) return -1;         \
            for (uint64_t k = 0; k < cnt; k++) {                              \
                uint64_t ri;                                                  \
                memcpy(&ri, f + off + k * es + 8, 8);                         \
                uint32_t ty = (uint32_t)(ri & 0xffffffffu);                   \
                if (ty == EX_R_X86_64_64 || ty == EX_R_X86_64_GLOB_DAT ||     \
                    ty == EX_R_X86_64_JUMP_SLOT) { need_syms = 1; break; }    \
            }                                                                 \
        }                                                                     \
    } while (0)
    EX_SCAN_TABLE(rela, relasz, relaent, 1);
    EX_SCAN_TABLE(rel,  relsz,  relent, 0);
    if (pltrelsz && !jmprel) return -1;
    if (jmprel && pltrelsz) {
        if (pltrel == EX_DT_RELA) EX_SCAN_TABLE(jmprel, pltrelsz, relaent, 1);
        else if (pltrel == EX_DT_REL) EX_SCAN_TABLE(jmprel, pltrelsz, relent, 0);
        else return -1;
    }
#undef EX_SCAN_TABLE
    if (need_syms) {
        if (ex_symbol_count(f, fsz, phnum, phentsize, phoff,
                            hash, gnu_hash, &symcount) != 0) return -1;
        if (symtab == 0 || strtab == 0) return -1;
    }

    /* Walk one relocation table (RELA has explicit addend, REL does not). */
#define EX_WALK_TABLE(TAB, SIZE, ENT, HAS_ADDEND) do {                       \
        if ((TAB) && (SIZE)) {                                               \
            uint64_t es = (ENT);                                             \
            if (es < (HAS_ADDEND ? 24u : 16u)) es = (HAS_ADDEND ? 24u : 16u);\
            uint64_t cnt = (SIZE) / es;                                      \
            if ((SIZE) % es || cnt > EX_SYM_MAX) return -1;                  \
            uint64_t off;                                                    \
            if (ex_vaddr_to_off(f, fsz, phnum, phentsize, phoff,             \
                                (TAB), (SIZE), &off) != 0) return -1;        \
            for (uint64_t k = 0; k < cnt; k++) {                             \
                const uint8_t *rr = f + off + k * es;                        \
                uint64_t ro, ri, ra = 0;                                     \
                memcpy(&ro, rr, 8);                                          \
                memcpy(&ri, rr + 8, 8);                                      \
                if (HAS_ADDEND) memcpy(&ra, rr + 16, 8);                      \
                int vr = ex_apply_one(f, fsz, bias, phnum, phentsize, phoff,  \
                                     symtab, syment, strtab, strsz, symcount, \
                                     ro, ri, (int64_t)ra, HAS_ADDEND,         \
                                     out_resolved);                          \
                if (vr < 0) return -1;                                       \
                if (vr > 0) (*out_skipped)++; else (*out_applied)++;          \
            }                                                                \
        }                                                                    \
    } while (0)

    EX_WALK_TABLE(rela, relasz, relaent, 1);
    EX_WALK_TABLE(rel,  relsz,  relent, 0);

    /* PLT/GOT JUMP_SLOTs live in a separate DT_JMPREL table (DT_PLTREL says
     * RELA or REL).  gcc -shared emits these for every imported function;
     * skipping them would leave the GOT pointing into the PLT resolver. */
    if (pltrelsz && !jmprel) return -1;
    if (jmprel && pltrelsz) {
        if (pltrel == EX_DT_RELA)
            EX_WALK_TABLE(jmprel, pltrelsz, relaent, 1);
        else if (pltrel == EX_DT_REL)
            EX_WALK_TABLE(jmprel, pltrelsz, relent, 0);
        else return -1;
    }
#undef EX_WALK_TABLE
    return 0;
}

/* -- the engine ------------------------------------------------------- */
int vsl_exec_elf_bytes(const void *elf_data, size_t elf_size,
                       VslExecResult *out) {
    if (!out) return VSL_EXEC_ERR_ARGS;
    memset(out, 0, sizeof(*out));
    if (!elf_data || elf_size < 64) {
        out->status = VSL_EXEC_ERR_ARGS;
        return out->status;
    }
    if (g_ex.running) { out->status = VSL_EXEC_ERR_STATE; return out->status; }
    if (!g_vsl.active && vsl_init() != 0) {
        out->status = VSL_EXEC_ERR_STATE;
        return out->status;
    }

    /* 1. Validate through the single owner; ET_EXEC or ET_DYN accepted. */
    uint64_t entry_link = 0;
    if (vsl_elf_validate(elf_data, elf_size, &entry_link) != 0) {
        out->status = VSL_EXEC_ERR_FORMAT;
        return out->status;
    }
    const uint8_t *p = (const uint8_t *)elf_data;
    uint16_t e_type = (uint16_t)p[16] | ((uint16_t)p[17] << 8);
    int is_dyn = (e_type == 3);
    if (e_type != 2 && e_type != 3) {
        out->status = VSL_EXEC_ERR_FORMAT;
        return out->status;
    }

    /* 2. Parse ELF header + program header table. */
    uint16_t e_phnum = (uint16_t)p[56] | ((uint16_t)p[57] << 8);
    uint16_t e_phentsize = (uint16_t)p[54] | ((uint16_t)p[55] << 8);
    uint64_t e_phoff;
    memcpy(&e_phoff, p + 32, 8);
    if (e_phoff + (uint64_t)e_phnum * e_phentsize > elf_size) {
        out->status = VSL_EXEC_ERR_FORMAT;
        return out->status;
    }

    /* Pass A: link-time geometry (needed to choose an ET_DYN load bias). */
    uint64_t link_lo = UINT64_MAX, link_hi = 0;
    for (int i = 0; i < e_phnum; i++) {
        const uint8_t *cur = p + e_phoff + (uint64_t)i * e_phentsize;
        uint32_t p_type;
        uint64_t p_offset, p_vaddr, p_filesz, p_memsz;
        memcpy(&p_type, cur, 4);
        if (p_type != 1) continue;                   /* PT_LOAD */
        memcpy(&p_offset, cur + 8, 8);
        memcpy(&p_vaddr, cur + 16, 8);
        memcpy(&p_filesz, cur + 32, 8);
        memcpy(&p_memsz, cur + 40, 8);
        if (p_memsz == 0) continue;
        if (p_offset + p_filesz > elf_size) {
            out->status = VSL_EXEC_ERR_FORMAT;
            return out->status;
        }
        uint64_t ps = p_vaddr & ~0xFFFULL;
        uint64_t pe = (p_vaddr + p_memsz + 0xFFF) & ~0xFFFULL;
        if (ps < link_lo) link_lo = ps;
        if (pe > link_hi) link_hi = pe;
    }

    /* ET_DYN: pick a page-aligned base inside the trap window such that the
     * whole image fits and nothing is mapped there.  bias = base - link_lo.
     * ET_EXEC keeps bias 0 (map at the original absolute link addresses). */
    uint64_t load_bias = 0;
    if (is_dyn) {
        if (link_lo == UINT64_MAX || link_hi <= link_lo) {
            out->status = VSL_EXEC_ERR_FORMAT;
            return out->status;
        }
        uint64_t span = link_hi - link_lo;
        if (span > (uint64_t)VSL_USER_SIZE) {
            out->status = VSL_EXEC_ERR_WINDOW;
            return out->status;
        }
        int found = 0;
        for (uint64_t k = 0; k < 64; k++) {
            uint64_t base = EX_WIN_LO + k * 0x00100000ULL;
            if (base + span > EX_WIN_HI) break;
            if (ex_range_free(base, base + span)) {
                load_bias = base - link_lo;
                found = 1;
                break;
            }
        }
        if (!found) {
            out->status = VSL_EXEC_ERR_COLLISION;
            return out->status;
        }
    }
    out->load_bias = load_bias;                      /* 0 for ET_EXEC */
    uint64_t entry = entry_link + load_bias;

    /* Pass B: biased bounding range + executable extents. */
    uint64_t lo = UINT64_MAX, hi = 0;
    uint64_t xlo = UINT64_MAX, xhi = 0;
    int n_load = 0;
    for (int i = 0; i < e_phnum; i++) {
        const uint8_t *cur = p + e_phoff + (uint64_t)i * e_phentsize;
        uint32_t p_type, p_flags;
        uint64_t p_offset, p_vaddr, p_filesz, p_memsz;
        memcpy(&p_type, cur, 4);
        if (p_type != 1) continue;
        memcpy(&p_flags, cur + 4, 4);
        memcpy(&p_offset, cur + 8, 8);
        memcpy(&p_vaddr, cur + 16, 8);
        memcpy(&p_filesz, cur + 32, 8);
        memcpy(&p_memsz, cur + 40, 8);
        if (p_memsz == 0) continue;
        uint64_t dest = p_vaddr + load_bias;
        if (dest < EX_WIN_LO || dest + p_memsz > EX_WIN_HI) {
            out->status = VSL_EXEC_ERR_WINDOW;
            return out->status;
        }
        uint64_t ps = dest & ~0xFFFULL;
        uint64_t pe = (dest + p_memsz + 0xFFF) & ~0xFFFULL;
        if (ps < lo) lo = ps;
        if (pe > hi) hi = pe;
        if (p_flags & 1) { /* PF_X */
            if (dest < xlo) xlo = dest;
            if (dest + p_memsz > xhi) xhi = dest + p_memsz;
        }
        n_load++;
    }
    if (n_load == 0 || entry < xlo || entry >= xhi) {
        out->status = VSL_EXEC_ERR_ENTRY;
        return out->status;
    }
    if (!ex_range_free(lo, hi)) {
        out->status = VSL_EXEC_ERR_COLLISION;
        return out->status;
    }

    int rc = VSL_EXEC_OK;
    uint8_t *map = NULL, *stack = NULL;
    int memfd = -1, save1 = -1, save2 = -1;
    uint32_t pid = 0, old_pid = g_vsl.current_pid;
    char old_kernel[32] = {0};
    int kernel_changed = 0;
    int handlers_installed = 0;
    uint64_t sp = 0;

    /* 3. Map the union at link addresses (one mapping covers overlapping
     *    segment pages), copy file bytes; anonymous zero pages are BSS. */
    map = mmap((void *)(uintptr_t)lo, (size_t)(hi - lo),
               PROT_READ | PROT_WRITE | PROT_EXEC,
               MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
    if (map == MAP_FAILED || (uint64_t)(uintptr_t)map != lo) {
        rc = VSL_EXEC_ERR_MAP;
        goto cleanup;
    }
    for (int i = 0; i < e_phnum; i++) {
        const uint8_t *cur = p + e_phoff + (uint64_t)i * e_phentsize;
        uint32_t p_type;
        uint64_t p_offset, p_vaddr, p_filesz;
        memcpy(&p_type, cur, 4);
        if (p_type != 1) continue;
        memcpy(&p_offset, cur + 8, 8);
        memcpy(&p_vaddr, cur + 16, 8);
        memcpy(&p_filesz, cur + 32, 8);
        if (p_filesz)
            memcpy((void *)(uintptr_t)(p_vaddr + load_bias), p + p_offset,
                   (size_t)p_filesz);
    }

    /* 3b. ET_DYN: apply RELATIVE / GLOB_DAT / JUMP_SLOT / 64 relocations.
     *     Undefined symbols resolve only from a bounded fixed host table;
     *     unknown names and unsupported types are a hard reject. */
    if (is_dyn) {
        uint32_t applied = 0, skipped = 0, resolved = 0;
        if (ex_apply_relocations(elf_data, elf_size, load_bias, e_phnum,
                                 e_phentsize, e_phoff,
                                 &applied, &skipped, &resolved) != 0) {
            rc = VSL_EXEC_ERR_RELOC;
            goto cleanup;
        }
        out->relocations_applied = applied;
        out->relocations_skipped = skipped;
        out->symbols_resolved = resolved;
        if (skipped) { rc = VSL_EXEC_ERR_RELOCTYPE; goto cleanup; }
    }

    /* 4. Process record through the single owner; run as that pid. */
    int new_pid = vsl_create_process(elf_data, elf_size);
    if (new_pid < 0) { rc = VSL_EXEC_ERR_STATE; goto cleanup; }
    pid = (uint32_t)new_pid;
    out->pid = pid;
    VSL_PROC *proc = vsl_get_process(pid);
    proc->state = VSL_PROC_RUNNING;
    g_vsl.current_pid = pid;

    /* 5. Linux personality owns the bare syscall range for this run. */
    {
        const char *act = wubu_kernel_active();
        if (act) snprintf(old_kernel, sizeof(old_kernel), "%s", act);
        if (!act || strcmp(act, "linux") != 0) {
            if (wubu_kernel_select("linux") == 0) kernel_changed = 1;
        }
    }

    /* 6. Trap filter (once per host process) + signal handlers. */
    if (ex_install_filter() != 0) { rc = VSL_EXEC_ERR_FILTER; goto cleanup; }
    {
        struct sigaction sa;
        memset(&sa, 0, sizeof(sa));
        sa.sa_sigaction = ex_sigsys;
        sa.sa_flags = SA_SIGINFO;
        sigemptyset(&sa.sa_mask);
        if (sigaction(SIGSYS, &sa, &g_old_sigsys) != 0) {
            rc = VSL_EXEC_ERR_SIGNAL;
            goto cleanup;
        }
        sa.sa_sigaction = ex_fault;
        if (sigaction(SIGSEGV, &sa, &g_old_sigsegv) != 0 ||
            sigaction(SIGBUS, &sa, &g_old_sigbus) != 0) {
            sigaction(SIGSYS, &g_old_sigsys, NULL);
            rc = VSL_EXEC_ERR_SIGNAL;
            goto cleanup;
        }
        handlers_installed = 1;
    }

    /* 7. Capture fds 1/2 through a memfd: the REAL vsl_sys_write does
     *    write() on the host fd, so this records exactly what it wrote. */
    memfd = (int)syscall(SYS_memfd_create, "vsl_exec_out", 0);
    if (memfd < 0) { rc = VSL_EXEC_ERR_SIGNAL; goto cleanup; }
    save1 = dup(1);
    save2 = dup(2);
    if (save1 < 0 || save2 < 0 ||
        dup2(memfd, 1) < 0 || dup2(memfd, 2) < 0) {
        rc = VSL_EXEC_ERR_SIGNAL;
        goto cleanup;
    }

    /* 8. Guest stack + initial ABI image. */
    stack = mmap(NULL, EX_STACK_SIZE, PROT_READ | PROT_WRITE,
                 MAP_PRIVATE | MAP_ANONYMOUS | MAP_STACK, -1, 0);
    if (stack == MAP_FAILED) { stack = NULL; rc = VSL_EXEC_ERR_MAP; goto cleanup; }
    sp = ex_build_stack(stack, EX_STACK_SIZE, entry);
    proc->stack_pointer = sp;

    /* 9. Transfer control.  exit/exit_group and faults siglongjmp back. */
    g_ex.running = 1;
    g_ex.reason = 0;
    g_ex.trapped = 0;
    g_ex.fault_addr = 0;
    if (sigsetjmp(g_ex.jb, 1) == 0) {
        ucontext_t uc;
        if (getcontext(&uc) != 0) {
            g_ex.reason = 2;
            g_ex.fault_addr = 0;
            siglongjmp(g_ex.jb, 2);
        }
        uc.uc_stack.ss_sp = stack;
        uc.uc_stack.ss_size = EX_STACK_SIZE;
        uc.uc_link = NULL;
        uc.uc_mcontext.gregs[REG_RSP] = (greg_t)sp;
        uc.uc_mcontext.gregs[REG_RIP] = (greg_t)entry;
        g_ex.active = 1;
        setcontext(&uc);   /* does not return; guest runs until exit/fault */
        g_ex.active = 0;
        g_ex.reason = 2;   /* unreachable unless setcontext itself failed */
        siglongjmp(g_ex.jb, 2);
    }
    g_ex.active = 0;
    g_ex.running = 0;

    out->syscalls_trapped = g_ex.trapped;
    if (g_ex.reason == 1) {
        out->status = VSL_EXEC_OK;
        out->exit_code = proc->exit_code;  /* set by the REAL vsl_sys_exit */
    } else {
        out->status = VSL_EXEC_ERR_FAULT;
        out->fault_addr = g_ex.fault_addr;
    }
    rc = out->status;

cleanup:
    g_ex.active = 0;
    g_ex.running = 0;
    /* Restore stdio first so diagnostics below go to the real console. */
    if (save1 >= 0) { dup2(save1, 1); close(save1); }
    if (save2 >= 0) { dup2(save2, 2); close(save2); }
    if (memfd >= 0) {
        off_t end = lseek(memfd, 0, SEEK_END);
        if (end > 0) {
            size_t want = (size_t)end;
            if (want > VSL_EXEC_OUTPUT_MAX) want = VSL_EXEC_OUTPUT_MAX;
            ssize_t got = pread(memfd, out->output, want, 0);
            out->output_len = got > 0 ? (size_t)got : 0;
        }
        close(memfd);
    }
    if (handlers_installed) {
        sigaction(SIGSYS, &g_old_sigsys, NULL);
        sigaction(SIGSEGV, &g_old_sigsegv, NULL);
        sigaction(SIGBUS, &g_old_sigbus, NULL);
    }
    if (pid) {
        vsl_destroy_process(pid);
        g_vsl.current_pid = old_pid;
    }
    if (kernel_changed && old_kernel[0])
        wubu_kernel_select(old_kernel);
    if (map && (uint64_t)(uintptr_t)map == lo)
        munmap(map, (size_t)(hi - lo));
    if (stack) munmap(stack, EX_STACK_SIZE);
    if (out->status == VSL_EXEC_OK && rc != VSL_EXEC_OK)
        out->status = rc;
    if (rc != VSL_EXEC_OK && out->status == VSL_EXEC_OK)
        out->status = rc;
    return rc;
}

int vsl_exec_elf(const char *path, VslExecResult *out) {
    if (!out) return VSL_EXEC_ERR_ARGS;
    memset(out, 0, sizeof(*out));
    if (!path) { out->status = VSL_EXEC_ERR_ARGS; return out->status; }
    FILE *f = fopen(path, "rb");
    if (!f) { out->status = VSL_EXEC_ERR_ARGS; return out->status; }
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); out->status = VSL_EXEC_ERR_ARGS; return out->status; }
    long sz = ftell(f);
    if (sz <= 0 || sz > 64 * 1024 * 1024) {
        fclose(f);
        out->status = VSL_EXEC_ERR_ARGS;
        return out->status;
    }
    rewind(f);
    uint8_t *buf = malloc((size_t)sz);
    if (!buf) { fclose(f); out->status = VSL_EXEC_ERR_STATE; return out->status; }
    size_t got = fread(buf, 1, (size_t)sz, f);
    fclose(f);
    int rc = VSL_EXEC_ERR_ARGS;
    if (got == (size_t)sz)
        rc = vsl_exec_elf_bytes(buf, (size_t)sz, out);
    else
        out->status = VSL_EXEC_ERR_ARGS;
    free(buf);
    return rc;
}
