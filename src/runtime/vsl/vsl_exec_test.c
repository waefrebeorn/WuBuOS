/*
 * vsl_exec_test.c  --  L11 gate: VSL EXECUTES, not just loads.
 *
 * Every gate runs a real assembled static freestanding ELF through
 * vsl_exec_elf() and asserts REAL values: exit codes recorded by the real
 * vsl_sys_exit handler, output written by the real vsl_sys_write handler,
 * getpid returning the VSL pid (real handler semantics, not host passthrough),
 * uname matching the host kernel.  Plus a hand-assembled ELF byte image,
 * exit_group, fault containment, and format/window rejection.
 *
 * ET_DYN leg: RELATIVE-only fixtures plus imported-symbol fixtures whose
 * GLOB_DAT/JUMP_SLOT/R_X86_64_64 entries resolve against the bounded host
 * symbol table; exact resolved addresses, output and call counters are
 * asserted, with differential negative controls for the pre-change engine.
 *
 * COPY/IRELATIVE leg: a hand-built ET_DYN whose R_X86_64_COPY copies real
 * host bytes into guest memory and whose R_X86_64_IRELATIVE resolves through
 * the bounded host IFUNC table to a real function; copied bytes, resolved
 * function output and call counters are asserted, with the pre-change rule
 * as negative control.  The 42 unique IRELATIVE resolver link addresses of
 * the real glibc libc.so.6 fixture are also registered in the bounded host
 * table and each is proven to resolve to a documented stand-in value.
 *
 * TLS leg: a hand-built ET_DYN applies bounded TPOFF64/DTPMOD64/DTPOFF32
 * relocations against a defined STT_TLS symbol and dereferences a HOST-
 * provided TLS block.  Two runs with per-image seeds assert distinct TLS
 * values; a symbol-less (STN_UNDEF) TPOFF64 is bounded by the defined module
 * TLS base (S = 0, word = tls_base + addend) and its exact dereferenced
 * address is asserted; the pre-change RELATIVE-only rule rejects the same
 * fixture, and non-TLS symbols / out-of-range offsets / other TLS models /
 * out-of-range symbol-less addends fail closed.
 *
 * DT_NEEDED leg: a real gcc -shared main object whose DT_NEEDED names a
 * freestanding ET_DYN dependency in the allow-listed guest directory.  The
 * gate asserts the dependency's own output precedes the main's (load order),
 * the imported function ran through the merged symbol table, and that a
 * missing dependency / disabled allow-list fails closed with ERR_RELOC.
 * The pre-change loader ignored DT_NEEDED and rejected the unresolved import.
 *
 * Usage: vsl_exec_test <guest-dir>   (guests built by mk/tests.mk)
 */
#include "../../egojar.h"
#include "vsl/vsl_exec_engine.h"
#include "vsl/vsl_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/utsname.h>
#include <unistd.h>

extern VSL_STATE g_vsl;

static int passed, failed;
#define CHECK(cond, msg) do { \
    if (cond) { passed++; printf("  ok: %s\n", (msg)); } \
    else { failed++; printf("  FAIL(line %d): %s\n", __LINE__, (msg)); } \
} while (0)

/* Hand-assemble a minimal static ELF64: one RWX PT_LOAD at `vaddr`,
 * code at file offset 0x1000, entry = vaddr.  Proves the bytes path. */
static size_t build_tiny_elf(uint8_t *buf, uint64_t vaddr, uint16_t e_type,
                             const uint8_t *code, size_t n) {
    memset(buf, 0, 0x2000);
    buf[0] = 0x7f; buf[1] = 'E'; buf[2] = 'L'; buf[3] = 'F';
    buf[4] = 2; buf[5] = 1; buf[6] = 1;
    uint16_t u16; uint32_t u32; uint64_t u64;
    u16 = e_type;        memcpy(buf + 16, &u16, 2);
    u16 = 0x3e;          memcpy(buf + 18, &u16, 2);   /* EM_X86_64 */
    u32 = 1;             memcpy(buf + 20, &u32, 4);   /* EV_CURRENT */
    u64 = vaddr;         memcpy(buf + 24, &u64, 8);   /* e_entry */
    u64 = 64;            memcpy(buf + 32, &u64, 8);   /* e_phoff */
    u16 = 64;            memcpy(buf + 52, &u16, 2);   /* e_ehsize */
    u16 = 56;            memcpy(buf + 54, &u16, 2);   /* e_phentsize */
    u16 = 1;             memcpy(buf + 56, &u16, 2);   /* e_phnum */
    /* program header @64 */
    u32 = 1;             memcpy(buf + 64, &u32, 4);   /* PT_LOAD */
    u32 = 7;             memcpy(buf + 68, &u32, 4);   /* PF_R|W|X */
    u64 = 0x1000;        memcpy(buf + 72, &u64, 8);   /* p_offset */
    u64 = vaddr;         memcpy(buf + 80, &u64, 8);   /* p_vaddr */
    u64 = vaddr;         memcpy(buf + 88, &u64, 8);   /* p_paddr */
    u64 = n;             memcpy(buf + 96, &u64, 8);   /* p_filesz */
    u64 = n;             memcpy(buf + 104, &u64, 8);  /* p_memsz */
    u64 = 0x1000;        memcpy(buf + 112, &u64, 8);  /* p_align */
    memcpy(buf + 0x1000, code, n);
    return 0x1000 + n;
}

/* Hand-assemble a minimal ET_DYN with ONE known R_X86_64_RELATIVE:
 *   PT_LOAD @0x1000 (code + data), PT_DYNAMIC @0x1200, .rela @0x1300.
 * The relocation writes (load_bias + 0x1110) into the 8-byte slot @0x1100.
 * Code writes those 8 raw bytes to fd 1, then writes "VSL-DYN-OK\n" through
 * the relocated pointer, then exit(0).  Perfectly known computed address. */
#define EX_DYN_ADDEND 0x1110ULL
static size_t build_dyn_elf(uint8_t *buf) {
    memset(buf, 0, 0x1400);
    buf[0] = 0x7f; buf[1] = 'E'; buf[2] = 'L'; buf[3] = 'F';
    buf[4] = 2; buf[5] = 1; buf[6] = 1;
    uint16_t u16; uint32_t u32; uint64_t u64;
    u16 = 3;    memcpy(buf + 16, &u16, 2);       /* e_type ET_DYN */
    u16 = 0x3e; memcpy(buf + 18, &u16, 2);       /* EM_X86_64 */
    u32 = 1;    memcpy(buf + 20, &u32, 4);       /* EV_CURRENT */
    u64 = 0x1000; memcpy(buf + 24, &u64, 8);     /* e_entry (link address) */
    u64 = 64;   memcpy(buf + 32, &u64, 8);       /* e_phoff */
    u16 = 64;   memcpy(buf + 52, &u16, 2);       /* e_ehsize */
    u16 = 56;   memcpy(buf + 54, &u16, 2);       /* e_phentsize */
    u16 = 2;    memcpy(buf + 56, &u16, 2);       /* e_phnum */
    /* phdr[0] PT_LOAD @64 */
    u32 = 1;    memcpy(buf + 64, &u32, 4);
    u32 = 7;    memcpy(buf + 68, &u32, 4);       /* PF_R|W|X */
    u64 = 0x1000; memcpy(buf + 72, &u64, 8);     /* p_offset */
    u64 = 0x1000; memcpy(buf + 80, &u64, 8);     /* p_vaddr */
    u64 = 0x1000; memcpy(buf + 88, &u64, 8);     /* p_paddr */
    u64 = 0x400;  memcpy(buf + 96, &u64, 8);     /* p_filesz */
    u64 = 0x400;  memcpy(buf + 104, &u64, 8);    /* p_memsz */
    u64 = 0x1000; memcpy(buf + 112, &u64, 8);    /* p_align */
    /* phdr[1] PT_DYNAMIC @120 */
    u32 = 2;    memcpy(buf + 120, &u32, 4);
    u32 = 6;    memcpy(buf + 124, &u32, 4);      /* PF_R|W */
    u64 = 0x1200; memcpy(buf + 128, &u64, 8);    /* p_offset */
    u64 = 0x1200; memcpy(buf + 136, &u64, 8);    /* p_vaddr */
    u64 = 0x1200; memcpy(buf + 144, &u64, 8);    /* p_paddr */
    u64 = 0x40;   memcpy(buf + 152, &u64, 8);    /* p_filesz */
    u64 = 0x40;   memcpy(buf + 160, &u64, 8);    /* p_memsz */
    u64 = 8;      memcpy(buf + 168, &u64, 8);    /* p_align */
    /* code @0x1000 (RIP-relative displacements are base-independent) */
    {
        static const uint8_t code[] = {
            0x48,0x8d,0x35,0xf9,0x00,0x00,0x00,   /* lea 0x1100(%rip),%rsi */
            0xb8,0x01,0x00,0x00,0x00,             /* mov $1,%eax */
            0xbf,0x01,0x00,0x00,0x00,             /* mov $1,%edi */
            0xba,0x08,0x00,0x00,0x00,             /* mov $8,%edx */
            0x0f,0x05,                           /* syscall: write ptr bytes */
            0x48,0x8b,0x35,0xe1,0x00,0x00,0x00,   /* mov 0x1100(%rip),%rsi */
            0xb8,0x01,0x00,0x00,0x00,             /* mov $1,%eax */
            0xbf,0x01,0x00,0x00,0x00,             /* mov $1,%edi */
            0xba,0x0b,0x00,0x00,0x00,             /* mov $11,%edx */
            0x0f,0x05,                           /* syscall: write string */
            0xb8,0x3c,0x00,0x00,0x00,             /* mov $60,%eax */
            0x31,0xff,                           /* xor %edi,%edi */
            0x0f,0x05                            /* syscall: exit(0) */
        };
        memcpy(buf + 0x1000, code, sizeof(code));
    }
    memcpy(buf + 0x1110, "VSL-DYN-OK\n", 11);
    /* PT_DYNAMIC @0x1200 */
    u64 = 7;    memcpy(buf + 0x1200, &u64, 8); u64 = 0x1300; memcpy(buf + 0x1208, &u64, 8);
    u64 = 8;    memcpy(buf + 0x1210, &u64, 8); u64 = 24;     memcpy(buf + 0x1218, &u64, 8);
    u64 = 9;    memcpy(buf + 0x1220, &u64, 8); u64 = 24;     memcpy(buf + 0x1228, &u64, 8);
    u64 = 0;    memcpy(buf + 0x1230, &u64, 8); u64 = 0;      memcpy(buf + 0x1238, &u64, 8);
    /* Elf64_Rela @0x1300: r_offset=0x1100, r_info=8 (RELATIVE), addend=0x1110 */
    u64 = 0x1100; memcpy(buf + 0x1300, &u64, 8);
    u64 = 8;      memcpy(buf + 0x1308, &u64, 8);
    u64 = EX_DYN_ADDEND; memcpy(buf + 0x1310, &u64, 8);
    return 0x1400;
}

/* Emit `write(1, &ch, 1)` followed by either `ret` (init/fini) or
 * `exit(code)` (main entry).  All bytes are inside the mapped PT_LOAD. */
static void ex_emit_write_byte(uint8_t *dst, uint8_t ch, int exit_code) {
    static const uint8_t pre[] = {
        0xb8,0,0,0,0,             /* mov $ch,%eax */
        0x50,                     /* push %rax */
        0x48,0x89,0xe6,           /* mov %rsp,%rsi */
        0xb8,1,0,0,0,             /* mov $1,%eax */
        0xbf,1,0,0,0,             /* mov $1,%edi */
        0xba,1,0,0,0,             /* mov $1,%edx */
        0x0f,0x05                 /* syscall */
    };
    memcpy(dst, pre, sizeof(pre));
    dst[1] = ch;
    dst += sizeof(pre);
    if (exit_code >= 0) {
        static const uint8_t ex[] = {
            0xb8,0x3c,0,0,0,      /* mov $60,%eax */
            0xbf,0,0,0,0,         /* mov $code,%edi */
            0x0f,0x05             /* syscall */
        };
        memcpy(dst, ex, sizeof(ex));
        dst[6] = (uint8_t)exit_code;   /* low byte of the $code immediate */
    } else {
        static const uint8_t rr[] = { 0x48,0x83,0xc4,0x08, 0xc3 };
        memcpy(dst, rr, sizeof(rr));
    }
}

/* ET_DYN main image with PT_INTERP=/libvslinterp.so and DT_INIT/DT_FINI.
 * Main entry writes 'M' then exit(77) -- it must NEVER be reached when the
 * engine honours PT_INTERP (the interpreter exits 0 instead).  DT_INIT writes
 * 'I' before transfer, DT_FINI writes 'F' on unload, through the existing
 * init/fini owner. */
static size_t build_interp_main_elf(uint8_t *buf) {
    memset(buf, 0, 0x800);
    buf[0] = 0x7f; buf[1] = 'E'; buf[2] = 'L'; buf[3] = 'F';
    buf[4] = 2; buf[5] = 1; buf[6] = 1;
    uint16_t u16; uint32_t u32; uint64_t u64;
    u16 = 3;    memcpy(buf + 16, &u16, 2);        /* ET_DYN */
    u16 = 0x3e; memcpy(buf + 18, &u16, 2);        /* x86-64 */
    u32 = 1;    memcpy(buf + 20, &u32, 4);
    u64 = 0x400; memcpy(buf + 24, &u64, 8);       /* e_entry */
    u64 = 64;   memcpy(buf + 32, &u64, 8);        /* e_phoff */
    u16 = 64;   memcpy(buf + 52, &u16, 2);
    u16 = 56;   memcpy(buf + 54, &u16, 2);
    u16 = 3;    memcpy(buf + 56, &u16, 2);        /* 3 phdrs */
    /* phdr[0] PT_LOAD @0x40: whole first page, RWX */
    u32 = 1;    memcpy(buf + 64, &u32, 4);
    u32 = 7;    memcpy(buf + 68, &u32, 4);
    u64 = 0;    memcpy(buf + 72, &u64, 8);        /* offset */
    u64 = 0;    memcpy(buf + 80, &u64, 8);        /* vaddr */
    u64 = 0;    memcpy(buf + 88, &u64, 8);        /* paddr */
    u64 = 0x800;memcpy(buf + 96, &u64, 8);        /* filesz */
    u64 = 0x800;memcpy(buf + 104, &u64, 8);       /* memsz */
    u64 = 0x1000;memcpy(buf + 112, &u64, 8);
    /* phdr[1] PT_INTERP @0x78 */
    u32 = 3;    memcpy(buf + 120, &u32, 4);
    u32 = 4;    memcpy(buf + 124, &u32, 4);
    u64 = 0x600;memcpy(buf + 128, &u64, 8);
    u64 = 0x600;memcpy(buf + 136, &u64, 8);
    u64 = 0x600;memcpy(buf + 144, &u64, 8);
    u64 = 17;   memcpy(buf + 152, &u64, 8);       /* "/libvslinterp.so\0" */
    u64 = 17;   memcpy(buf + 160, &u64, 8);
    u64 = 1;    memcpy(buf + 168, &u64, 8);
    /* phdr[2] PT_DYNAMIC @0xb0 */
    u32 = 2;    memcpy(buf + 176, &u32, 4);
    u32 = 6;    memcpy(buf + 180, &u32, 4);
    u64 = 0x700;memcpy(buf + 184, &u64, 8);
    u64 = 0x700;memcpy(buf + 192, &u64, 8);
    u64 = 0x700;memcpy(buf + 200, &u64, 8);
    u64 = 48;   memcpy(buf + 208, &u64, 8);
    u64 = 48;   memcpy(buf + 216, &u64, 8);
    u64 = 8;    memcpy(buf + 224, &u64, 8);
    ex_emit_write_byte(buf + 0x400, 'M', 77);     /* main entry (never runs) */
    ex_emit_write_byte(buf + 0x500, 'I', -1);     /* DT_INIT */
    ex_emit_write_byte(buf + 0x540, 'F', -1);     /* DT_FINI */
    memcpy(buf + 0x600, "/libvslinterp.so", 17);
    u64 = 12;   memcpy(buf + 0x700, &u64, 8);     /* DT_INIT */
    u64 = 0x500;memcpy(buf + 0x708, &u64, 8);
    u64 = 13;   memcpy(buf + 0x710, &u64, 8);     /* DT_FINI */
    u64 = 0x540;memcpy(buf + 0x718, &u64, 8);
    u64 = 0;    memcpy(buf + 0x720, &u64, 8);     /* DT_NULL */
    return 0x800;
}

/* Hand-build a freestanding ET_DYN "interpreter" with a versioned self
 * definition (vsl_selfsym@VSL_SELF, defined at vaddr 0x600) and a versioned
 * self-import (same name/version, UNDEF) carrying a GLOB_DAT relocation.  The
 * need names a different file (libfake.so), so the import is satisfiable ONLY
 * when the interpreter's own exported definition is preferred during its own
 * relocation.  Its entry dereferences the relocated GOT slot and writes the
 * 8-byte self-symbol magic to fd 1, then exits(0). */
#define SELF_MAGIC 0x56534c53454c4631ULL
/* String-table-RELATIVE offsets (DT_STRTAB is at vaddr/file 0x300). */
#define SELF_STR_REL       0x01u   /* file 0x301: "vsl_selfsym" */
#define SELF_VER_REL       0x0du   /* file 0x30d: "VSL_SELF" (verdef) */
#define SELF_NEED_VER_REL  0x16u   /* file 0x316: "VSL_SELF" (verneed) */
#define SELF_FILE_REL      0x1fu   /* file 0x31f: "libfake.so" */
#define SELF_NEED_VER_FILE 0x316u  /* file offset mutated by the control */
#define SELF_STRSZ   0x2au
static size_t build_selfsym_interp_elf(uint8_t *buf) {
    memset(buf, 0, 0x900);
    buf[0] = 0x7f; buf[1] = 'E'; buf[2] = 'L'; buf[3] = 'F';
    buf[4] = 2; buf[5] = 1; buf[6] = 1;
    uint16_t u16; uint32_t u32; uint64_t u64;
    u16 = 3;    memcpy(buf + 16, &u16, 2);        /* ET_DYN */
    u16 = 0x3e; memcpy(buf + 18, &u16, 2);
    u32 = 1;    memcpy(buf + 20, &u32, 4);
    u64 = 0x100; memcpy(buf + 24, &u64, 8);       /* e_entry */
    u64 = 64;   memcpy(buf + 32, &u64, 8);        /* e_phoff */
    u16 = 64;   memcpy(buf + 52, &u16, 2);
    u16 = 56;   memcpy(buf + 54, &u16, 2);
    u16 = 2;    memcpy(buf + 56, &u16, 2);        /* 2 phdrs */
    /* phdr[0] PT_LOAD: vaddr == file offset, whole 0x900 RWX */
    u32 = 1;    memcpy(buf + 64,  &u32, 4);
    u32 = 7;    memcpy(buf + 68,  &u32, 4);
    u64 = 0;    memcpy(buf + 72,  &u64, 8);       /* p_offset */
    u64 = 0;    memcpy(buf + 80,  &u64, 8);       /* p_vaddr */
    u64 = 0;    memcpy(buf + 88,  &u64, 8);
    u64 = 0x900;memcpy(buf + 96,  &u64, 8);       /* filesz */
    u64 = 0x900;memcpy(buf + 104, &u64, 8);       /* memsz */
    u64 = 0x1000;memcpy(buf + 112, &u64, 8);
    /* phdr[1] PT_DYNAMIC @0x800, 14 pairs (224 bytes) */
    u32 = 2;    memcpy(buf + 120, &u32, 4);
    u32 = 6;    memcpy(buf + 124, &u32, 4);
    u64 = 0x800;memcpy(buf + 128, &u64, 8);
    u64 = 0x800;memcpy(buf + 136, &u64, 8);
    u64 = 0x800;memcpy(buf + 144, &u64, 8);
    u64 = 224;  memcpy(buf + 152, &u64, 8);
    u64 = 224;  memcpy(buf + 160, &u64, 8);
    u64 = 8;    memcpy(buf + 168, &u64, 8);
    /* Entry @0x100: deref relocated GOT slot @0x500 -> magic @0x600, write
     * 8 bytes to fd 1, exit(0).  RIP-relative LEA is base-independent. */
    static const uint8_t code[] = {
        0x48,0x8d,0x35,0xf9,0x03,0x00,0x00,       /* lea 0x500(%rip),%rsi */
        0x48,0x8b,0x06,                           /* mov (%rsi),%rax */
        0x48,0x8b,0x00,                           /* mov (%rax),%rax */
        0x50,                                     /* push %rax */
        0xb8,0x01,0x00,0x00,0x00,                 /* mov $1,%eax */
        0xbf,0x01,0x00,0x00,0x00,                 /* mov $1,%edi */
        0x48,0x89,0xe6,                           /* mov %rsp,%rsi */
        0xba,0x08,0x00,0x00,0x00,                 /* mov $8,%edx */
        0x0f,0x05,                                /* syscall */
        0x58,                                     /* pop %rax */
        0xb8,0x3c,0x00,0x00,0x00,                 /* mov $60,%eax */
        0x31,0xff,                               /* xor %edi,%edi */
        0x0f,0x05                                /* syscall (exit 0) */
    };
    memcpy(buf + 0x100, code, sizeof(code));
    /* Strings */
    memcpy(buf + 0x300 + SELF_STR_REL,      "vsl_selfsym", 11); /* + NUL */
    memcpy(buf + 0x300 + SELF_VER_REL,      "VSL_SELF", 8);
    memcpy(buf + 0x300 + SELF_NEED_VER_REL, "VSL_SELF", 8);
    memcpy(buf + 0x300 + SELF_FILE_REL,     "libfake.so", 10);
    /* The versioned self-symbol's data. */
    u64 = SELF_MAGIC; memcpy(buf + 0x600, &u64, 8);
    /* dynsym @0x400: null, defined@0x418, undef@0x430 */
    {
        uint8_t *d = buf + 0x418;
        u32 = SELF_STR_REL; memcpy(d, &u32, 4);
        d[4] = 0x11; d[5] = 0;                    /* GLOBAL | OBJECT */
        u16 = 1; memcpy(d + 6, &u16, 2);          /* defined */
        u64 = 0x600; memcpy(d + 8, &u64, 8);      /* st_value */
        u64 = 8;     memcpy(d + 16, &u64, 8);     /* st_size */
        d = buf + 0x430;
        u32 = SELF_STR_REL; memcpy(d, &u32, 4);
        d[4] = 0x11; d[5] = 0;
        u16 = 0; memcpy(d + 6, &u16, 2);          /* SHN_UNDEF */
        u64 = 0; memcpy(d + 8, &u64, 8);
        u64 = 8; memcpy(d + 16, &u64, 8);
    }
    /* versym @0x460: {local, def=2, need=3} */
    u16 = 0; memcpy(buf + 0x460, &u16, 2);
    u16 = 2; memcpy(buf + 0x462, &u16, 2);
    u16 = 3; memcpy(buf + 0x464, &u16, 2);
    /* verdef @0x480 -> verdaux @0x494 */
    u16 = 1; memcpy(buf + 0x480, &u16, 2);        /* vd_version */
    u16 = 1; memcpy(buf + 0x482, &u16, 2);        /* vd_flags */
    u16 = 2; memcpy(buf + 0x484, &u16, 2);        /* vd_ndx */
    u16 = 1; memcpy(buf + 0x486, &u16, 2);        /* vd_cnt */
    u32 = 20;memcpy(buf + 0x48c, &u32, 4);        /* vd_aux */
    u32 = 0; memcpy(buf + 0x490, &u32, 4);        /* vd_next */
    u32 = SELF_VER_REL; memcpy(buf + 0x494, &u32, 4);
    u32 = 0; memcpy(buf + 0x498, &u32, 4);
    /* verneed @0x4a0 -> vernaux @0x4b0 */
    u16 = 1; memcpy(buf + 0x4a0, &u16, 2);        /* vn_version */
    u16 = 1; memcpy(buf + 0x4a2, &u16, 2);        /* vn_cnt */
    u32 = SELF_FILE_REL; memcpy(buf + 0x4a4, &u32, 4); /* vn_file */
    u32 = 16;memcpy(buf + 0x4a8, &u32, 4);        /* vn_aux */
    u32 = 0; memcpy(buf + 0x4ac, &u32, 4);        /* vn_next */
    u16 = 3; memcpy(buf + 0x4b6, &u16, 2);        /* vna_other = 3 */
    u32 = SELF_NEED_VER_REL; memcpy(buf + 0x4b8, &u32, 4);
    u32 = 0; memcpy(buf + 0x4bc, &u32, 4);
    /* DT_HASH @0x4c0: nbucket=1, nchain=3 */
    u32 = 1; memcpy(buf + 0x4c0, &u32, 4);
    u32 = 3; memcpy(buf + 0x4c4, &u32, 4);
    u32 = 1; memcpy(buf + 0x4c8, &u32, 4);        /* bucket[0] */
    u32 = 2; memcpy(buf + 0x4d4, &u32, 4);        /* chain[1] */
    /* R_X86_64_GLOB_DAT @0x700: symidx=2, type=6 */
    u64 = 0x500;               memcpy(buf + 0x700, &u64, 8);
    u64 = (2ULL << 32) | 6ULL; memcpy(buf + 0x708, &u64, 8);
    /* dynamic @0x800 */
    const uint64_t tags[14][2] = {
        { 4, 0x4c0 }, { 5, 0x300 }, { 6, 0x400 },
        { 7, 0x700 }, { 8, 24 }, { 9, 24 },
        { 10, SELF_STRSZ }, { 11, 24 },
        { 0x6ffffff0ULL, 0x460 }, { 0x6ffffffcULL, 0x480 },
        { 0x6ffffffdULL, 1 }, { 0x6ffffffeULL, 0x4a0 },
        { 0x6fffffffULL, 1 }, { 0, 0 }
    };
    for (int i = 0; i < 14; i++) {
        memcpy(buf + 0x800 + (uint64_t)i * 16, &tags[i][0], 8);
        memcpy(buf + 0x808 + (uint64_t)i * 16, &tags[i][1], 8);
    }
    return 0x900;
}

/* Pre-change loader rule: PT_INTERP was ignored and the main e_entry was
 * called directly.  A synthetic image that writes 'M'/exits(77) at its
 * entry proves the differential with the interpreter-driven new path. */
static int legacy_ignores_interp(void) { return 1; }

/* Pre-change loader rule: any PT_INTERP was ignored, so __libc_start_main
 * (absent from the bounded host symbol table) failed closed with ERR_RELOC.
 * Negative control for the real glibc binary: the same bytes, old rule. */
static int legacy_rejects_glibc_dyn(const uint8_t *img, size_t n) {
    if (n < 64) return 0;
    uint64_t phoff; uint16_t phentsize, phnum;
    memcpy(&phoff, img + 32, 8);
    memcpy(&phentsize, img + 54, 2);
    memcpy(&phnum, img + 56, 2);
    if (phoff + (uint64_t)phnum * phentsize > n) return 0;
    for (uint16_t i = 0; i < phnum; i++) {
        const uint8_t *ph = img + phoff + (uint64_t)i * phentsize;
        uint32_t t; memcpy(&t, ph, 4);
        if (t == 3) return 1;                    /* PT_INTERP ignored */
    }
    return 0;
}

/* Pre-change loader rule: any e_type != ET_EXEC(2) was rejected as FORMAT.
 * Used as the differential negative control against the exact fixture. */
static int legacy_rejects_et_dyn(const uint8_t *img, size_t n) {
    if (n < 64) return 0;
    uint16_t t = (uint16_t)img[16] | ((uint16_t)img[17] << 8);
    return t != 2;
}

/* Faithful model of the PRE-CHANGE relocation rule (aadf13c): only
 * R_X86_64_RELATIVE (8) was applied; ANY other type went into `skipped`,
 * which made the engine reject the image with VSL_EXEC_ERR_RELOCTYPE.
 * Scans PT_DYNAMIC -> DT_RELA/DT_REL and DT_JMPREL in an ET_DYN image.
 * Used as the differential negative control for the imported-symbol gates. */
static int legacy_rejects_nonrelative(const uint8_t *img, size_t n) {
    if (n < 64) return 0;
    uint16_t phoff16, phnum, phentsize;
    memcpy(&phoff16, img + 32, 2); (void)phoff16;
    uint64_t phoff; memcpy(&phoff, img + 32, 8);
    memcpy(&phentsize, img + 54, 2);
    memcpy(&phnum, img + 56, 2);
    if (phoff + (uint64_t)phnum * phentsize > n) return 0;
    /* Map one link-time vaddr to a file offset via the first covering PT_LOAD. */
    uint64_t lo_v = 0, lo_o = 0, lo_sz = 0;
    int have_load = 0;
    for (uint16_t i = 0; i < phnum; i++) {
        const uint8_t *ph = img + phoff + (uint64_t)i * phentsize;
        uint32_t t; memcpy(&t, ph, 4);
        if (t != 1) continue;
        uint64_t o, v, fsz;
        memcpy(&o, ph + 8, 8); memcpy(&v, ph + 16, 8); memcpy(&fsz, ph + 32, 8);
        if (v <= n && o <= n && fsz <= n - o) { lo_v = v; lo_o = o; lo_sz = fsz; have_load = 1; break; }
    }
    if (!have_load) return 0;
    /* Locate PT_DYNAMIC. */
    uint64_t dsz = 0, doff = 0;
    for (uint16_t i = 0; i < phnum; i++) {
        const uint8_t *ph = img + phoff + (uint64_t)i * phentsize;
        uint32_t t; memcpy(&t, ph, 4);
        if (t != 2) continue;
        memcpy(&doff, ph + 8, 8); memcpy(&dsz, ph + 32, 8);
        break;
    }
    if (dsz < 16 || doff > n || dsz > n - doff) return 0;
    uint64_t rela = 0, relasz = 0, relaent = 24;
    uint64_t jmprel = 0, pltrelsz = 0, pltrel = 0;
    for (uint64_t k = 0; k + 16 <= dsz; k += 16) {
        uint64_t tag, val;
        memcpy(&tag, img + doff + k, 8); memcpy(&val, img + doff + k + 8, 8);
        if (tag == 0) break;
        if (tag == 7) rela = val;
        else if (tag == 8) relasz = val;
        else if (tag == 9) relaent = val;
        else if (tag == 23) jmprel = val;
        else if (tag == 2) pltrelsz = val;
        else if (tag == 20) pltrel = val;
    }
    if (relaent < 24) relaent = 24;
    int saw_nonrelative = 0;
    /* Walk DT_RELA (RELA format) and DT_JMPREL (usually RELA too). */
    uint64_t tables[2][2] = { { rela, relasz }, { jmprel, pltrelsz } };
    for (int ti = 0; ti < 2; ti++) {
        uint64_t tab = tables[ti][0], sz = tables[ti][1];
        if (ti == 1 && pltrel && pltrel != 7) continue; /* old code ignored PLTREL too */
        if (!tab || !sz) continue;
        if (tab < lo_v || tab - lo_v + sz > lo_sz) continue;
        uint64_t off = lo_o + (tab - lo_v);
        if (off > n || sz > n - off) continue;
        for (uint64_t k = 0; k + 24 <= sz; k += relaent) {
            uint64_t ri; memcpy(&ri, img + off + k + 8, 8);
            if ((uint32_t)(ri & 0xffffffffu) != 8) saw_nonrelative = 1;
        }
    }
    return saw_nonrelative;
}

/* Read a built fixture into memory so a gate can mutate a copy in place
 * (used by the DT_NEEDED missing-soname negative control). */
static uint8_t *read_test_file(const char *path, size_t *out_sz) {
    FILE *fp = fopen(path, "rb");
    if (!fp) return NULL;
    if (fseek(fp, 0, SEEK_END) != 0) { fclose(fp); return NULL; }
    long sz = ftell(fp);
    if (sz <= 0 || sz > 16 * 1024 * 1024) { fclose(fp); return NULL; }
    rewind(fp);
    uint8_t *buf = malloc((size_t)sz);
    if (!buf) { fclose(fp); return NULL; }
    size_t got = fread(buf, 1, (size_t)sz, fp);
    fclose(fp);
    if (got != (size_t)sz) { free(buf); return NULL; }
    *out_sz = got;
    return buf;
}

/* Locate the R_X86_64_COPY source dynsym in a REAL host-linked fixture by
 * walking ELF64 section headers (no libelf).  Returns 1 when a dynsym with the
 * given name and SHN_UNDEF is found; fills the file offset of its st_size and
 * its DT_VERSYM index.  Used by the bounded-COPY negatives: grow st_size past
 * the provider's size, and record that the pre-change host-only rule rejected
 * every versioned COPY (it refused vi > GLOBAL unconditionally). */
static int copy_dynsym_info(const uint8_t *img, size_t n, const char *name,
                            uint64_t *size_off, uint16_t *versym) {
    if (n < 64 || img[0] != 0x7f || memcmp(img + 1, "ELF", 3) != 0) return 0;
    uint64_t shoff; uint16_t shentsize, shnum;
    memcpy(&shoff, img + 0x28, 8);
    memcpy(&shentsize, img + 0x3a, 2);
    memcpy(&shnum, img + 0x3c, 2);
    if (shentsize < 64 || shnum == 0 || shoff > n ||
        (uint64_t)shnum * shentsize > n - shoff) return 0;
    uint64_t dyn_off = 0, dyn_sz = 0, dyn_ent = 0, vso = 0, vssz = 0;
    uint32_t str_idx = 0;
    for (uint16_t i = 0; i < shnum; i++) {
        const uint8_t *sh = img + shoff + (uint64_t)i * shentsize;
        uint32_t type; uint64_t off, sz, ent; uint32_t link;
        memcpy(&type, sh + 4, 4);
        memcpy(&off, sh + 24, 8);
        memcpy(&sz, sh + 32, 8);
        memcpy(&link, sh + 40, 4);
        memcpy(&ent, sh + 56, 8);
        if (off > n || sz > n - off) continue;
        if (type == 11) {                /* SHT_DYNSYM */
            dyn_off = off; dyn_sz = sz; dyn_ent = ent ? ent : 24; str_idx = link;
        } else if (type == 0x6fffffffu) { /* SHT_GNU_versym */
            vso = off; vssz = sz;
        }
    }
    if (!dyn_off || !dyn_sz || dyn_ent < 24 || str_idx >= shnum) return 0;
    const uint8_t *shs = img + shoff + (uint64_t)str_idx * shentsize;
    uint64_t stroff, strsz;
    memcpy(&stroff, shs + 24, 8);
    memcpy(&strsz, shs + 32, 8);
    if (stroff > n || strsz > n - stroff) return 0;
    for (uint64_t i = 0; i < dyn_sz / dyn_ent; i++) {
        const uint8_t *e = img + dyn_off + i * dyn_ent;
        uint32_t st_name; uint16_t st_shndx;
        memcpy(&st_name, e, 4);
        memcpy(&st_shndx, e + 6, 2);
        /* A real COPY symbol is defined in .data/.bss; a hand-built one may
         * be SHN_UNDEF.  Reject only reserved section indices. */
        if (st_shndx >= 0xff00 || st_name >= strsz) continue;
        const char *nm = (const char *)(img + stroff + st_name);
        if (!memchr(nm, 0, (size_t)(strsz - st_name)) || strcmp(nm, name) != 0)
            continue;
        if (size_off) *size_off = dyn_off + i * dyn_ent + 16;
        uint16_t v = 1;                          /* VSYM_GLOBAL default */
        if (vso && (i + 1) * 2 <= vssz)
            memcpy(&v, img + vso + i * 2, 2);
        if (versym) *versym = v;
        return 1;
    }
    return 0;
}

/* Faithful model of the PRE-CHANGE relocation rule: PT_DYNAMIC's DT_NEEDED
 * tags were ignored (`default: break`) and no dependency was loaded, so an
 * import such as vsl_dep_entry (absent from the fixed host table) failed
 * closed with VSL_EXEC_ERR_RELOC.  Returns 1 when this image would have been
 * rejected for that reason. */
static int legacy_rejects_dtneeded(const uint8_t *img, size_t n) {
    if (n < 64) return 0;
    uint64_t phoff; uint16_t phentsize, phnum;
    memcpy(&phoff, img + 32, 8);
    memcpy(&phentsize, img + 54, 2);
    memcpy(&phnum, img + 56, 2);
    if (phoff + (uint64_t)phnum * phentsize > n) return 0;
    uint64_t doff = 0, dsz = 0;
    for (uint16_t i = 0; i < phnum; i++) {
        const uint8_t *ph = img + phoff + (uint64_t)i * phentsize;
        uint32_t t; memcpy(&t, ph, 4);
        if (t != 2) continue;
        memcpy(&doff, ph + 8, 8);
        memcpy(&dsz, ph + 32, 8);
        break;
    }
    if (dsz < 16 || doff > n || dsz > n - doff) return 0;
    for (uint64_t k = 0; k + 16 <= dsz; k += 16) {
        uint64_t tag; memcpy(&tag, img + doff + k, 8);
        if (tag == 0) break;
        if (tag == 1) return 1;                      /* DT_NEEDED ignored */
    }
    return 0;
}

/* Model the PRE-CHANGE base-name-only loader on a COPY of a versioned
 * fixture: clear DT_VERSYM's value so no version data is consulted.  All
 * code, DT_NEEDED graph and relocations stay byte-identical; the resulting
 * output is what the old loader would have produced from the same guest. */
static int legacy_strip_versions(uint8_t *img, size_t n) {
    if (n < 64) return 0;
    uint64_t phoff; uint16_t phentsize, phnum;
    memcpy(&phoff, img + 32, 8);
    memcpy(&phentsize, img + 54, 2);
    memcpy(&phnum, img + 56, 2);
    if (phoff + (uint64_t)phnum * phentsize > n) return 0;
    uint64_t doff = 0, dsz = 0;
    for (uint16_t i = 0; i < phnum; i++) {
        const uint8_t *ph = img + phoff + (uint64_t)i * phentsize;
        uint32_t t; memcpy(&t, ph, 4);
        if (t != 2) continue;
        memcpy(&doff, ph + 8, 8);
        memcpy(&dsz, ph + 32, 8);
        break;
    }
    if (dsz < 16 || doff > n || dsz > n - doff) return 0;
    for (uint64_t k = 0; k + 16 <= dsz; k += 16) {
        uint64_t tag; memcpy(&tag, img + doff + k, 8);
        if (tag == 0) break;
        if (tag == 0x6ffffff0ULL) {                  /* DT_VERSYM */
            uint64_t zero = 0;
            memcpy(img + doff + k + 8, &zero, 8);
            return 1;
        }
    }
    return 0;
}

/* Rename a version requirement in a COPY of the fixture's dynamic string
 * table so the request has no matching definition (missing-version case). */
static int rename_version(uint8_t *img, size_t n, const char *from, char to) {
    size_t len = strlen(from);
    if (len == 0 || n < len + 1) return 0;
    for (size_t i = 0; i + len + 1 <= n; i++) {
        if (memcmp(img + i, from, len) == 0 && img[i + len] == 0) {
            img[i + len - 1] = to;
            return 1;
        }
    }
    return 0;
}

/* Hand-assemble a tiny ET_DYN with IMPORTED symbols (DT_HASH + DT_SYMTAB
 * + DT_STRTAB).  Three relocations exercise the bounded resolver:
 *   [0] R_X86_64_GLOB_DAT   sym=vsl_test_shim  -> slot @0x1100
 *   [1] R_X86_64_JUMP_SLOT  sym=write          -> slot @0x1108
 *   [2] R_X86_64_64         sym=vsl_test_shim, addend 0x10 -> slot @0x1110
 * The guest writes the three resolved 8-byte words, calls slot[0] to get
 * the shim magic, writes it, then calls slot[1] (host write shim) to emit
 * a fixed message, then exit(0).  No libc, no ld.so, no PLT resolver. */
#define DSYM_STR_OFF_SHIM 1u
#define DSYM_STR_OFF_WRITE 15u
#define DSYM_STR_OFF_DATA 21u
#define DSYM_STRSZ 35u
static size_t build_dynsym_elf(uint8_t *buf) {
    memset(buf, 0, 0x1600);
    buf[0] = 0x7f; buf[1] = 'E'; buf[2] = 'L'; buf[3] = 'F';
    buf[4] = 2; buf[5] = 1; buf[6] = 1;
    uint16_t u16; uint32_t u32; uint64_t u64;
    u16 = 3;    memcpy(buf + 16, &u16, 2);        /* ET_DYN */
    u16 = 0x3e; memcpy(buf + 18, &u16, 2);        /* EM_X86_64 */
    u32 = 1;    memcpy(buf + 20, &u32, 4);
    u64 = 0x1000; memcpy(buf + 24, &u64, 8);      /* e_entry */
    u64 = 64;   memcpy(buf + 32, &u64, 8);        /* e_phoff */
    u16 = 64;   memcpy(buf + 52, &u16, 2);        /* e_ehsize */
    u16 = 56;   memcpy(buf + 54, &u16, 2);        /* e_phentsize */
    u16 = 2;    memcpy(buf + 56, &u16, 2);        /* e_phnum */
    /* phdr[0] PT_LOAD: file/vaddr 0x1000, filesz/memsz 0x600, RWX */
    u32 = 1;    memcpy(buf + 64,  &u32, 4);
    u32 = 7;    memcpy(buf + 68,  &u32, 4);
    u64 = 0x1000; memcpy(buf + 72,  &u64, 8);
    u64 = 0x1000; memcpy(buf + 80,  &u64, 8);
    u64 = 0x1000; memcpy(buf + 88,  &u64, 8);
    u64 = 0x600;  memcpy(buf + 96,  &u64, 8);
    u64 = 0x600;  memcpy(buf + 104, &u64, 8);
    u64 = 0x1000; memcpy(buf + 112, &u64, 8);
    /* phdr[1] PT_DYNAMIC @0x1400, 9 pairs (144 bytes) */
    u32 = 2;    memcpy(buf + 120, &u32, 4);
    u32 = 6;    memcpy(buf + 124, &u32, 4);
    u64 = 0x1400; memcpy(buf + 128, &u64, 8);
    u64 = 0x1400; memcpy(buf + 136, &u64, 8);
    u64 = 0x1400; memcpy(buf + 144, &u64, 8);
    u64 = 144;    memcpy(buf + 152, &u64, 8);
    u64 = 144;    memcpy(buf + 160, &u64, 8);
    u64 = 8;      memcpy(buf + 168, &u64, 8);
    /* Code: raw syscalls + indirect calls through the relocated GOT slots.
     * Displacements are link-time constants (base-independent). */
    static const uint8_t code[] = {
        0x48,0x8d,0x35,0xf9,0x00,0x00,0x00, 0xb8,0x01,0x00,0x00,0x00,0xbf,0x01,0x00,0x00,
        0x00,0xba,0x18,0x00,0x00,0x00,0x0f,0x05,0xff,0x15,0xe2,0x00,0x00,0x00,0x48,0x89,
        0x05,0xf3,0x00,0x00,0x00,0x48,0x8d,0x35,0xec,0x00,0x00,0x00,0xb8,0x01,0x00,0x00,
        0x00,0xbf,0x01,0x00,0x00,0x00,0xba,0x08,0x00,0x00,0x00,0x0f,0x05,0x48,0x8d,0x35,
        0xdc,0x00,0x00,0x00,0xbf,0x01,0x00,0x00,0x00,0xba,0x0c,0x00,0x00,0x00,0xff,0x15,
        0xb4,0x00,0x00,0x00,0xb8,0x3c,0x00,0x00,0x00,0x31,0xff,0x0f,0x05
    };
    memcpy(buf + 0x1000, code, sizeof(code));
    memcpy(buf + 0x1120, "VSL-SYMS-OK\n", 12);
    /* DT_HASH @0x1300: nbucket=1, nchain=4, bucket[0]=1, zero chains. */
    u32 = 1; memcpy(buf + 0x1300, &u32, 4);
    u32 = 4; memcpy(buf + 0x1304, &u32, 4);
    u32 = 1; memcpy(buf + 0x1308, &u32, 4);
    /* strtab @0x1280 */
    memcpy(buf + 0x1280, "\0vsl_test_shim\0write\0vsl_test_data\0", DSYM_STRSZ);
    /* symtab @0x1200: null + 3 SHN_UNDEF GLOBAL NOTYPE symbols.
     * Elf64_Sym: st_name(4), st_info(1), st_other(1), st_shndx(2),
     *            st_value(8), st_size(8). */
    const uint32_t names[3] = { DSYM_STR_OFF_SHIM, DSYM_STR_OFF_WRITE, DSYM_STR_OFF_DATA };
    for (int i = 0; i < 3; i++) {
        uint8_t *s = buf + 0x1200 + (uint64_t)(i + 1) * 24;
        u32 = names[i]; memcpy(s, &u32, 4);
        s[4] = 0x10; s[5] = 0;                        /* GLOBAL, DEFAULT */
        u16 = 0; memcpy(s + 6, &u16, 2);              /* SHN_UNDEF */
        u64 = 0; memcpy(s + 8, &u64, 8);
        memcpy(s + 16, &u64, 8);
    }
    /* dynamic @0x1400 */
    const uint64_t tags[9][2] = {
        { 4, 0x1300 }, { 5, 0x1280 }, { 6, 0x1200 },
        { 7, 0x1500 }, { 8, 72 },     { 9, 24 },
        { 10, DSYM_STRSZ }, { 11, 24 }, { 0, 0 }
    };
    for (int i = 0; i < 9; i++) {
        memcpy(buf + 0x1400 + (uint64_t)i * 16, &tags[i][0], 8);
        memcpy(buf + 0x1408 + (uint64_t)i * 16, &tags[i][1], 8);
    }
    /* Elf64_Rela @0x1500: three imported-symbol relocations. */
    u64 = 0x1100; memcpy(buf + 0x1500, &u64, 8);
    u64 = ((uint64_t)1 << 32) | 6; memcpy(buf + 0x1508, &u64, 8); /* GLOB_DAT sym1 */
    u64 = 0;      memcpy(buf + 0x1510, &u64, 8);
    u64 = 0x1108; memcpy(buf + 0x1518, &u64, 8);
    u64 = ((uint64_t)2 << 32) | 7; memcpy(buf + 0x1520, &u64, 8); /* JUMP_SLOT sym2 */
    u64 = 0;      memcpy(buf + 0x1528, &u64, 8);
    u64 = 0x1110; memcpy(buf + 0x1530, &u64, 8);
    u64 = ((uint64_t)1 << 32) | 1; memcpy(buf + 0x1538, &u64, 8); /* R_64 sym1 */
    u64 = 0x10;   memcpy(buf + 0x1540, &u64, 8);
    return 0x1600;
}

/* Hand-assemble an ET_DYN carrying a DEFINED STT_TLS symbol (st_value 0x20)
 * and three TLS relocations against it:
 *   [0] R_X86_64_TPOFF64  -> GOT @0x1100 = tls_base + 0x20  (TLS var address)
 *   [1] R_X86_64_DTPMOD64 -> GOT @0x1108 = module id (1)
 *   [2] R_X86_64_DTPOFF32 -> GOT @0x1110 = 0x20 (in-block offset)
 * The guest dereferences the TPOFF64 address, then writes the 8-byte TLS
 * value, the 8-byte module id and the 4-byte offset to fd 1 (20 bytes),
 * then exit(0).  No libc, no ld.so, no %fs: the host block base is the TP.
 * Mutations used by the gate: sym1 st_info @0x121c, st_value @0x1220,
 * RELA[0].r_info @0x1508. */
#define TLS_SYM_STR_OFF 1u
#define TLS_STRSZ       12u          /* "\0vsl_tls_var\0" */
#define TLS_SYM_VALUE   0x20u
static size_t build_tls_elf(uint8_t *buf) {
    memset(buf, 0, 0x1600);
    buf[0] = 0x7f; buf[1] = 'E'; buf[2] = 'L'; buf[3] = 'F';
    buf[4] = 2; buf[5] = 1; buf[6] = 1;
    uint16_t u16; uint32_t u32; uint64_t u64;
    u16 = 3;    memcpy(buf + 16, &u16, 2);        /* ET_DYN */
    u16 = 0x3e; memcpy(buf + 18, &u16, 2);        /* EM_X86_64 */
    u32 = 1;    memcpy(buf + 20, &u32, 4);
    u64 = 0x1000; memcpy(buf + 24, &u64, 8);      /* e_entry */
    u64 = 64;   memcpy(buf + 32, &u64, 8);        /* e_phoff */
    u16 = 64;   memcpy(buf + 52, &u16, 2);
    u16 = 56;   memcpy(buf + 54, &u16, 2);
    u16 = 2;    memcpy(buf + 56, &u16, 2);        /* e_phnum */
    /* phdr[0] PT_LOAD @0x1000, filesz/memsz 0x600, RWX */
    u32 = 1;    memcpy(buf + 64,  &u32, 4);
    u32 = 7;    memcpy(buf + 68,  &u32, 4);
    u64 = 0x1000; memcpy(buf + 72,  &u64, 8);
    u64 = 0x1000; memcpy(buf + 80,  &u64, 8);
    u64 = 0x1000; memcpy(buf + 88,  &u64, 8);
    u64 = 0x600;  memcpy(buf + 96,  &u64, 8);
    u64 = 0x600;  memcpy(buf + 104, &u64, 8);
    u64 = 0x1000; memcpy(buf + 112, &u64, 8);
    /* phdr[1] PT_DYNAMIC @0x1400, 9 pairs (144 bytes) */
    u32 = 2;    memcpy(buf + 120, &u32, 4);
    u32 = 6;    memcpy(buf + 124, &u32, 4);
    u64 = 0x1400; memcpy(buf + 128, &u64, 8);
    u64 = 0x1400; memcpy(buf + 136, &u64, 8);
    u64 = 0x1400; memcpy(buf + 144, &u64, 8);
    u64 = 144;    memcpy(buf + 152, &u64, 8);
    u64 = 144;    memcpy(buf + 160, &u64, 8);
    u64 = 8;      memcpy(buf + 168, &u64, 8);
    /* Code: load the TPOFF64 slot, deref the host TLS word, write it, then
     * write the DTPMOD64 and DTPOFF32 slots, then exit(0).  All RIP-relative
     * displacements are base-independent. */
    static const uint8_t code[] = {
        0x48,0x8d,0x35,0xf9,0x00,0x00,0x00,       /* lea 0x1100(%rip),%rsi   */
        0x48,0x8b,0x06,                           /* mov (%rsi),%rax         */
        0x48,0x8b,0x00,                           /* mov (%rax),%rax         */
        0x50,                                     /* push %rax               */
        0xb8,0x01,0x00,0x00,0x00,                 /* mov $1,%eax             */
        0xbf,0x01,0x00,0x00,0x00,                 /* mov $1,%edi             */
        0x48,0x89,0xe6,                           /* mov %rsp,%rsi           */
        0xba,0x08,0x00,0x00,0x00,                 /* mov $8,%edx             */
        0x0f,0x05,                                /* syscall                 */
        0x58,                                     /* pop %rax                */
        0x48,0x8d,0x35,0xde,0x00,0x00,0x00,       /* lea 0x1108(%rip),%rsi   */
        0xb8,0x01,0x00,0x00,0x00,                 /* mov $1,%eax             */
        0xbf,0x01,0x00,0x00,0x00,                 /* mov $1,%edi             */
        0xba,0x08,0x00,0x00,0x00,                 /* mov $8,%edx             */
        0x0f,0x05,                                /* syscall                 */
        0x48,0x8d,0x35,0xce,0x00,0x00,0x00,       /* lea 0x1110(%rip),%rsi   */
        0xb8,0x01,0x00,0x00,0x00,                 /* mov $1,%eax             */
        0xbf,0x01,0x00,0x00,0x00,                 /* mov $1,%edi             */
        0xba,0x04,0x00,0x00,0x00,                 /* mov $4,%edx             */
        0x0f,0x05,                                /* syscall                 */
        0xb8,0x3c,0x00,0x00,0x00,                 /* mov $60,%eax            */
        0x31,0xff,                               /* xor %edi,%edi           */
        0x0f,0x05                                /* syscall (exit 0)        */
    };
    memcpy(buf + 0x1000, code, sizeof(code));
    memcpy(buf + 0x1280, "\0vsl_tls_var\0", TLS_STRSZ);
    /* DT_HASH @0x1300: nbucket=1, nchain=2, bucket[0]=1, zeros. */
    u32 = 1; memcpy(buf + 0x1300, &u32, 4);
    u32 = 2; memcpy(buf + 0x1304, &u32, 4);
    u32 = 1; memcpy(buf + 0x1308, &u32, 4);
    /* symtab @0x1200: null + one DEFINED GLOBAL STT_TLS symbol @0x1218. */
    {
        uint8_t *s = buf + 0x1218;
        u32 = TLS_SYM_STR_OFF; memcpy(s, &u32, 4);
        s[4] = 0x16; s[5] = 0;                    /* GLOBAL | STT_TLS */
        u16 = 1;      memcpy(s + 6, &u16, 2);     /* st_shndx = 1 (defined) */
        u64 = TLS_SYM_VALUE; memcpy(s + 8, &u64, 8);  /* st_value = 0x20 */
        u64 = 8;      memcpy(s + 16, &u64, 8);    /* st_size = 8 */
    }
    /* dynamic @0x1400 */
    const uint64_t tags[9][2] = {
        { 4, 0x1300 }, { 5, 0x1280 }, { 6, 0x1200 },
        { 7, 0x1500 }, { 8, 72 },     { 9, 24 },
        { 10, TLS_STRSZ }, { 11, 24 }, { 0, 0 }
    };
    for (int i = 0; i < 9; i++) {
        memcpy(buf + 0x1400 + (uint64_t)i * 16, &tags[i][0], 8);
        memcpy(buf + 0x1408 + (uint64_t)i * 16, &tags[i][1], 8);
    }
    /* Elf64_Rela @0x1500: three TLS relocations, all against sym1. */
    const uint32_t rtypes[3] = { 18, 16, 21 };   /* TPOFF64, DTPMOD64, DTPOFF32 */
    const uint64_t roffs[3]  = { 0x1100, 0x1108, 0x1110 };
    for (int i = 0; i < 3; i++) {
        uint8_t *r = buf + 0x1500 + (uint64_t)i * 24;
        memcpy(r, &roffs[i], 8);
        u64 = ((uint64_t)1 << 32) | rtypes[i];
        memcpy(r + 8, &u64, 8);
        u64 = 0; memcpy(r + 16, &u64, 8);
    }
    return 0x1600;
}

/* Hand-assemble an ET_DYN carrying one R_X86_64_COPY and one
 * R_X86_64_IRELATIVE.  One RWX PT_LOAD 0x1000..0x1600:
 *   0x1000 code       0x1100 COPY destination (16 bytes, zero before load)
 *   0x1120 IFUNC GOT slot (initial word = 0x1180 = resolver link address)
 *   0x1130 "VSL-CI-OK\n"  0x1180 resolver stub byte (never executed)
 *   0x1200 dynsym     0x1280 dynstr     0x1300 DT_HASH
 *   0x1400 PT_DYNAMIC 0x1500 DT_RELA (2 entries)
 * The guest writes the 16 copied bytes, the 8 resolved bytes, the marker,
 * then exit(0).  No libc/ld.so: every observed byte is loader work. */
#define CI_COPY_OFF    0x1100ULL
#define CI_FUNC_OFF    0x1120ULL
#define CI_RESOLVER    0x1180ULL
#define CI_COPY_SZ     16u
#define CI_STRSZ       15u                  /* "\0vsl_copy_data\0" */
#define CI_IFUNC_MAGIC 0x56534c49464e5543ULL /* "VSLIFUNC" */
static size_t build_copy_irelative_elf(uint8_t *buf) {
    memset(buf, 0, 0x1600);
    buf[0] = 0x7f; buf[1] = 'E'; buf[2] = 'L'; buf[3] = 'F';
    buf[4] = 2; buf[5] = 1; buf[6] = 1;
    uint16_t u16; uint32_t u32; uint64_t u64;
    u16 = 3;    memcpy(buf + 16, &u16, 2);        /* ET_DYN */
    u16 = 0x3e; memcpy(buf + 18, &u16, 2);        /* EM_X86_64 */
    u32 = 1;    memcpy(buf + 20, &u32, 4);
    u64 = 0x1000; memcpy(buf + 24, &u64, 8);      /* e_entry */
    u64 = 64;   memcpy(buf + 32, &u64, 8);        /* e_phoff */
    u16 = 64;   memcpy(buf + 52, &u16, 2);
    u16 = 56;   memcpy(buf + 54, &u16, 2);
    u16 = 2;    memcpy(buf + 56, &u16, 2);        /* e_phnum */
    /* phdr[0] PT_LOAD 0x1000..0x1600 RWX */
    u32 = 1;    memcpy(buf + 64,  &u32, 4);
    u32 = 7;    memcpy(buf + 68,  &u32, 4);
    u64 = 0x1000; memcpy(buf + 72,  &u64, 8);
    u64 = 0x1000; memcpy(buf + 80,  &u64, 8);
    u64 = 0x1000; memcpy(buf + 88,  &u64, 8);
    u64 = 0x600;  memcpy(buf + 96,  &u64, 8);
    u64 = 0x600;  memcpy(buf + 104, &u64, 8);
    u64 = 0x1000; memcpy(buf + 112, &u64, 8);
    /* phdr[1] PT_DYNAMIC @0x1400, 9 pairs (144 bytes) */
    u32 = 2;    memcpy(buf + 120, &u32, 4);
    u32 = 6;    memcpy(buf + 124, &u32, 4);
    u64 = 0x1400; memcpy(buf + 128, &u64, 8);
    u64 = 0x1400; memcpy(buf + 136, &u64, 8);
    u64 = 0x1400; memcpy(buf + 144, &u64, 8);
    u64 = 144;    memcpy(buf + 152, &u64, 8);
    u64 = 144;    memcpy(buf + 160, &u64, 8);
    u64 = 8;      memcpy(buf + 168, &u64, 8);
    /* RIP-relative displacements are link-time constants (base-independent). */
    static const uint8_t code[] = {
        /* write(1, 0x1100, 16): bytes copied by COPY */
        0x48,0x8d,0x35,0xf9,0x00,0x00,0x00,
        0xb8,0x01,0x00,0x00,0x00, 0xbf,0x01,0x00,0x00,0x00,
        0xba,0x10,0x00,0x00,0x00, 0x0f,0x05,
        /* write(1, 0x1120, 8): function address produced by IRELATIVE */
        0x48,0x8d,0x35,0x01,0x01,0x00,0x00,
        0xb8,0x01,0x00,0x00,0x00, 0xbf,0x01,0x00,0x00,0x00,
        0xba,0x08,0x00,0x00,0x00, 0x0f,0x05,
        /* write(1, 0x1130, 10): marker */
        0x48,0x8d,0x35,0xf9,0x00,0x00,0x00,
        0xb8,0x01,0x00,0x00,0x00, 0xbf,0x01,0x00,0x00,0x00,
        0xba,0x0a,0x00,0x00,0x00, 0x0f,0x05,
        /* exit(0) */
        0xb8,0x3c,0x00,0x00,0x00, 0x31,0xff, 0x0f,0x05
    };
    memcpy(buf + 0x1000, code, sizeof(code));
    memcpy(buf + 0x1130, "VSL-CI-OK\n", 10);
    buf[0x1180] = 0xc3;                   /* resolver stub; never executed */
    /* dynsym[1] @0x1218: "vsl_copy_data", SHN_UNDEF, st_size = 16 */
    u32 = 1;           memcpy(buf + 0x1218, &u32, 4);
    buf[0x121c] = 0x10; buf[0x121d] = 0;  /* GLOBAL, DEFAULT */
    u16 = 0;           memcpy(buf + 0x121e, &u16, 2);
    u64 = 0;           memcpy(buf + 0x1220, &u64, 8);
    u64 = CI_COPY_SZ;  memcpy(buf + 0x1228, &u64, 8);
    memcpy(buf + 0x1280, "\0vsl_copy_data\0", CI_STRSZ);
    /* DT_HASH @0x1300: nbucket=1, nchain=2, bucket[0]=1 */
    u32 = 1; memcpy(buf + 0x1300, &u32, 4);
    u32 = 2; memcpy(buf + 0x1304, &u32, 4);
    u32 = 1; memcpy(buf + 0x1308, &u32, 4);
    const uint64_t tags[9][2] = {
        { 4, 0x1300 }, { 5, 0x1280 }, { 6, 0x1200 },
        { 7, 0x1500 }, { 8, 48 },     { 9, 24 },
        { 10, CI_STRSZ }, { 11, 24 }, { 0, 0 }
    };
    for (int i = 0; i < 9; i++) {
        memcpy(buf + 0x1400 + (uint64_t)i * 16, &tags[i][0], 8);
        memcpy(buf + 0x1408 + (uint64_t)i * 16, &tags[i][1], 8);
    }
    /* RELA[0] @0x1500: COPY sym1 -> 0x1100.
     * RELA[1] @0x1518: IRELATIVE (sym 0) -> 0x1120, resolver 0x1180. */
    u64 = CI_COPY_OFF; memcpy(buf + 0x1500, &u64, 8);
    u64 = ((uint64_t)1 << 32) | 5; memcpy(buf + 0x1508, &u64, 8);
    u64 = 0;           memcpy(buf + 0x1510, &u64, 8);
    u64 = CI_FUNC_OFF; memcpy(buf + 0x1518, &u64, 8);
    u64 = 37;          memcpy(buf + 0x1520, &u64, 8);
    u64 = CI_RESOLVER; memcpy(buf + 0x1528, &u64, 8);
    return 0x1600;
}

/* Map a link-time vaddr to a file offset via the first covering PT_LOAD. */
static int img_vaddr_to_off(const uint8_t *img, size_t n, uint64_t va,
                            uint64_t len, uint64_t *out) {
    uint64_t phoff; uint16_t phentsize, phnum;
    memcpy(&phoff, img + 32, 8);
    memcpy(&phentsize, img + 54, 2);
    memcpy(&phnum, img + 56, 2);
    if (phoff + (uint64_t)phnum * phentsize > n) return -1;
    for (uint16_t i = 0; i < phnum; i++) {
        const uint8_t *ph = img + phoff + (uint64_t)i * phentsize;
        uint32_t t; memcpy(&t, ph, 4);
        if (t != 1) continue;                       /* PT_LOAD */
        uint64_t o, v, fsz;
        memcpy(&o, ph + 8, 8);  memcpy(&v, ph + 16, 8); memcpy(&fsz, ph + 32, 8);
        if (va >= v && va - v <= fsz && len <= fsz - (va - v) &&
            o + (va - v) <= n) { *out = o + (va - v); return 0; }
    }
    return -1;
}

/* Retarget one DT_VERNEED record's vn_file string on a COPY of a fixture.
 * Both file names must already exist in .dynstr; only the 4-byte vn_file
 * pointer is rewritten, so the DT_NEEDED sonames stay byte-identical.  The
 * result is an ELF that requires version X from libA while only libB
 * defines it -- the exact case vn_file enforcement must refuse. */
static int patch_verneed_file(uint8_t *img, size_t n,
                              const char *from, const char *to) {
    uint64_t phoff; uint16_t phentsize, phnum;
    memcpy(&phoff, img + 32, 8);
    memcpy(&phentsize, img + 54, 2);
    memcpy(&phnum, img + 56, 2);
    if (phoff + (uint64_t)phnum * phentsize > n) return 0;
    uint64_t doff = 0, dsz = 0;
    for (uint16_t i = 0; i < phnum; i++) {
        const uint8_t *ph = img + phoff + (uint64_t)i * phentsize;
        uint32_t t; memcpy(&t, ph, 4);
        if (t != 2) continue;                       /* PT_DYNAMIC */
        memcpy(&doff, ph + 8, 8); memcpy(&dsz, ph + 32, 8);
        break;
    }
    if (dsz < 16 || doff > n || dsz > n - doff) return 0;
    uint64_t vneed = 0, vnum = 0, strtab = 0, strsz = 0;
    for (uint64_t k = 0; k + 16 <= dsz; k += 16) {
        uint64_t tag, val;
        memcpy(&tag, img + doff + k, 8); memcpy(&val, img + doff + k + 8, 8);
        if (tag == 0) break;
        if (tag == 0x6ffffffeULL) vneed = val;       /* DT_VERNEED */
        else if (tag == 0x6fffffffULL) vnum = val;   /* DT_VERNEEDNUM */
        else if (tag == 5) strtab = val;             /* DT_STRTAB */
        else if (tag == 10) strsz = val;             /* DT_STRSZ */
    }
    if (!vneed || !strtab || !strsz || strsz > (1u << 20)) return 0;
    uint64_t stroff;
    if (img_vaddr_to_off(img, n, strtab, strsz, &stroff) != 0) return 0;
    size_t tolen = strlen(to);
    uint64_t to_off = 0;
    int have_to = 0;
    for (uint64_t i = 0; i + tolen + 1 <= strsz; i++) {
        const char *s = (const char *)(img + stroff + i);
        if (strcmp(s, to) == 0) { to_off = i; have_to = 1; break; }
    }
    if (!have_to || to_off > 0xffffffffULL) return 0;
    uint64_t cur = vneed, walked = 0, limit = vnum ? vnum : 32;
    if (limit > 32) return 0;
    while (walked < limit) {
        uint64_t off;
        if (img_vaddr_to_off(img, n, cur, 16, &off) != 0) return 0;
        uint32_t vf, next;
        memcpy(&vf,   img + off + 4,  4);          /* Elf64_Verneed.vn_file */
        memcpy(&next, img + off + 12, 4);
        if (vf < strsz && memchr(img + stroff + vf, 0, (size_t)(strsz - vf))) {
            const char *fname = (const char *)(img + stroff + vf);
            if (strcmp(fname, from) == 0) {
                uint32_t v = (uint32_t)to_off;
                memcpy(img + off + 4, &v, 4);
                return 1;
            }
        }
        walked++;
        if (next == 0) break;
        cur += next;
    }
    return 0;
}


int main(int argc, char **argv) {
    setvbuf(stdout, NULL, _IONBF, 0);
    if (argc < 2) {
        fprintf(stderr, "usage: %s <guest-dir>\n", argv[0]);
        return 2;
    }
    const char *dir = argv[1];
    char path[512];
    VslExecResult r;

    printf("== L11 gate: VSL execution engine ==\n");

    /* (a) assembled static ELF: exit(42) -> engine returns 42 */
    snprintf(path, sizeof(path), "%s/guest_exit42.elf", dir);
    int rc = vsl_exec_elf(path, &r);
    printf("[gate a] %s: rc=%d status=%d exit=%d trapped=%llu pid=%u\n",
           path, rc, r.status, r.exit_code,
           (unsigned long long)r.syscalls_trapped, r.pid);
    CHECK(rc == VSL_EXEC_OK && r.status == VSL_EXEC_OK, "exit42 runs to exit");
    CHECK(r.exit_code == 42, "exit42: REAL exit code 42 via vsl_sys_exit");
    CHECK(r.syscalls_trapped >= 1, "exit42: syscall really trapped+dispatched");
    CHECK(r.pid > 1, "exit42: ran as a real VSL process record");

    /* (b) write(1,msg,len); exit(0) -> captured output equals msg exactly */
    static const char msg[] = "VSL-L11: real write via vsl_sys_write\n";
    snprintf(path, sizeof(path), "%s/guest_write.elf", dir);
    rc = vsl_exec_elf(path, &r);
    printf("[gate b] rc=%d exit=%d out_len=%zu out=\"%.*s\"\n",
           rc, r.exit_code, r.output_len, (int)r.output_len, r.output);
    CHECK(rc == VSL_EXEC_OK && r.exit_code == 0, "write guest exits 0");
    CHECK(r.output_len == sizeof(msg) - 1, "write: captured length exact");
    CHECK(r.output_len == sizeof(msg) - 1 &&
          memcmp(r.output, msg, sizeof(msg) - 1) == 0,
          "write: captured bytes EQUAL msg (real vsl_sys_write)");

    /* (c) getpid: REAL handler returns the VSL pid, not the host pid */
    snprintf(path, sizeof(path), "%s/guest_getpid.elf", dir);
    rc = vsl_exec_elf(path, &r);
    long guest_pid = -1;
    if (r.output_len >= 8) memcpy(&guest_pid, r.output, 8);
    printf("[gate c] rc=%d exit=%d guest getpid=%ld engine pid=%u host pid=%d\n",
           rc, r.exit_code, guest_pid, r.pid, (int)getpid());
    CHECK(rc == VSL_EXEC_OK && r.exit_code == 0, "getpid guest exits 0");
    CHECK(r.output_len == 8, "getpid: guest wrote 8 raw bytes");
    CHECK(guest_pid == (long)r.pid,
          "getpid: REAL vsl semantics (guest saw the VSL pid)");
    CHECK(guest_pid != (long)getpid(),
          "getpid: NOT host passthrough (VSL pid != host pid)");

    /* (c2) uname: REAL handler copies host utsname; sysname must match */
    snprintf(path, sizeof(path), "%s/guest_uname.elf", dir);
    rc = vsl_exec_elf(path, &r);
    struct utsname host_u;
    uname(&host_u);
    printf("[gate c2] rc=%d exit=%d out_len=%zu sysname=\"%.65s\" host=\"%s\"\n",
           rc, r.exit_code, r.output_len, r.output, host_u.sysname);
    CHECK(rc == VSL_EXEC_OK && r.exit_code == 0, "uname guest exits 0");
    CHECK(r.output_len == 65, "uname: full 65-byte sysname field written");
    CHECK(r.output_len == 65 &&
          strncmp(r.output, host_u.sysname, 65) == 0,
          "uname: REAL vsl_sys_uname data matches host uname");

    /* (d) exit_group(77): second exit path unwinds too */
    snprintf(path, sizeof(path), "%s/guest_exit_group.elf", dir);
    rc = vsl_exec_elf(path, &r);
    printf("[gate d] rc=%d exit=%d\n", rc, r.exit_code);
    CHECK(rc == VSL_EXEC_OK && r.exit_code == 77,
          "exit_group(77): REAL exit code 77");

    /* (e) hand-assembled ELF bytes: exit(42) without any toolchain */
    {
        /* mov eax,60; mov edi,42; syscall */
        const uint8_t code[] = { 0xb8, 0x3c, 0, 0, 0,
                                 0xbf, 0x2a, 0, 0, 0, 0x0f, 0x05 };
        uint8_t img[0x2000];
        size_t n = build_tiny_elf(img, 0x40000000ULL, 2, code, sizeof(code));
        rc = vsl_exec_elf_bytes(img, n, &r);
        printf("[gate e] hand-assembled bytes: rc=%d exit=%d\n", rc, r.exit_code);
        CHECK(rc == VSL_EXEC_OK && r.exit_code == 42,
              "hand-assembled ELF bytes execute to exit(42)");
    }

    /* (f) fault containment: guest SIGSEGV -> ERR_FAULT, engine survives */
    snprintf(path, sizeof(path), "%s/guest_fault.elf", dir);
    rc = vsl_exec_elf(path, &r);
    printf("[gate f] fault guest: rc=%d status=%d fault_addr=0x%llx\n",
           rc, r.status, (unsigned long long)r.fault_addr);
    CHECK(rc == VSL_EXEC_ERR_FAULT, "guest SIGSEGV reported as ERR_FAULT");
    /* engine still runnable afterwards */
    snprintf(path, sizeof(path), "%s/guest_exit42.elf", dir);
    rc = vsl_exec_elf(path, &r);
    CHECK(rc == VSL_EXEC_OK && r.exit_code == 42,
          "engine survives fault and runs again (no process death)");

    /* (g) rejection gates: bad format, ET_DYN, out-of-window */
    {
        uint8_t img[0x2000];
        const uint8_t code[] = { 0xb8, 0x3c, 0, 0, 0, 0xbf, 0, 0, 0, 0, 0x0f, 0x05 };
        uint8_t garbage[128];
        memset(garbage, 0xAB, sizeof(garbage));
        rc = vsl_exec_elf_bytes(garbage, sizeof(garbage), &r);
        CHECK(rc == VSL_EXEC_ERR_FORMAT, "garbage bytes rejected (FORMAT)");
        size_t n = build_tiny_elf(img, 0x40000000ULL, 3 /* ET_DYN */, code, sizeof(code));
        /* ET_DYN is now SUPPORTED (gates i/j). The pre-change rule would have
         * rejected this same image: keep that as the negative control. */
        CHECK(legacy_rejects_et_dyn(img, n),
              "ET_DYN image would be rejected by pre-change loader");
        n = build_tiny_elf(img, 0x400000ULL /* below VSL window */, 2, code, sizeof(code));
        rc = vsl_exec_elf_bytes(img, n, &r);
        CHECK(rc == VSL_EXEC_ERR_WINDOW, "out-of-window link address rejected");
        rc = vsl_exec_elf("/nonexistent/vsl-guest.elf", &r);
        CHECK(rc == VSL_EXEC_ERR_ARGS, "missing file rejected cleanly");
    }

    /* (i) ET_DYN differential gate: hand-assembled ET_DYN with one known
     *     R_X86_64_RELATIVE.  Pre-change loader rejects it; new one runs it
     *     and the guest echoes the computed relocated address. */
    {
        uint8_t img[0x1400];
        size_t n = build_dyn_elf(img);
        CHECK(legacy_rejects_et_dyn(img, n),
              "negative control: pre-change loader rejects this ET_DYN fixture");
        rc = vsl_exec_elf_bytes(img, n, &r);
        uint64_t ptr = 0;
        if (r.output_len >= 8) memcpy(&ptr, r.output, 8);
        printf("[gate i] ET_DYN(relative): rc=%d exit=%d bias=0x%llx applied=%u skipped=%u "
               "ptr=0x%llx out_len=%zu tail=\"%.*s\"\n",
               rc, r.exit_code, (unsigned long long)r.load_bias,
               r.relocations_applied, r.relocations_skipped,
               (unsigned long long)ptr, r.output_len,
               (int)(r.output_len > 8 ? r.output_len - 8 : 0),
               r.output_len > 8 ? r.output + 8 : "");
        CHECK(rc == VSL_EXEC_OK && r.exit_code == 0, "ET_DYN: runs to exit(0)");
        CHECK(r.load_bias != 0, "ET_DYN: non-zero load bias selected");
        CHECK(r.relocations_applied == 1 && r.relocations_skipped == 0,
              "ET_DYN: exactly one RELATIVE relocation applied, none skipped");
        CHECK(r.output_len == 19, "ET_DYN: 8 pointer bytes + 11 string bytes");
        CHECK(ptr == r.load_bias + EX_DYN_ADDEND,
              "ET_DYN: relocated pointer == load_bias + addend (computed address)");
        CHECK((ptr & 0xfffULL) == (EX_DYN_ADDEND & 0xfffULL),
              "ET_DYN: chosen base is page-aligned (pointer low bits match addend)");
        CHECK(ptr >= VSL_USER_BASE && ptr < VSL_USER_BASE + VSL_USER_SIZE,
              "ET_DYN: relocated address lies inside VSL trap window");
        CHECK(r.output_len == 19 && memcmp(r.output + 8, "VSL-DYN-OK\n", 11) == 0,
              "ET_DYN: string read through relocated pointer");
    }

    /* (j) A REAL gcc-produced ET_DYN shared object with one R_X86_64_RELATIVE. */
    snprintf(path, sizeof(path), "%s/guest_dyn.elf", dir);
    rc = vsl_exec_elf(path, &r);
    printf("[gate j] real .so: rc=%d exit=%d bias=0x%llx applied=%u skipped=%u out_len=%zu out=\"%.*s\"\n",
           rc, r.exit_code, (unsigned long long)r.load_bias,
           r.relocations_applied, r.relocations_skipped, r.output_len,
           (int)r.output_len, r.output);
    CHECK(rc == VSL_EXEC_OK && r.exit_code == 0, "real ET_DYN .so runs to exit(0)");
    CHECK(r.load_bias != 0 && r.relocations_applied >= 1 && r.relocations_skipped == 0,
          "real .so: relocated at a bias with RELATIVE applied");
    CHECK(r.output_len == 11 && memcmp(r.output, "VSL-DYN-SO\n", 11) == 0,
          "real .so: output read through relocated pointer");

    /* (k) Hand-assembled ET_DYN with IMPORTED symbols: GLOB_DAT, JUMP_SLOT
     *     and R_X86_64_64 resolved from the bounded host table.  Pre-change
     *     engine rejects every non-RELATIVE type with RELOCTYPE. */
    {
        uint8_t img[0x1600];
        size_t n = build_dynsym_elf(img);
        CHECK(legacy_rejects_nonrelative(img, n),
              "negative control: pre-change loader rejects imported-symbol fixture");
        uint64_t shim_addr = vsl_dyn_host_symbol("vsl_test_shim");
        uint64_t write_addr = vsl_dyn_host_symbol("write");
        CHECK(shim_addr != 0 && write_addr != 0,
              "host table provides vsl_test_shim and write");
        uint64_t shim_before = vsl_dyn_shim_call_count();
        uint64_t wr_before = vsl_dyn_write_call_count();
        rc = vsl_exec_elf_bytes(img, n, &r);
        uint64_t got[4] = {0};
        for (int i = 0; i < 4 && r.output_len >= (size_t)(i + 1) * 8; i++)
            memcpy(&got[i], r.output + i * 8, 8);
        printf("[gate k] hand ET_DYN imports: rc=%d exit=%d bias=0x%llx "
               "applied=%u skipped=%u resolved=%u out_len=%zu\n",
               rc, r.exit_code, (unsigned long long)r.load_bias,
               r.relocations_applied, r.relocations_skipped, r.symbols_resolved,
               r.output_len);
        printf("[gate k] GLOB_DAT=0x%llx (want 0x%llx) "
               "JUMP_SLOT=0x%llx (want 0x%llx) R64=0x%llx (want 0x%llx)\n",
               (unsigned long long)got[0], (unsigned long long)shim_addr,
               (unsigned long long)got[1], (unsigned long long)write_addr,
               (unsigned long long)got[2], (unsigned long long)(shim_addr + 0x10));
        CHECK(rc == VSL_EXEC_OK && r.exit_code == 0, "imports fixture runs to exit(0)");
        CHECK(r.relocations_applied == 3 && r.relocations_skipped == 0,
              "imports: 3 relocations applied, none skipped");
        CHECK(r.symbols_resolved == 3, "imports: 3 symbols resolved from host table");
        CHECK(r.output_len == 44, "imports: 24 addr bytes + 8 magic + 12 msg");
        CHECK(got[0] == shim_addr,
              "GLOB_DAT slot == exact vsl_test_shim host address");
        CHECK(got[1] == write_addr,
              "JUMP_SLOT slot == exact write host address");
        CHECK(got[2] == shim_addr + 0x10,
              "R_X86_64_64 slot == shim address + addend");
        CHECK(got[3] == 0x56534c53484d4949ULL,
              "guest called through GLOB_DAT GOT: shim magic returned");
        CHECK(r.output_len == 44 &&
              memcmp(r.output + 32, "VSL-SYMS-OK\n", 12) == 0,
              "guest called through JUMP_SLOT GOT: host write shim emitted msg");
        CHECK(vsl_dyn_shim_call_count() == shim_before + 1,
              "shim call counter proves control reached host shim");
        CHECK(vsl_dyn_write_call_count() == wr_before + 1,
              "write-shim counter proves JUMP_SLOT call used host write shim");
    }

    /* (l) REAL gcc -shared object with undefined write/vsl_test_shim/
     *     vsl_test_data: GNU_HASH dynsym, DT_JMPREL JUMP_SLOTs, GLOB_DATs. */
    {
        uint64_t shim_before = vsl_dyn_shim_call_count();
        uint64_t wr_before = vsl_dyn_write_call_count();
        snprintf(path, sizeof(path), "%s/guest_dynsym.elf", dir);
        rc = vsl_exec_elf(path, &r);
        uint64_t magic = 0, self = 0;
        if (r.output_len >= 24) memcpy(&magic, r.output + 16, 8);
        if (r.output_len >= 32) memcpy(&self, r.output + 24, 8);
        printf("[gate l] real .so imports: rc=%d exit=%d bias=0x%llx "
               "applied=%u skipped=%u resolved=%u out_len=%zu\n",
               rc, r.exit_code, (unsigned long long)r.load_bias,
               r.relocations_applied, r.relocations_skipped,
               r.symbols_resolved, r.output_len);
        CHECK(rc == VSL_EXEC_OK && r.exit_code == 0,
              "real imported .so runs to exit(0)");
        CHECK(r.relocations_applied >= 5 && r.relocations_skipped == 0,
              "real .so: RELATIVE+GLOB_DAT+JUMP_SLOT all applied");
        CHECK(r.symbols_resolved >= 4,
              "real .so: GNU_HASH dynsym resolved 4+ symbols");
        CHECK(r.output_len == 32, "real .so: 16 data + 8 shim magic + 8 self magic");
        CHECK(r.output_len == 32 &&
              memcmp(r.output, "VSL-DYNSYM-DATA\n", 16) == 0,
              "real .so: GLOB_DAT resolved vsl_test_data to host string");
        CHECK(magic == 0x56534c53484d4949ULL,
              "real .so: JUMP_SLOT resolved vsl_test_shim via PLT/GOT");
        CHECK(self == 0x4141414141414141ULL,
              "real .so: defined symbol resolved to bias+st_value");
        CHECK(vsl_dyn_shim_call_count() == shim_before + 1,
              "real .so: shim invoked exactly once via resolved JUMP_SLOT");
        CHECK(vsl_dyn_write_call_count() >= wr_before + 3,
              "real .so: 3 writes went through the resolved write JUMP_SLOT");
    }

    /* (m) fail-closed: unknown symbol and unsupported relocation type. */
    {
        uint8_t img[0x1600];
        size_t n = build_dynsym_elf(img);
        CHECK(vsl_dyn_host_symbol("no_such_sym_xx") == 0,
              "host table refuses an unknown symbol name");
        /* (m2) Differential gate: the bounded ld.so provider binds a
         * versioned import ONLY on an exact (name, DT_VERNEED version,
         * DT_VERNEED vn_file) triple.  Every near-miss fails closed. */
        {
            uint64_t exact = vsl_dyn_host_symbol_ver(
                "_dl_argv", "GLIBC_PRIVATE", "ld-linux-x86-64.so.2");
            uint64_t wrong_file = vsl_dyn_host_symbol_ver(
                "_dl_argv", "GLIBC_PRIVATE", "libc.so.6");
            uint64_t wrong_ver = vsl_dyn_host_symbol_ver(
                "_dl_argv", "GLIBC_2.2.5", "ld-linux-x86-64.so.2");
            uint64_t unversioned = vsl_dyn_host_symbol("_dl_argv");
            printf("[gate m2] versioned ld.so provider: exact=%llx "
                   "wrong_file=%llx wrong_ver=%llx unversioned=%llx\n",
                   (unsigned long long)exact, (unsigned long long)wrong_file,
                   (unsigned long long)wrong_ver,
                   (unsigned long long)unversioned);
            CHECK(exact != 0,
                  "_dl_argv@GLIBC_PRIVATE binds from exact ld.so vn_file");
            CHECK(wrong_file == 0,
                  "wrong vn_file fails closed (exact vn_file matching)");
            CHECK(wrong_ver == 0, "wrong version name fails closed");
            CHECK(unversioned == 0,
                  "unversioned lookup cannot reach the versioned ld.so entry");
            CHECK(vsl_dyn_host_symbol_ver("__libc_stack_end", "GLIBC_2.2.5",
                                          "ld-linux-x86-64.so.2") != 0,
                  "__libc_stack_end@GLIBC_2.2.5 binds from exact ld.so vn_file");
            CHECK(vsl_dyn_host_symbol_ver("_dl_find_object", "GLIBC_2.35",
                                          "ld-linux-x86-64.so.2") != 0,
                  "_dl_find_object@GLIBC_2.35 binds from exact ld.so vn_file");
            /* (m3) All remaining versioned ld.so imports of the REAL glibc
             * libc.so.6 bind ONLY on the exact (version, vn_file) triple.
             * Data imports are zeroed host storage; function imports are
             * bounded return-0/void stand-ins.  Every near-miss (wrong file
             * or wrong version) still fails closed. */
            {
                static const char *const names[] = {
                    "__libc_enable_secure", "_rtld_global", "_rtld_global_ro",
                    "_dl_find_dso_for_object", "_dl_deallocate_tls",
                    "__tls_get_addr", "_dl_signal_error", "_dl_signal_exception",
                    "_dl_audit_symbind_alt", "__tunable_is_initialized",
                    "_dl_rtld_di_serinfo", "_dl_allocate_tls",
                    "__tunable_get_val", "_dl_catch_exception",
                    "_dl_allocate_tls_init", "__nptl_change_stack_perm",
                    "_dl_audit_preinit"
                };
                size_t bound = 0;
                for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); i++) {
                    const char *ver = (strcmp(names[i], "__tls_get_addr") == 0)
                                    ? "GLIBC_2.3" : "GLIBC_PRIVATE";
                    uint64_t a = vsl_dyn_host_symbol_ver(names[i], ver,
                                                         "ld-linux-x86-64.so.2");
                    uint64_t wf = vsl_dyn_host_symbol_ver(names[i], ver,
                                                         "libc.so.6");
                    uint64_t wv = vsl_dyn_host_symbol_ver(
                        names[i], "GLIBC_9.99", "ld-linux-x86-64.so.2");
                    if (a != 0 && wf == 0 && wv == 0) bound++;
                    else printf("[gate m3] BAD %s a=%llx wf=%llx wv=%llx\n",
                                names[i], (unsigned long long)a,
                                (unsigned long long)wf,
                                (unsigned long long)wv);
                }
                printf("[gate m3] remaining ld.so imports bound exactly: "
                       "%zu/%zu\n", bound,
                       sizeof(names) / sizeof(names[0]));
                CHECK(bound == sizeof(names) / sizeof(names[0]),
                      "all remaining ld.so imports bind by exact "
                      "(version, vn_file) and fail closed on near-misses");
            }
        }
        /* Same length, unknown name: vsl_test_shim -> no_such_sym_xx. */
        memcpy(img + 0x1281, "no_such_sym_xx", 13);
        rc = vsl_exec_elf_bytes(img, n, &r);
        printf("[gate m] unknown symbol: rc=%d status=%d (want %d)\n",
               rc, r.status, VSL_EXEC_ERR_RELOC);
        CHECK(rc == VSL_EXEC_ERR_RELOC, "unknown symbol rejected (ERR_RELOC)");

        n = build_dynsym_elf(img);
        /* COPY (5) and IRELATIVE (37) are now supported; use a genuinely
         * unassigned type to prove unsupported relocations still fail. */
        uint64_t ri = 250; memcpy(img + 0x1508, &ri, 8);
        rc = vsl_exec_elf_bytes(img, n, &r);
        printf("[gate m] unsupported reloc type: rc=%d status=%d skipped=%u (want %d)\n",
               rc, r.status, r.relocations_skipped, VSL_EXEC_ERR_RELOCTYPE);
        CHECK(rc == VSL_EXEC_ERR_RELOCTYPE && r.relocations_skipped == 1,
              "unsupported relocation rejected (ERR_RELOCTYPE)");
    }

    /* (n) COPY + IRELATIVE differential gate.  The pre-change rule applied
     *     only RELATIVE, so it would reject this same image with RELOCTYPE. */
    {
        uint8_t img[0x1600];
        size_t n = build_copy_irelative_elf(img);
        CHECK(legacy_rejects_nonrelative(img, n),
              "negative control: pre-change loader rejects COPY/IRELATIVE fixture");
        uint64_t ifunc_before = vsl_dyn_ifunc_call_count();
        rc = vsl_exec_elf_bytes(img, n, &r);
        uint64_t resolved_fn = 0;
        if (r.output_len >= 24) memcpy(&resolved_fn, r.output + 16, 8);
        printf("[gate n] COPY+IRELATIVE: rc=%d exit=%d bias=0x%llx "
               "applied=%u skipped=%u resolved=%u out_len=%zu "
               "copy=\"%.*s\" func=0x%llx ifunc_calls=%llu\n",
               rc, r.exit_code, (unsigned long long)r.load_bias,
               r.relocations_applied, r.relocations_skipped, r.symbols_resolved,
               r.output_len, r.output_len >= 16 ? 16 : (int)r.output_len, r.output,
               (unsigned long long)resolved_fn,
               (unsigned long long)(vsl_dyn_ifunc_call_count() - ifunc_before));
        CHECK(rc == VSL_EXEC_OK && r.exit_code == 0,
              "COPY/IRELATIVE fixture runs to exit(0)");
        CHECK(r.relocations_applied == 2 && r.relocations_skipped == 0,
              "both COPY and IRELATIVE applied, none skipped");
        CHECK(r.symbols_resolved == 1,
              "COPY resolved its named host symbol (IRELATIVE is symbol-less)");
        CHECK(r.output_len == 34, "output: 16 copied + 8 resolved + 10 marker");
        CHECK(r.output_len == 34 &&
              memcmp(r.output, "VSLCOPY-DATA-123", 16) == 0,
              "R_X86_64_COPY copied the EXACT host bytes into guest memory");
        CHECK(resolved_fn == CI_IFUNC_MAGIC,
              "R_X86_64_IRELATIVE wrote the registered resolver's real output");
        CHECK(r.output_len == 34 &&
              memcmp(r.output + 24, "VSL-CI-OK\n", 10) == 0,
              "guest executed past both relocations to the marker");
        CHECK(vsl_dyn_ifunc_call_count() == ifunc_before + 1,
              "IFUNC call counter proves the bounded resolver really ran");

        /* Fail-closed cases: unknown COPY name, oversized COPY, resolver that
         * is not in the bounded IFUNC table.  Each must be ERR_RELOC. */
        n = build_copy_irelative_elf(img);
        img[0x1281] = 'x';                         /* vsl_copy_data -> xsl_* */
        rc = vsl_exec_elf_bytes(img, n, &r);
        printf("[gate n] unknown COPY name: rc=%d status=%d (want %d)\n",
               rc, r.status, VSL_EXEC_ERR_RELOC);
        CHECK(rc == VSL_EXEC_ERR_RELOC, "unknown COPY source rejected");

        n = build_copy_irelative_elf(img);
        {
            uint64_t big = CI_COPY_SZ + 1;
            memcpy(img + 0x1228, &big, 8);         /* st_size > host byte limit */
        }
        rc = vsl_exec_elf_bytes(img, n, &r);
        printf("[gate n] oversized COPY: rc=%d status=%d (want %d)\n",
               rc, r.status, VSL_EXEC_ERR_RELOC);
        CHECK(rc == VSL_EXEC_ERR_RELOC, "oversized COPY rejected (source bound)");

        n = build_copy_irelative_elf(img);
        {
            uint64_t bad = CI_RESOLVER + 1;        /* not in IFUNC table */
            memcpy(img + 0x1528, &bad, 8);
        }
        rc = vsl_exec_elf_bytes(img, n, &r);
        printf("[gate n] unregistered IRELATIVE: rc=%d status=%d (want %d)\n",
               rc, r.status, VSL_EXEC_ERR_RELOC);
        CHECK(rc == VSL_EXEC_ERR_RELOC,
              "unregistered IFUNC resolver rejected (fail closed)");
    }

    /* (n2) Real-glibc IRELATIVE resolver table.  The 42 unique resolver link
     * addresses from the real libc.so.6 fixture (46 relocations) must each be
     * bound through the bounded host table -- no guest byte runs at relocate
     * time.  A single-IRELATIVE hand-built ET_DYN is retargeted to each
     * address in turn; the guest writes the documented host stand-in value. */
    {
        static const uint64_t libc_resolvers[] = {
            0x137650ULL, 0x137770ULL, 0x137890ULL, 0x1379b0ULL,
            0xb16b0ULL,  0xb1730ULL,  0xb1820ULL,  0xb20c0ULL,
            0xb21e0ULL,  0xb2300ULL,  0xb2390ULL,  0xb2540ULL,
            0xb25b0ULL,  0xb2610ULL,  0xb26b0ULL,  0xb3120ULL,
            0xb3190ULL,  0xb3210ULL,  0xb32a0ULL,  0xb43a0ULL,
            0xb4410ULL,  0xb4dc0ULL,  0xb4e50ULL,  0xb4ef0ULL,
            0xb4ff0ULL,  0xb5090ULL,  0xb5150ULL,  0xb51e0ULL,
            0xb5210ULL,  0xb53e0ULL,  0xc87b0ULL,  0xca420ULL,
            0xca980ULL,  0xca9e0ULL,  0xcaa90ULL,  0xcb9f0ULL,
            0xcbc20ULL,  0xcc450ULL,  0xcc4b0ULL,  0xd8fb0ULL,
            0xd9030ULL,  0xd90e0ULL
        };
        const size_t nres = sizeof(libc_resolvers) / sizeof(libc_resolvers[0]);
        uint8_t base[0x1600], img[0x1600];
        size_t bn = build_copy_irelative_elf(base);
        const uint64_t magic = vsl_dyn_ifunc_libc_value();
        uint64_t before = vsl_dyn_ifunc_call_count();
        size_t ok_count = 0;
        for (size_t i = 0; i < nres; i++) {
            memcpy(img, base, bn);
            memcpy(img + 0x1528, &libc_resolvers[i], 8); /* RELA[1] addend */
            rc = vsl_exec_elf_bytes(img, bn, &r);
            if (rc == VSL_EXEC_OK && r.exit_code == 0 &&
                r.output_len == 34 &&
                memcmp(r.output + 16, &magic, 8) == 0) ok_count++;
            else
                printf("[gate n2] resolver 0x%llx: rc=%d status=%d "
                       "reloc_failure=\"%s\"\n",
                       (unsigned long long)libc_resolvers[i], rc, r.status,
                       r.reloc_failure);
        }
        printf("[gate n2] real-glibc IFUNC table: %zu/%zu resolvers bound, "
               "host calls +%llu, standin=0x%llx\n",
               ok_count, nres,
               (unsigned long long)(vsl_dyn_ifunc_call_count() - before),
               (unsigned long long)magic);
        CHECK(nres == 42, "all 42 unique real-glibc resolver addresses listed");
        CHECK(ok_count == nres,
              "every real-glibc IRELATIVE resolver bound via host table");
        CHECK(vsl_dyn_ifunc_call_count() == before + nres,
              "one bounded host resolver call per real-glibc resolver entry");
    }

    /* (o) Bounded TLS differential gate.  A hand-built ET_DYN uses TPOFF64
     *     to dereference a host-provided TLS word and also applies DTPMOD64
     *     and DTPOFF32.  The host seeds the block per image; two runs with
     *     different seeds must emit different TLS values.  The faithful
     *     pre-change rule (RELATIVE-only) rejects the same fixture. */
    {
        uint8_t img[0x1600];
        size_t n = build_tls_elf(img);
        const uint64_t tagA = 0x56534c54584c5431ULL; /* "VSLTXT1" */
        const uint64_t tagB = 0x56534c54584c5432ULL; /* "VSLTXT2" */
        uint8_t seedA[0x28] = {0}, seedB[0x28] = {0};
        memcpy(seedA + TLS_SYM_VALUE, &tagA, 8);
        memcpy(seedB + TLS_SYM_VALUE, &tagB, 8);
        VslImageCfg cfgA = { NULL, 0, 0, seedA, sizeof(seedA), NULL };
        VslImageCfg cfgB = { NULL, 0, 0, seedB, sizeof(seedB), NULL };
        uint64_t outA = 0, outB = 0;

        /* Negative control: the pre-change loader applied ONLY RELATIVE and
         * rejected every other type with ERR_RELOCTYPE.  TPOFF64(18) is one. */
        CHECK(legacy_rejects_nonrelative(img, n),
              "negative control: pre-change loader rejects TPOFF64 fixture");

        for (int pass = 0; pass < 2; pass++) {
            rc = vsl_exec_image(img, n, pass ? &cfgB : &cfgA, &r);
            printf("[gate o] run %d: rc=%d exit=%d out_len=%zu tls_relocs=%u "
                   "tls_base=0x%llx host_base=0x%llx\n",
                   pass, rc, r.exit_code, r.output_len, r.tls_relocations,
                   (unsigned long long)r.tls_base,
                   (unsigned long long)vsl_tls_base());
            CHECK(rc == VSL_EXEC_OK && r.exit_code == 0,
                  "TLS ET_DYN runs to exit(0)");
            CHECK(r.output_len == 20, "TLS guest emitted 20 bytes");
            uint64_t seen = 0, mod = 0;
            uint32_t off = 0;
            if (r.output_len >= 20) {
                memcpy(&seen, r.output, 8);
                memcpy(&mod, r.output + 8, 8);
                memcpy(&off, r.output + 16, 4);
            }
            if (pass == 0) outA = seen; else outB = seen;
            printf("[gate o] run %d: tls=0x%llx mod=%llu off=0x%x\n",
                   pass, (unsigned long long)seen,
                   (unsigned long long)mod, (unsigned)off);
            CHECK(seen == (pass ? tagB : tagA),
                  "TPOFF64 dereferenced THIS image's host TLS value");
            CHECK(mod == 1, "DTPMOD64 wrote bounded module id 1");
            CHECK(off == TLS_SYM_VALUE, "DTPOFF32 wrote the in-block offset");
            CHECK(r.tls_relocations == 3, "all three TLS relocations applied");
            CHECK(r.tls_base == vsl_tls_base() && r.tls_base != 0,
                  "result TLS base == host-provided block address");
        }
        CHECK(outA != outB && outA == tagA && outB == tagB,
              "per-image TLS values differ across two runs");

        /* Symbol-less (STN_UNDEF) TPOFF64: the DEFINED MODULE TLS BASE is the
         * only source (S = 0), so the written word must be tls_base + A.
         * Mutate RELA[0] of the same fixture to sym 0 / addend 0x40 and seed
         * that offset: the guest dereferences the exact written address. */
        {
            uint8_t symless[0x1600];
            memcpy(symless, img, n);
            uint64_t ri = 18;                       /* sym 0, R_X86_64_TPOFF64 */
            uint64_t add = 0x40;
            memcpy(symless + 0x1508, &ri, 8);       /* RELA[0].r_info */
            memcpy(symless + 0x1510, &add, 8);      /* RELA[0].r_addend */
            uint8_t seed[0x48] = {0};               /* 0x40 + 8-byte tag */
            const uint64_t tagS = 0x56534c54584c5331ULL; /* "VSLTLS1" */
            memcpy(seed + 0x40, &tagS, 8);
            VslImageCfg cfgS = { NULL, 0, 0, seed, sizeof(seed), NULL };
            rc = vsl_exec_image(symless, n, &cfgS, &r);
            printf("[gate o] symbol-less TPOFF64: rc=%d exit=%d out_len=%zu "
                   "tls_relocs=%u seen=0x%llx\n",
                   rc, r.exit_code, r.output_len, r.tls_relocations,
                   (unsigned long long)(r.output_len >= 8 ?
                                        *(uint64_t *)r.output : 0));
            CHECK(rc == VSL_EXEC_OK && r.exit_code == 0,
                  "symbol-less TPOFF64 image runs to exit(0)");
            CHECK(r.output_len == 20 &&
                  *(uint64_t *)r.output == tagS,
                  "symbol-less TPOFF64 dereferenced tls_base + addend exactly");
            CHECK(r.tls_relocations == 3,
                  "symbol-less TPOFF64 counted as an applied TLS relocation");
            CHECK(r.tls_base == vsl_tls_base() && r.tls_base != 0,
                  "symbol-less path uses the defined module TLS base");

            /* Fail closed: an out-of-block addend is not a guessed TLS model. */
            uint64_t far = 0x200;                   /* > EX_TLS_BLOCK_MAX - 8 */
            memcpy(symless + 0x1510, &far, 8);
            rc = vsl_exec_elf_bytes(symless, n, &r);
            printf("[gate o] symbol-less TPOFF64 out-of-range: rc=%d "
                   "status=%d reloc_failure=\"%s\"\n",
                   rc, r.status, r.reloc_failure);
            CHECK(rc == VSL_EXEC_ERR_RELOC &&
                  strstr(r.reloc_failure, "TLS relocation type 18 symbol 0") != NULL,
                  "out-of-range symbol-less TPOFF64 addend fails closed");
        }

        /* Fail-closed: a non-STT_TLS symbol cannot satisfy a TLS relocation. */
        {
            uint8_t bad[0x1600];
            memcpy(bad, img, n);
            bad[0x121c] = 0x12;                    /* GLOBAL|FUNC, not TLS */
            rc = vsl_exec_elf_bytes(bad, n, &r);
            printf("[gate o] non-STT_TLS symbol: rc=%d status=%d (want %d)\n",
                   rc, r.status, VSL_EXEC_ERR_RELOC);
            CHECK(rc == VSL_EXEC_ERR_RELOC,
                  "TLS relocation on a non-STT_TLS symbol rejected (ERR_RELOC)");
        }
        /* Fail-closed: an STT_TLS symbol in a PLAIN relocation is refused. */
        {
            uint8_t bad[0x1600];
            memcpy(bad, img, n);
            uint64_t ri = ((uint64_t)1 << 32) | 6;  /* DTPMOD64 -> GLOB_DAT */
            memcpy(bad + 0x1518, &ri, 8);
            rc = vsl_exec_elf_bytes(bad, n, &r);
            CHECK(rc == VSL_EXEC_ERR_RELOC,
                  "STT_TLS symbol in a plain relocation rejected (ERR_RELOC)");
        }
        /* Fail-closed: out-of-block st_value. */
        {
            uint8_t bad[0x1600];
            memcpy(bad, img, n);
            uint64_t big = VSL_TLS_BLOCK_MAX + 1;
            memcpy(bad + 0x1220, &big, 8);
            rc = vsl_exec_elf_bytes(bad, n, &r);
            printf("[gate o] oversized TLS offset: rc=%d status=%d (want %d)\n",
                   rc, r.status, VSL_EXEC_ERR_RELOC);
            CHECK(rc == VSL_EXEC_ERR_RELOC,
                  "out-of-block TLS offset rejected (ERR_RELOC)");
        }
        /* Fail-closed: other TLS models stay unsupported (TLSGD, TLSDESC). */
        {
            static const uint32_t unsupported[] = { 19, 36 };
            for (size_t i = 0; i < 2; i++) {
                uint8_t bad[0x1600];
                memcpy(bad, img, n);
                uint64_t ri = ((uint64_t)1 << 32) | unsupported[i];
                memcpy(bad + 0x1508, &ri, 8);
                rc = vsl_exec_elf_bytes(bad, n, &r);
                printf("[gate o] unsupported TLS type %u: rc=%d status=%d "
                       "skipped=%u (want %d)\n",
                       unsupported[i], rc, r.status, r.relocations_skipped,
                       VSL_EXEC_ERR_RELOCTYPE);
                CHECK(rc == VSL_EXEC_ERR_RELOCTYPE && r.relocations_skipped == 1,
                      "unsupported TLS model rejected (ERR_RELOCTYPE)");
            }
        }
    }

    /* (p) Bounded DT_NEEDED differential gate.  The main object imports
     *     vsl_dep_entry from libvsldep.so, a freestanding ET_DYN found ONLY
     *     in the allow-listed guest dir.  Pre-change loader ignored DT_NEEDED
     *     and rejected the unresolved import; here the dep is mapped first,
     *     relocated, its export merged, then the main relocates and calls it.
     *     Output order (dependency marker BEFORE main marker) proves load
     *     order and real control flow, not just a mapped file. */
    {
        size_t n = 0;
        snprintf(path, sizeof(path), "%s/guest_dtneeded.elf", dir);
        uint8_t *img = read_test_file(path, &n);
        CHECK(img != NULL, "DT_NEEDED main fixture readable");
        if (img) {
            CHECK(legacy_rejects_dtneeded(img, n),
                  "negative control: pre-change loader rejects DT_NEEDED fixture");
            CHECK(vsl_dyn_set_dep_search_dir(dir) == 0,
                  "allow-listed dependency search dir accepted");
            rc = vsl_exec_elf_bytes(img, n, &r);
            uint64_t magic = 0;
            if (r.output_len >= 22) memcpy(&magic, r.output + 14, 8);
            const char *dep0 = vsl_dyn_dep_name(0);
            uint64_t depfn = vsl_dyn_dep_symbol("vsl_dep_entry");
            printf("[gate p] DT_NEEDED: rc=%d exit=%d deps=%u dep0=%s out_len=%zu "
                   "out=\"%.*s\" magic=0x%llx depfn=0x%llx\n",
                   rc, r.exit_code, r.deps_loaded, dep0 ? dep0 : "(none)",
                   r.output_len, (int)r.output_len, r.output,
                   (unsigned long long)magic, (unsigned long long)depfn);
            CHECK(rc == VSL_EXEC_OK && r.exit_code == 0,
                  "DT_NEEDED main runs to exit(0)");
            CHECK(r.deps_loaded == 1, "exactly one dependency loaded");
            CHECK(dep0 && strcmp(dep0, "libvsldep.so") == 0,
                  "load order: libvsldep.so is dependency 0");
            CHECK(vsl_dyn_dep_name(1) == NULL,
                  "no phantom dependency in the load-order view");
            CHECK(r.output_len == 33,
                  "output: 14 dep + 8 magic + 11 main bytes");
            CHECK(r.output_len == 33 &&
                  memcmp(r.output, "VSLDEP-CALLED\n", 14) == 0,
                  "dependency code ran FIRST (loaded before main relocations)");
            CHECK(magic == 0x56534445504d4141ULL,
                  "imported vsl_dep_entry returned its real magic via merged table");
            CHECK(r.output_len == 33 &&
                  memcmp(r.output + 22, "VSLMAIN-OK\n", 11) == 0,
                  "main continued after calling into the dependency");
            CHECK(depfn >= VSL_USER_BASE && depfn < VSL_USER_BASE + VSL_USER_SIZE,
                  "merged dep symbol address lies inside the VSL trap window");

            /* Negative control: no allow-listed dir -> missing dependency. */
            CHECK(vsl_dyn_set_dep_search_dir(NULL) == 0,
                  "dependency allow-list cleared");
            rc = vsl_exec_elf_bytes(img, n, &r);
            printf("[gate p] missing dep (no allow-list): rc=%d status=%d "
                   "(want %d)\n", rc, r.status, VSL_EXEC_ERR_RELOC);
            CHECK(rc == VSL_EXEC_ERR_RELOC,
                  "missing dependency refused (fail-closed ERR_RELOC)");
            CHECK(vsl_dyn_dep_name(0) == NULL,
                  "failed load leaves no stale dependency state");

            /* Negative control: soname absent from an otherwise valid dir. */
            CHECK(vsl_dyn_set_dep_search_dir(dir) == 0,
                  "dependency allow-list restored");
            uint8_t *at = NULL;
            for (size_t i = 0; i + 12 <= n; i++)
                if (memcmp(img + i, "libvsldep.so", 12) == 0) { at = img + i; break; }
            CHECK(at != NULL, "DT_NEEDED soname located in fixture");
            if (at) {
                uint8_t saved = at[8];
                at[8] = 'X';                       /* libvsldep.so -> libvsldeX.so */
                rc = vsl_exec_elf_bytes(img, n, &r);
                printf("[gate p] missing soname: rc=%d status=%d (want %d)\n",
                       rc, r.status, VSL_EXEC_ERR_RELOC);
                CHECK(rc == VSL_EXEC_ERR_RELOC,
                      "unknown/missing DT_NEEDED soname refused (ERR_RELOC)");
                at[8] = saved;
            }
            CHECK(vsl_dyn_set_dep_search_dir(NULL) == 0,
                  "dependency allow-list cleared after gate");
            free(img);
        }
    }

    /* (q) Bounded lazy first-call PLT binding + DT_INIT/DT_FINI execution.
     *     A real gcc -shared object imports vsl_test_shim (called twice
     *     through one JUMP_SLOT) and write (called once WITH arguments).
     *     Each slot must resolve exactly once on its first call.
     *     .init_array writes a marker before _start and .fini_array on unload.
     *     Negative control: the SAME bytes with both opt-in flags OFF (the
     *     pre-change loader behaviour) produce no markers and no resolver
     *     count -- the new feature, not the fixture, is what is being tested.
     *     Symbol versioning is gated separately in (r) below. */
    {
        size_t n = 0;
        snprintf(path, sizeof(path), "%s/guest_initplt.elf", dir);
        uint8_t *img = read_test_file(path, &n);
        CHECK(img != NULL, "lazy-PLT/init-fini main fixture readable");
        if (img) {
            uint64_t shim_before = vsl_dyn_shim_call_count();
            uint64_t wr_before = vsl_dyn_write_call_count();
            uint64_t resolve_before = vsl_dyn_plt_resolve_count();
            vsl_dyn_set_lazy_plt(1);
            vsl_dyn_set_init_fini(1);
            rc = vsl_exec_elf_bytes(img, n, &r);
            uint64_t a = 0, b = 0;
            if (r.output_len >= 25) {
                memcpy(&a, r.output + 9, 8);
                memcpy(&b, r.output + 17, 8);
            }
            printf("[gate q] lazy PLT + init/fini: rc=%d exit=%d out_len=%zu "
                   "slots=%u resolves=%u init=%u fini=%u out=\"%.*s\"\n",
                   rc, r.exit_code, r.output_len, r.plt_slots_bound,
                   r.plt_resolves, r.init_called, r.fini_called,
                   (int)r.output_len, r.output);
            CHECK(rc == VSL_EXEC_OK && r.exit_code == 0,
                  "lazy-PLT/init-fini fixture runs to exit(0)");
            CHECK(r.output_len == 34, "output: 9 init + 16 magic + 9 fini bytes");
            CHECK(r.output_len >= 9 && memcmp(r.output, "VSL-INIT\n", 9) == 0,
                  "init_array marker written BEFORE main runs");
            CHECK(a == 0x56534c53484d4949ULL && b == 0x56534c53484d4949ULL,
                  "both lazy PLT calls returned the real shim magic");
            CHECK(r.output_len == 34 &&
                  memcmp(r.output + 25, "VSL-FINI\n", 9) == 0,
                  "fini_array marker written on unload");
            CHECK(r.plt_slots_bound == 2,
                  "both JUMP_SLOTs got lazy stubs");
            CHECK(r.plt_resolves == 2,
                  "each slot resolved ONCE: shim twice + write with args");
            CHECK(vsl_dyn_plt_resolve_count() == resolve_before + 2,
                  "lifetime resolver count advanced by exactly two");
            CHECK(vsl_dyn_shim_call_count() == shim_before + 2,
                  "host shim invoked twice through the bound GOT");
            CHECK(vsl_dyn_write_call_count() == wr_before + 1,
                  "lazy write@plt preserved fd/buf/len arguments");
            CHECK(r.init_called == 1 && r.fini_called == 1,
                  "one init and one fini function invoked");

            /* Negative control: pre-change behaviour (flags off). */
            vsl_dyn_set_lazy_plt(0);
            vsl_dyn_set_init_fini(0);
            resolve_before = vsl_dyn_plt_resolve_count();
            rc = vsl_exec_elf_bytes(img, n, &r);
            uint64_t a2 = 0, b2 = 0;
            if (r.output_len >= 16) {
                memcpy(&a2, r.output, 8);
                memcpy(&b2, r.output + 8, 8);
            }
            printf("[gate q] negative control (flags off): rc=%d exit=%d "
                   "out_len=%zu slots=%u resolves=%u init=%u fini=%u "
                   "out=\"%.*s\"\n",
                   rc, r.exit_code, r.output_len, r.plt_slots_bound,
                   r.plt_resolves, r.init_called, r.fini_called,
                   (int)r.output_len, r.output);
            CHECK(rc == VSL_EXEC_OK && r.exit_code == 0,
                  "negative control: eager guest still runs to exit(0)");
            CHECK(r.output_len == 16, "negative control: only 16 magic bytes, no markers");
            CHECK(r.output_len < 9 || memcmp(r.output, "VSL-INIT\n", 9) != 0,
                  "negative control: init marker MISSING (old loader)");
            CHECK(r.plt_slots_bound == 0 && r.plt_resolves == 0,
                  "negative control: no stubs, no resolver calls");
            CHECK(vsl_dyn_plt_resolve_count() == resolve_before,
                  "negative control: lifetime resolver count unchanged");
            CHECK(r.init_called == 0 && r.fini_called == 0,
                  "negative control: no init/fini invoked (old loader)");
            CHECK(a2 == 0x56534c53484d4949ULL && b2 == 0x56534c53484d4949ULL,
                  "negative control: eager JUMP_SLOT still calls shim twice");

            free(img);
        }
    }

    /* (r) Bounded ELF symbol versioning + strong/weak preference differential.
     *     The main fixture imports vsl_ver_entry@VSL_2 and vsl_weakpick@VSL_1.
     *     Dependency A (first DT_NEEDED) has vsl_ver_entry@VSL_1 (WRONG) and
     *     a WEAK vsl_weakpick@VSL_1; dependency B has the requested VSL_2
     *     alias (hidden/weak) and a STRONG vsl_weakpick@VSL_1.  A base-name/
     *     first-wins loader picks A's wrong default version; the bounded
     *     loader matches version names and prefers strong over weak within
     *     the same version.  Negative controls run the SAME bytes with
     *     DT_VERSYM cleared (pre-change behaviour) and with the required
     *     version renamed to a missing name. */
    {
        size_t n = 0;
        snprintf(path, sizeof(path), "%s/guest_symver.elf", dir);
        uint8_t *img = read_test_file(path, &n);
        CHECK(img != NULL, "symbol-versioning main fixture readable");
        if (img) {
            CHECK(vsl_dyn_set_dep_search_dir(dir) == 0,
                  "versioning gate: dependency allow-list accepted");
            rc = vsl_exec_elf_bytes(img, n, &r);
            uint64_t ver = 0, weakm = 0;
            if (r.output_len >= 16) {
                memcpy(&ver, r.output, 8);
                memcpy(&weakm, r.output + 8, 8);
            }
            printf("[gate r] versioned: rc=%d exit=%d deps=%u out_len=%zu "
                   "ver=0x%llx weak=0x%llx out=\"%.*s\"\n",
                   rc, r.exit_code, r.deps_loaded, r.output_len,
                   (unsigned long long)ver, (unsigned long long)weakm,
                   (int)r.output_len, r.output);
            CHECK(rc == VSL_EXEC_OK && r.exit_code == 0,
                  "versioned guest runs to exit(0)");
            CHECK(r.deps_loaded == 2, "both versioned dependencies loaded");
            CHECK(r.output_len == 16, "two 8-byte magic words emitted");
            CHECK(ver == 0x4242523200000002ULL,
                  "vsl_ver_entry@VSL_2 resolved from dep B, not A's VSL_1");
            CHECK(weakm == 0x5354524f00000002ULL,
                  "strong vsl_weakpick@VSL_1 beats A's weak definition");

            /* Negative control 1: same bytes, no version data -> old rule. */
            uint8_t *old = (uint8_t *)malloc(n);
            CHECK(old != NULL, "legacy control copy allocated");
            if (old) {
                memcpy(old, img, n);
                CHECK(legacy_strip_versions(old, n) == 1,
                      "negative control: DT_VERSYM cleared (pre-change rule)");
                rc = vsl_exec_elf_bytes(old, n, &r);
                uint64_t over = 0, oweak = 0;
                if (r.output_len >= 16) {
                    memcpy(&over, r.output, 8);
                    memcpy(&oweak, r.output + 8, 8);
                }
                printf("[gate r] negative control (base-name only): "
                       "rc=%d exit=%d out_len=%zu ver=0x%llx weak=0x%llx "
                       "out=\"%.*s\"\n",
                       rc, r.exit_code, r.output_len,
                       (unsigned long long)over, (unsigned long long)oweak,
                       (int)r.output_len, r.output);
                CHECK(rc == VSL_EXEC_OK && r.exit_code == 0,
                      "legacy control: guest still runs (old loader succeeded)");
                CHECK(over == 0x4141523100000001ULL,
                      "old loader picks WRONG default vsl_ver_entry@VSL_1 from dep A");
                CHECK(over != ver,
                      "version-name matching changes the resolved definition");
                CHECK(oweak == weakm,
                      "strong-over-weak preference stays active without version tags");
                free(old);
            }

            /* Negative control 2: required version has NO definition. */
            uint8_t *bad = (uint8_t *)malloc(n);
            CHECK(bad != NULL, "missing-version copy allocated");
            if (bad) {
                memcpy(bad, img, n);
                CHECK(rename_version(bad, n, "VSL_2", '9') == 1,
                      "missing-version requirement renamed VSL_2 -> VSL_9");
                rc = vsl_exec_elf_bytes(bad, n, &r);
                printf("[gate r] missing version: rc=%d status=%d (want %d)\n",
                       rc, r.status, VSL_EXEC_ERR_RELOC);
                CHECK(rc == VSL_EXEC_ERR_RELOC,
                      "missing version definition refused (fail-closed ERR_RELOC)");
                /* Same mutated bytes under the old base-name-only rule: the
                 * unknown version is ignored and the guest would still run. */
                CHECK(legacy_strip_versions(bad, n) == 1,
                      "missing-version control: DT_VERSYM cleared");
                rc = vsl_exec_elf_bytes(bad, n, &r);
                printf("[gate r] missing version (base-name only): "
                       "rc=%d exit=%d out_len=%zu\n",
                       rc, r.exit_code, r.output_len);
                CHECK(rc == VSL_EXEC_OK && r.exit_code == 0,
                      "old loader ignores the missing version and succeeds");
                free(bad);
            }
            CHECK(vsl_dyn_set_dep_search_dir(NULL) == 0,
                  "versioning gate: dependency allow-list cleared");
            free(img);
        }
    }

/* (s) DT_VERNEED vn_file enforcement differential.  The unmodified
 *     fixture requires VSL_2 from libvslverB.so and VSL_1 from
 *     libvslverA.so, so hardened mode still succeeds.  A copy whose
 *     VSL_2 vn_file is retargeted at libvslverA.so (which has no VSL_2)
 *     must fail closed; the legacy any-file rule still resolves it from
 *     libvslverB.so -- the old-loader differential. */
{
    size_t n = 0;
    snprintf(path, sizeof(path), "%s/guest_symver.elf", dir);
    uint8_t *img = read_test_file(path, &n);
    CHECK(img != NULL, "vn_file gate: fixture readable");
    if (img) {
        const char *dirs[] = { dir, NULL };
        VslImageCfg hard = { dirs, 0, 0, NULL, 0, NULL };   /* enforce vn_file */
        VslImageCfg old  = { dirs, 1, 1, NULL, 0, NULL };   /* pre-change rules */
        rc = vsl_exec_image(img, n, &hard, &r);
        uint64_t ver = 0, weakm = 0;
        if (r.output_len >= 16) {
            memcpy(&ver, r.output, 8);
            memcpy(&weakm, r.output + 8, 8);
        }
        printf("[gate s] hardened unmodified: rc=%d exit=%d ver=0x%llx "
               "weak=0x%llx\n", rc, r.exit_code,
               (unsigned long long)ver, (unsigned long long)weakm);
        CHECK(rc == VSL_EXEC_OK && r.exit_code == 0,
              "vn_file gate: legitimate version needs still run");
        CHECK(ver == 0x4242523200000002ULL,
              "VSL_2 still resolves from its declared libvslverB.so");
        CHECK(weakm == 0x5745414b00000001ULL,
              "vn_file confines VSL_1 to libvslverA.so (weak definition)");

        uint8_t *bad = (uint8_t *)malloc(n);
        CHECK(bad != NULL, "vn_file gate: control copy allocated");
        if (bad) {
            memcpy(bad, img, n);
            CHECK(patch_verneed_file(bad, n, "libvslverB.so",
                                     "libvslverA.so") == 1,
                  "vn_file retargeted B -> A on copy (DT_NEEDED unchanged)");
            int rc_hard = vsl_exec_image(bad, n, &hard, &r);
            printf("[gate s] hardened wrong-file: rc=%d status=%d "
                   "(want %d)\n", rc_hard, r.status, VSL_EXEC_ERR_RELOC);
            CHECK(rc_hard == VSL_EXEC_ERR_RELOC,
                  "version required from libA is NOT satisfied by libB");
            int rc_old = vsl_exec_image(bad, n, &old, &r);
            uint64_t over = 0;
            if (r.output_len >= 16) memcpy(&over, r.output, 8);
            printf("[gate s] legacy wrong-file: rc=%d exit=%d "
                   "ver=0x%llx\n", rc_old, r.exit_code,
                   (unsigned long long)over);
            CHECK(rc_old == VSL_EXEC_OK && r.exit_code == 0,
                  "old loader ignores vn_file and still succeeds");
            CHECK(over == 0x4242523200000002ULL,
                  "old loader satisfies VSL_2 from the wrong file");
            CHECK(rc_hard != rc_old,
                  "vn_file enforcement changes the outcome (differential)");
            free(bad);
        }
        free(img);
    }
}

/* (t) Weak vs strong UNDEF differential.  An undefined WEAK symbol
 *     with no definition binds to 0 in hardened mode; the same bytes
 *     under the legacy fail-closed rule are rejected.  The STRONG
 *     variant must still fail closed under both rules. */
{
    size_t nw = 0, ns = 0;
    const char *no_dirs[] = { NULL };
    VslImageCfg hard = { no_dirs, 0, 0, NULL, 0, NULL };
    VslImageCfg old  = { no_dirs, 0, 1, NULL, 0, NULL };
    snprintf(path, sizeof(path), "%s/guest_weakundef.elf", dir);
    uint8_t *w = read_test_file(path, &nw);
    CHECK(w != NULL, "weak-UNDEF gate: fixture readable");
    if (w) {
        rc = vsl_exec_image(w, nw, &hard, &r);
        uint64_t addr = 0xdeadbeefULL;
        if (r.output_len >= 8) memcpy(&addr, r.output, 8);
        printf("[gate t] weak hardened: rc=%d exit=%d out_len=%zu "
               "addr=0x%llx\n", rc, r.exit_code, r.output_len,
               (unsigned long long)addr);
        CHECK(rc == VSL_EXEC_OK && r.exit_code == 0 && r.output_len == 8,
              "missing weak definition binds to 0 and guest runs");
        CHECK(addr == 0, "undefined weak resolved address is exactly 0");
        int rc_old = vsl_exec_image(w, nw, &old, &r);
        printf("[gate t] weak legacy: rc=%d status=%d (want %d)\n",
               rc_old, r.status, VSL_EXEC_ERR_RELOC);
        CHECK(rc_old == VSL_EXEC_ERR_RELOC,
              "old loader fails closed on the missing weak symbol");
        CHECK(rc != rc_old, "weak-UNDEF semantics differ from old loader");
        free(w);
    }
    snprintf(path, sizeof(path), "%s/guest_strongundef.elf", dir);
    uint8_t *simg = read_test_file(path, &ns);
    CHECK(simg != NULL, "strong-UNDEF control: fixture readable");
    if (simg) {
        rc = vsl_exec_image(simg, ns, &hard, &r);
        printf("[gate t] strong hardened: rc=%d status=%d (want %d)\n",
               rc, r.status, VSL_EXEC_ERR_RELOC);
        CHECK(rc == VSL_EXEC_ERR_RELOC,
              "strong UNDEF still fails closed (no silent zero binding)");
        free(simg);
    }
}

/* (u) Per-image dependency search directory list.  Two dirs hold
 *     objects with the SAME soname but different magics.  A per-image
 *     cfg selects dirB while the process-wide allow-list stays dirA;
 *     the legacy entry point still sees dirA -- the differential. */
{
    size_t n = 0;
    char dirA[512], dirB[512];
    snprintf(dirA, sizeof(dirA), "%s/dirA", dir);
    snprintf(dirB, sizeof(dirB), "%s/dirB", dir);
    const char *onlyA[] = { dirA, NULL };
    const char *onlyB[] = { dirB, NULL };
    const char *skips[] = { "/nonexistent-vsl-dep-dir", dirB, NULL };
    VslImageCfg cfgA = { onlyA, 0, 0, NULL, 0, NULL };
    VslImageCfg cfgB = { onlyB, 0, 0, NULL, 0, NULL };
    VslImageCfg cfgSkip = { skips, 0, 0, NULL, 0, NULL };
    snprintf(path, sizeof(path), "%s/guest_imgdep.elf", dir);
    uint8_t *img = read_test_file(path, &n);
    CHECK(img != NULL, "per-image dir gate: fixture readable");
    if (img) {
        CHECK(vsl_dyn_set_dep_search_dir(dirA) == 0,
              "legacy process-wide allow-list set to dirA");
        int rcb = vsl_exec_image(img, n, &cfgB, &r);
        uint64_t mb = 0;
        if (r.output_len >= 8) memcpy(&mb, r.output, 8);
        printf("[gate u] per-image dirB: rc=%d deps=%u magic=0x%llx\n",
               rcb, r.deps_loaded, (unsigned long long)mb);
        CHECK(rcb == VSL_EXEC_OK && r.deps_loaded == 1,
              "per-image list loaded the dependency from dirB");
        CHECK(mb == 0x4449524200000002ULL,
              "per-image dirB copy supplied vsl_imgdep_magic");

        int rca = vsl_exec_elf_bytes(img, n, &r);   /* legacy loader */
        uint64_t ma = 0;
        if (r.output_len >= 8) memcpy(&ma, r.output, 8);
        printf("[gate u] legacy global dirA: rc=%d deps=%u magic=0x%llx\n",
               rca, r.deps_loaded, (unsigned long long)ma);
        CHECK(rca == VSL_EXEC_OK && ma == 0x4449524100000001ULL,
              "legacy global allow-list still resolves dirA");
        CHECK(ma != mb,
              "per-image search list differs from the process-wide list");
        CHECK(rcb != rca || mb != ma,
              "per-image directory selection is a real differential");

        rc = vsl_exec_image(img, n, &cfgA, &r);
        uint64_t ma2 = 0;
        if (r.output_len >= 8) memcpy(&ma2, r.output, 8);
        CHECK(rc == VSL_EXEC_OK && ma2 == ma,
              "per-image run never mutated the global allow-list");
        rc = vsl_exec_image(img, n, &cfgSkip, &r);
        uint64_t ms = 0;
        if (r.output_len >= 8) memcpy(&ms, r.output, 8);
        printf("[gate u] multi-dir skip: rc=%d magic=0x%llx\n",
               rc, (unsigned long long)ms);
        CHECK(rc == VSL_EXEC_OK && ms == mb,
              "empty first dir skipped; later dir in the list found dep");
        CHECK(vsl_dyn_set_dep_search_dir(NULL) == 0,
              "per-image dir gate: global allow-list cleared");
        free(img);
    }
}

/* (v) Bounded PT_INTERP / REAL glibc ld.so bootstrap gate.
 *     A REAL dynamically linked gcc binary (plain `gcc`, no -static) has
 *     PT_INTERP=/lib64/ld-linux-x86-64.so.2 and imports __libc_start_main,
 *     __cxa_finalize and the weak _ITM_* / __gmon_start__ GLOB_DATs.  The real
 *     host ld.so (copied into glibc_rootfs/lib64) is static-pie with ONE
 *     IRELATIVE (resolver 0x1a8d0, returns 0 on both paths), two self-defined
 *     __rseq_* GLOB_DATs and 11 DT_RELR relative relocations.  This leg binds
 *     that resolver from VSL's host IFUNC table, adds bounded DT_RELR, and a
 *     minimal _dl_* host surface.  A kernel hands control to the loader
 *     without pre-relocating the main image, so VSL skips main relocation /
 *     DT_NEEDED preload when a genuine ld-linux interpreter is loaded.
 *  VERIFIED OUTCOME: real ld.so guest bytes execute (syscalls>0), then fault
 *  at an exact recorded address; the engine survives and stays runnable.  A
 *  full hello-world run remains infeasible in this bounded model.  The
 *  pre-change loader ignored PT_INTERP and never loaded an interpreter. */
{
    size_t n = 0;
    const char *none[] = { NULL };
    snprintf(path, sizeof(path), "%s/guest_glibc_dyn.elf", dir);
    uint8_t *img = read_test_file(path, &n);
    CHECK(img != NULL, "PT_INTERP gate: real dynamic gcc binary readable");
    if (img) {
        char rootfs[512];
        snprintf(rootfs, sizeof(rootfs), "%s/glibc_rootfs", dir);
        VslImageCfg cfg = { none, 0, 0, NULL, 0, rootfs };

        /* Negative control: pre-change loader (PT_INTERP ignored). */
        CHECK(legacy_rejects_glibc_dyn(img, n) == 1,
              "negative control: fixture really carries PT_INTERP");
        vsl_dyn_set_interp(0);
        int rc_old = vsl_exec_image(img, n, NULL, &r);
        uint32_t old_loaded = r.interp_loaded;
        printf("[gate v] old loader: rc=%d status=%d interp_present=%u "
               "interp_loaded=%u\n",
               rc_old, r.status, r.interp_present, old_loaded);
        CHECK(rc_old == VSL_EXEC_ERR_RELOC,
              "old loader rejects the real glibc binary (ERR_RELOC)");
        CHECK(old_loaded == 0,
              "negative control: legacy loader never loaded an interpreter");

        vsl_dyn_set_interp(1);
        int rc_new = vsl_exec_image(img, n, &cfg, &r);
        printf("[gate v] new loader: rc=%d status=%d interp_present=%u "
               "path=\"%s\" resolved=%u loaded=%u reason=\"%s\" "
               "fault=0x%llx syscalls=%llu out_len=%zu out=\"%.*s\"\n",
               rc_new, r.status, r.interp_present, r.interp_path,
               r.interp_resolved, r.interp_loaded, r.interp_reason,
               (unsigned long long)r.fault_addr,
               (unsigned long long)r.syscalls_trapped, r.output_len,
               (int)(r.output_len < 64 ? r.output_len : 64), r.output);
        CHECK(r.interp_present == 1, "PT_INTERP recognised on the real binary");
        CHECK(strcmp(r.interp_path, "/lib64/ld-linux-x86-64.so.2") == 0,
              "bounded interpreter path is the exact ld.so soname");
        CHECK(r.interp_resolved == 1,
              "ld.so path resolved inside the guest rootfs only");
        CHECK(r.interp_loaded == 1,
              "REAL glibc ld.so mapped + self-relocated (IRELATIVE+RELR+COPY)");
        CHECK(rc_new == VSL_EXEC_ERR_FAULT,
              "real glibc ld.so GUEST BYTES executed and faulted (engine survived)");
        CHECK(r.fault_addr != 0,
              "exact guest fault address recorded as the bootstrap boundary");
        CHECK(r.syscalls_trapped > 0,
              "ld.so dispatched real guest syscalls through VSL before faulting");
        CHECK(old_loaded != r.interp_loaded,
              "differential: interpreter loaded ONLY by the new loader");
        vsl_dyn_set_interp(0);
        free(img);
    }
}

/* (w) Positive bounded-interpreter gate.  A synthetic ET_DYN main image
 *     carries PT_INTERP=/libvslinterp.so and DT_INIT/DT_FINI.  The loader
 *     maps the freestanding interpreter from the guest rootfs, invokes
 *     DT_INIT first, hands control to the interpreter (NOT to the main
 *     entry), and the interpreter reads the kernel-shaped auxv off its own
 *     stack and emits its exact tag/value pairs.  DT_FINI runs on unload. */
{
    uint8_t img[0x800];
    size_t n = build_interp_main_elf(img);
    const char *none[] = { NULL };
    char rootfs[512];
    snprintf(rootfs, sizeof(rootfs), "%s/glibc_rootfs", dir);
    VslImageCfg cfg = { none, 0, 0, NULL, 0, rootfs };
    CHECK(n == 0x800, "interp-main fixture assembled");

    /* Negative control: with PT_INTERP handling OFF the old rule calls the
     * main entry directly (writes 'M', exits 77) -- no auxv, no interp. */
    CHECK(legacy_ignores_interp() == 1, "legacy rule: ignore PT_INTERP");
    vsl_dyn_set_interp(0);
    vsl_dyn_set_init_fini(0);
    rc = vsl_exec_image(img, n, NULL, &r);
    printf("[gate w] old loader: rc=%d exit=%d out=\"%.*s\"\n",
           rc, r.exit_code, (int)r.output_len, r.output);
    CHECK(rc == VSL_EXEC_OK && r.exit_code == 77,
          "old loader calls main entry (exit 77) and ignores PT_INTERP");
    CHECK(r.output_len == 1 && r.output[0] == 'M',
          "old loader output is only the main-entry marker");

    vsl_dyn_set_interp(1);
    vsl_dyn_set_init_fini(1);
    rc = vsl_exec_image(img, n, &cfg, &r);
    printf("[gate w] new loader: rc=%d exit=%d deps=%u out_len=%zu "
           "interp=%u resolved=%u bias=0x%llx auxv=%u init=%u fini=%u\n",
           rc, r.exit_code, r.deps_loaded, r.output_len,
           r.interp_loaded, r.interp_resolved,
           (unsigned long long)r.interp_bias, r.auxv_entries,
           r.init_called, r.fini_called);
    CHECK(rc == VSL_EXEC_OK && r.exit_code == 0,
          "interpreter runs and exits(0); main entry was not reached");
    CHECK(r.output_len == 58 && r.output[0] == 'I' && r.output[57] == 'F',
          "DT_INIT marker precedes output, DT_FINI marker follows on unload");
    CHECK(r.init_called == 1 && r.fini_called == 1,
          "DT_INIT/DT_FINI invoked through the existing init/fini owner");
    CHECK(r.interp_loaded == 1 && r.interp_resolved == 1,
          "interpreter mapped from the bounded guest rootfs");
    CHECK(r.auxv_entries == 8, "auxv has seven entries plus AT_NULL");
    if (r.output_len == 58) {
        uint64_t v[7];
        for (int i = 0; i < 7; i++)
            memcpy(&v[i], r.output + 1 + i * 8, 8);
        printf("[gate w] auxv PHDR=0x%llx PHENT=%llu PHNUM=%llu "
               "BASE=0x%llx ENTRY=0x%llx PAGESZ=%llu RANDOM=0x%llx\n",
               (unsigned long long)v[0], (unsigned long long)v[1],
               (unsigned long long)v[2], (unsigned long long)v[3],
               (unsigned long long)v[4], (unsigned long long)v[5],
               (unsigned long long)v[6]);
        CHECK(v[0] == r.load_bias + 64,
              "AT_PHDR is main e_phoff mapped at the chosen load bias");
        CHECK(v[1] == 56 && v[2] == 3,
              "AT_PHENT/AT_PHNUM describe the real main program headers");
        CHECK(v[3] == r.interp_bias && v[3] != 0,
              "AT_BASE is the interpreter's own non-zero load bias");
        CHECK(v[4] == r.load_bias + 0x400,
              "AT_ENTRY is the real main entry the ld.so would jump to");
        CHECK(v[5] == 4096, "AT_PAGESZ is 4096");
        CHECK(v[6] != 0, "AT_RANDOM points at stack bytes");
    }

    /* Fail-closed controls: no rootfs, escaping path, malformed path. */
    {
        VslImageCfg no_root = { none, 0, 0, NULL, 0, NULL };
        rc = vsl_exec_image(img, n, &no_root, &r);
        printf("[gate w] no rootfs: rc=%d reason=\"%s\"\n", rc, r.interp_reason);
        CHECK(rc == VSL_EXEC_ERR_INTERP, "PT_INTERP refused with no rootfs");
    }
    {
        uint8_t bad[0x800];
        memcpy(bad, img, n);
        memcpy(bad + 0x600, "/../../etc/passwd", 17);
        rc = vsl_exec_image(bad, n, &cfg, &r);
        CHECK(rc == VSL_EXEC_ERR_INTERP,
              "dot-escaping PT_INTERP path fails closed");
        memcpy(bad, img, n);
        uint64_t z = 0;
        memcpy(bad + 152, &z, 8);                  /* PT_INTERP p_filesz = 0 */
        rc = vsl_exec_image(bad, n, &cfg, &r);
        CHECK(rc == VSL_EXEC_ERR_INTERP,
              "malformed PT_INTERP size fails closed");
    }
    vsl_dyn_set_interp(0);
    vsl_dyn_set_init_fini(0);
}

/* (x) arch_prctl(ARCH_SET_FS) + TCB + real module-id TLS layout.  A
 *     freestanding guest installs its own %fs base with the real syscall,
 *     then reads the engine-seeded TCB (self ptr @%fs:0, dtv @%fs:8) and the
 *     dtv's module-id table.  fs_tls OFF is the negative control. */
{
    snprintf(path, sizeof(path), "%s/guest_fs_tcb.elf", dir);
    vsl_dyn_set_fs_tls(0);
    rc = vsl_exec_elf(path, &r);
    printf("[gate x] fs_tls off: rc=%d exit=%d out_len=%zu fs_syscalls=%u fs_base=0x%llx\n",
           rc, r.exit_code, r.output_len, r.fs_syscalls,
           (unsigned long long)r.fs_base);
    CHECK(rc == VSL_EXEC_OK && r.exit_code == 0, "guest runs with fs_tls off");
    CHECK(r.fs_syscalls == 0 && r.fs_base == 0,
          "fs_tls off: arch_prctl not handled, no FS base recorded");
    if (r.output_len >= 8) {
        int64_t off_rc = 0;
        memcpy(&off_rc, r.output, 8);
        CHECK(off_rc != 0,
              "negative control: ARCH_SET_FS unhandled (no TCB installed)");
    }

    vsl_dyn_set_fs_tls(1);
    rc = vsl_exec_elf(path, &r);
    printf("[gate x] fs_tls on: rc=%d exit=%d out_len=%zu fs_syscalls=%u "
           "fs_base=0x%llx tcb=0x%llx modules=%u\n",
           rc, r.exit_code, r.output_len, r.fs_syscalls,
           (unsigned long long)r.fs_base, (unsigned long long)r.tcb_base,
           r.tls_modules);
    CHECK(rc == VSL_EXEC_OK && r.exit_code == 0, "guest exits 0 with fs_tls on");
    CHECK(r.fs_syscalls == 2, "SET + GET arch_prctl calls trapped+handled");
    CHECK(r.fs_base != 0, "guest FS base recorded in the result");
    CHECK(r.tls_modules == 1, "main image registered as TLS module 1");
    CHECK(r.tcb_base != 0, "host TCB backing store exposed for the gate");
    if (r.output_len >= 56) {
        int64_t set_rc = 0;
        uint64_t self = 0, dtv = 0, got = 0, tcb_addr = 0, cnt = 0, mod1 = 0;
        memcpy(&set_rc, r.output, 8);
        memcpy(&self, r.output + 8, 8);
        memcpy(&dtv, r.output + 16, 8);
        memcpy(&got, r.output + 24, 8);
        memcpy(&tcb_addr, r.output + 32, 8);
        memcpy(&cnt, r.output + 40, 8);
        memcpy(&mod1, r.output + 48, 8);
        printf("[gate x] set_rc=%lld self=0x%llx dtv=0x%llx got=0x%llx "
               "tcb=0x%llx cnt=%llu mod1=0x%llx\n",
               (long long)set_rc, (unsigned long long)self,
               (unsigned long long)dtv, (unsigned long long)got,
               (unsigned long long)tcb_addr, (unsigned long long)cnt,
               (unsigned long long)mod1);
        CHECK(set_rc == 0, "ARCH_SET_FS succeeded inside the guest");
        CHECK(self == tcb_addr && self == r.fs_base,
              "%fs:0 reads the self pointer at the guest-chosen FS base");
        CHECK(dtv != 0, "%fs:8 dtv pointer seeded by the engine");
        CHECK(got == r.fs_base, "ARCH_GET_FS returns the recorded guest base");
        CHECK(r.fs_base == tcb_addr, "result FS base equals the guest TCB address");
        CHECK(cnt == 1, "dtv module count reflects the real module layout");
        CHECK(mod1 == vsl_tls_base(), "dtv[1] is the real module-1 TLS block base");
    }
    CHECK(getpid() > 0, "host libc still works after a guest switched %fs");
    vsl_dyn_set_fs_tls(0);
}

/* (z) Ordered rootfs search list.  The same PT_INTERP fixture is placed in
 *     a SECOND rootfs dir only; an ordered list searches an empty dir first
 *     and must walk both before finding it.  Reversed order and a list with
 *     no interpreter are fail-closed/one-hit controls.  The process-wide
 *     vsl_dyn_add_rootfs() list is exercised too. */
{
    uint8_t img[0x800];
    size_t n = build_interp_main_elf(img);
    const char *none[] = { NULL };
    char dirA[512], rootfs2[512];
    snprintf(dirA, sizeof(dirA), "%s/dirA", dir);        /* no libvslinterp.so */
    snprintf(rootfs2, sizeof(rootfs2), "%s/glibc_rootfs2", dir);
    const char *ordered[]   = { dirA, rootfs2, NULL };
    const char *onlyA[]     = { dirA, NULL };
    const char *reversed[]  = { rootfs2, dirA, NULL };
    VslImageCfg cfgZ   = { none, 0, 0, NULL, 0, NULL, ordered };
    VslImageCfg cfgRev = { none, 0, 0, NULL, 0, NULL, reversed };
    VslImageCfg cfgNo  = { none, 0, 0, NULL, 0, NULL, onlyA };
    VslImageCfg cfgP   = { none, 0, 0, NULL, 0, NULL, NULL };

    vsl_dyn_set_interp(1);
    vsl_dyn_set_init_fini(1);
    rc = vsl_exec_image(img, n, &cfgZ, &r);
    printf("[gate z] ordered(dirA,rootfs2): rc=%d interp=%u tried=%u path=\"%s\"\n",
           rc, r.interp_loaded, r.rootfs_tried, r.interp_path);
    CHECK(rc == VSL_EXEC_OK && r.interp_loaded == 1,
          "interpreter found via the ordered rootfs list");
    CHECK(r.rootfs_tried == 2, "empty first dir searched, hit on second dir");
    CHECK(strcmp(r.interp_path, "/libvslinterp.so") == 0,
          "bounded interp path intact after the run (auxv count width)");

    rc = vsl_exec_image(img, n, &cfgRev, &r);
    printf("[gate z] reversed(rootfs2,dirA): rc=%d tried=%u\n",
           rc, r.rootfs_tried);
    CHECK(rc == VSL_EXEC_OK && r.rootfs_tried == 1,
          "reversed order hits on the first dir only");

    rc = vsl_exec_image(img, n, &cfgNo, &r);
    printf("[gate z] no-interp list: rc=%d tried=%u reason=\"%s\"\n",
           rc, r.rootfs_tried, r.interp_reason);
    CHECK(rc == VSL_EXEC_ERR_INTERP && r.rootfs_tried == 1,
          "list with no interpreter fails closed after one search");

    vsl_dyn_clear_rootfs_list();
    CHECK(vsl_dyn_add_rootfs(dirA) == 0 && vsl_dyn_add_rootfs(rootfs2) == 0,
          "process-wide ordered rootfs list accepts two dirs");
    rc = vsl_exec_image(img, n, &cfgP, &r);
    printf("[gate z] process-wide list: rc=%d interp=%u tried=%u\n",
           rc, r.interp_loaded, r.rootfs_tried);
    CHECK(rc == VSL_EXEC_OK && r.interp_loaded == 1 && r.rootfs_tried == 2,
          "process-wide ordered list searched in call order");
    vsl_dyn_clear_rootfs_list();
    vsl_dyn_set_interp(0);
    vsl_dyn_set_init_fini(0);
}

/* (y) DT_VERSYM preference for the interpreter's own symbols.  The
 *     hand-built interpreter has a versioned self-definition AND a versioned
 *     self-import whose DT_VERNEED names a different file; it can only be
 *     relocated if its OWN exported version is preferred.  A copy with a
 *     mismatched need-version is the fail-closed control. */
{
    uint8_t interp[0x900];
    size_t ni = build_selfsym_interp_elf(interp);
    CHECK(ni == 0x900, "selfsym interpreter fixture assembled");
    uint8_t main[0x800];
    size_t nm = build_interp_main_elf(main);
    CHECK(nm == 0x800, "interp-main fixture assembled (selfsym gate)");

    char root[512], ipath[512], bpath[512], mpath[512], bmain[512];
    snprintf(root, sizeof(root), "%s/glibc_rootfs", dir);
    snprintf(ipath, sizeof(ipath), "%s/libvslself.so", root);
    snprintf(bpath, sizeof(bpath), "%s/libvslself2.so", root);
    snprintf(mpath, sizeof(mpath), "%s/libvslself.so", dir);
    snprintf(bmain, sizeof(bmain), "%s/libvslself2.so", dir);

    /* Write both the good and the version-mismatched interpreter images. */
    uint8_t bad[0x900];
    memcpy(bad, interp, ni);
    bad[SELF_NEED_VER_FILE] = 'X';       /* need "XSL_SELF", def "VSL_SELF" */
    int wok = 1;
    FILE *fp = fopen(ipath, "wb");
    if (fp) { wok &= fwrite(interp, 1, ni, fp) == ni; fclose(fp); } else wok = 0;
    fp = fopen(bpath, "wb");
    if (fp) { wok &= fwrite(bad, 1, ni, fp) == ni; fclose(fp); } else wok = 0;
    CHECK(wok, "selfsym interpreter fixtures written to the guest rootfs");

    const char *none[] = { NULL };
    VslImageCfg cfg = { none, 0, 0, NULL, 0, root, NULL };
    vsl_dyn_set_interp(1);
    vsl_dyn_set_init_fini(0);

    /* Good interpreter: versioned self-import resolves to its OWN def. */
    memset(main + 0x600, 0, 17);
    memcpy(main + 0x600, "/libvslself.so", 15);
    fp = fopen(mpath, "wb");
    if (fp) { fwrite(main, 1, nm, fp); fclose(fp); }
    {
        size_t mn = 0;
        uint8_t *mb = read_test_file(mpath, &mn);
        CHECK(mb != NULL, "selfsym main fixture readable");
        rc = vsl_exec_image(mb, mn, &cfg, &r);
        free(mb);
    }
    printf("[gate y] selfsym interp: rc=%d exit=%d interp=%u out_len=%zu "
           "magic=0x%llx\n", rc, r.exit_code, r.interp_loaded, r.output_len,
           r.output_len >= 8 ? (unsigned long long)*(uint64_t *)r.output : 0ULL);
    CHECK(rc == VSL_EXEC_OK && r.exit_code == 0 && r.interp_loaded == 1,
          "interpreter self-relocated its versioned import");
    CHECK(r.output_len == 8, "interpreter emitted the 8-byte self-symbol magic");
    if (r.output_len >= 8) {
        uint64_t magic = 0;
        memcpy(&magic, r.output, 8);
        CHECK(magic == SELF_MAGIC,
              "versioned self-import bound to the interpreter's OWN definition");
    }

    /* Bad need-version: DT_VERNEED no longer matches the self definition. */
    memset(main + 0x600, 0, 17);
    memcpy(main + 0x600, "/libvslself2.so", 16);
    fp = fopen(bmain, "wb");
    if (fp) { fwrite(main, 1, nm, fp); fclose(fp); }
    {
        size_t bn = 0;
        uint8_t *bb = read_test_file(bmain, &bn);
        CHECK(bb != NULL, "bad-version main fixture readable");
        rc = vsl_exec_image(bb, bn, &cfg, &r);
        free(bb);
    }
    printf("[gate y] bad need-version: rc=%d status=%d reason=\"%s\"\n",
           rc, r.status, r.interp_reason);
    CHECK(rc == VSL_EXEC_ERR_INTERP,
          "version-mismatched self-import fails closed (ERR_INTERP)");
    vsl_dyn_set_interp(0);
}

/* (aa) Bounded guest libc shim: bind the main image's VERSIONED libc imports
 *      in the real-ld.so path and run a genuine dynamically linked hello-
 *      world end-to-end.  `guest_hello.elf` is host-linked (PT_INTERP
 *      /lib64/ld-linux-x86-64.so.2, DT_NEEDED libc.so.6, JUMP_SLOT
 *      puts@GLIBC_2.2.5 + GLOB_DAT __libc_start_main@GLIBC_2.34).  The real
 *      ld.so is still mapped and self-relocated from the guest rootfs, but
 *      with bind_main ON VSL owns main relocation and binds those imports
 *      from a bounded GUEST libc shim exporting the matching DT_VERDEF names.
 *  DIFFERENTIAL / NEGATIVE CONTROLS:
 *   - old rule (bind OFF): ld.so owns main -> guest ld.so executes then
 *     faults; no hello output;
 *   - no shim dir: DT_NEEDED libc.so.6 fails closed with the exact soname;
 *   - version-mismatched shim (puts@VSL_BAD_9): main relocation fails closed
 *     with the exact unresolved symbol name 'puts' and its GOT offset. */
{
    static const char hello[] = "VSL-HELLO-DYNAMIC-WORLD";
    size_t hn = 0;
    snprintf(path, sizeof(path), "%s/guest_hello.elf", dir);
    uint8_t *himg = read_test_file(path, &hn);
    CHECK(himg != NULL, "real dynamic hello-world fixture readable");
    if (himg) {
        char root[512], shimdir[512], badshimdir[512];
        snprintf(root, sizeof(root), "%s/glibc_rootfs", dir);
        snprintf(shimdir, sizeof(shimdir), "%s/glibc_rootfs", dir);
        snprintf(badshimdir, sizeof(badshimdir), "%s/glibc_rootfs_badver", dir);
        const char *none[] = { NULL };
        const char *shim_dirs[] = { shimdir, NULL };
        const char *bad_dirs[] = { badshimdir, NULL };
        VslImageCfg cfg_none = { none,      0, 0, NULL, 0, root, NULL };
        VslImageCfg cfg_shim = { shim_dirs, 0, 0, NULL, 0, root, NULL };
        VslImageCfg cfg_bad  = { bad_dirs,  0, 0, NULL, 0, root, NULL };

        /* NEGATIVE A: pre-change ld.so-owned main relocation. */
        vsl_dyn_set_interp(1);
        vsl_dyn_set_interp_bind_main(0);
        vsl_dyn_set_init_fini(0);
        int rc_old = vsl_exec_image(himg, hn, &cfg_none, &r);
        printf("[gate aa] old ld.so-owned main: rc=%d status=%d interp=%u "
               "fault=0x%llx syscalls=%llu out_len=%zu\n",
               rc_old, r.status, r.interp_loaded,
               (unsigned long long)r.fault_addr,
               (unsigned long long)r.syscalls_trapped, r.output_len);
        CHECK(r.interp_loaded == 1,
              "old rule still maps the real ld.so from the guest rootfs");
        CHECK(r.output_len < sizeof(hello) ||
              memcmp(r.output, hello, sizeof(hello) - 1) != 0,
              "old rule never reaches the guest hello-world main");

        /* NEGATIVE B: version-mismatched shim -> exact unresolved symbol. */
        vsl_dyn_set_interp_bind_main(1);
        int rc_bad = vsl_exec_image(himg, hn, &cfg_bad, &r);
        printf("[gate aa] bad-version shim: rc=%d status=%d "
               "reloc_failure=\"%s\"\n",
               rc_bad, r.status, r.reloc_failure);
        CHECK(rc_bad == VSL_EXEC_ERR_RELOC,
              "wrong-version shim fails closed with ERR_RELOC");
        CHECK(strstr(r.reloc_failure, "puts") != NULL,
              "exact unresolved symbol 'puts' recorded for the negative");

        /* NEGATIVE C: no bounded dep dir -> DT_NEEDED fails closed. */
        int rc_noshim = vsl_exec_image(himg, hn, &cfg_none, &r);
        printf("[gate aa] no shim dir: rc=%d status=%d reloc_failure=\"%s\"\n",
               rc_noshim, r.status, r.reloc_failure);
        CHECK(rc_noshim == VSL_EXEC_ERR_RELOC,
              "absent libc shim fails closed with ERR_RELOC");
        CHECK(strstr(r.reloc_failure, "libc.so.6") != NULL,
              "exact missing provider soname recorded");

        /* NEW: bind main imports from the good bounded guest libc shim. */
        int rc_new = vsl_exec_image(himg, hn, &cfg_shim, &r);
        printf("[gate aa] shim-bound: rc=%d exit=%d interp=%u deps=%u "
               "relocs=%u syms=%u reason=\"%s\" reloc_failure=\"%s\" "
               "out_len=%zu out=\"%.*s\"\n",
               rc_new, r.exit_code, r.interp_loaded, r.deps_loaded,
               r.relocations_applied, r.symbols_resolved,
               r.interp_reason, r.reloc_failure, r.output_len,
               (int)(r.output_len < 80 ? r.output_len : 80), r.output);
        CHECK(rc_new == VSL_EXEC_OK && r.exit_code == 0,
              "real dynamic hello-world runs end-to-end to exit(0)");
        CHECK(r.interp_loaded == 1,
              "real ld.so still mapped + self-relocated (ld.so path intact)");
        CHECK(r.deps_loaded >= 1,
              "bounded guest libc shim loaded through DT_NEEDED");
        CHECK(r.symbols_resolved >= 3,
              "puts/__libc_start_main/__cxa_finalize bound from guest shim");
        CHECK(r.output_len == sizeof(hello) &&
              memcmp(r.output, hello, sizeof(hello) - 1) == 0 &&
              r.output[sizeof(hello) - 1] == '\n',
              "exact hello-world bytes written by GUEST puts through VSL");
        CHECK(rc_old != rc_new,
              "differential: main-import binding changes the outcome");
        vsl_dyn_set_interp_bind_main(0);
        vsl_dyn_set_interp(0);
        free(himg);
    }
}

/* (ab) Bounded R_X86_64_COPY of a REAL libc data symbol from a provider
 *      resolved inside the guest rootfs.  `guest_copy.elf` is host-linked
 *      (PT_INTERP ld-linux, DT_NEEDED libc.so.6) and emits a genuine
 *      `COPY optind@GLIBC_2.2.5` of 4 bytes into its main image.  With the
 *      opt-in rootfs provider resolution ON, VSL finds libc.so.6 under
 *      <rootfs>/lib64, copies the provider's versioned `optind@@GLIBC_2.2.5`
 *      bytes (sentinel 0x5a) into the main image and runs to exit(0x5a).
 * NEG A: resolution OFF -> DT_NEEDED libc.so.6 fails closed.
 * NEG B: st_size grown past the provider's size -> bounded COPY rejects.
 * DIFFERENTIAL: the pre-change host-only rule rejected every versioned COPY. */
{
    uint8_t *cimg;
    size_t cn = 0;
    uint64_t szoff = 0;
    uint16_t versym = 0;
    snprintf(path, sizeof(path), "%s/guest_copy.elf", dir);
    cimg = read_test_file(path, &cn);
    CHECK(cimg != NULL, "real libc-COPY fixture readable");
    if (cimg) {
        char root[512];
        snprintf(root, sizeof(root), "%s/glibc_rootfs", dir);
        const char *nodir[] = { NULL };
        VslImageCfg cfg = { nodir, 0, 0, NULL, 0, root, NULL };

        CHECK(copy_dynsym_info(cimg, cn, "optind", &szoff, &versym) == 1,
              "real fixture's COPY destination dynsym 'optind' located");
        printf("[ab] COPY dynsym versym=0x%x size_off=0x%llx\n",
               (unsigned)versym, (unsigned long long)szoff);
        CHECK((versym & 0x7fff) > 1,
              "COPY is versioned: pre-change host-only rule refused it");

        /* NEW: rootfs provider resolution finds libc.so.6 under lib64. */
        vsl_dyn_set_interp(1);
        vsl_dyn_set_interp_bind_main(1);
        vsl_dyn_set_init_fini(0);
        vsl_dyn_set_libc_rootfs(1);
        int rc = vsl_exec_image(cimg, cn, &cfg, &r);
        printf("[ab] rootfs COPY: rc=%d exit=%d interp=%u deps=%u "
               "relocs=%u syms=%u reason=\"%s\" reloc_failure=\"%s\" "
               "out_len=%zu out0=0x%02x\n",
               rc, r.exit_code, r.interp_loaded, r.deps_loaded,
               r.relocations_applied, r.symbols_resolved,
               r.interp_reason, r.reloc_failure, r.output_len,
               r.output_len ? (unsigned char)r.output[0] : 0);
        CHECK(rc == VSL_EXEC_OK && r.exit_code == 0x5a,
              "real libc data symbol copied into main image (exit=0x5a)");
        CHECK(r.interp_loaded == 1 && r.deps_loaded >= 1,
              "real ld.so mapped and libc.so.6 provider loaded from rootfs");
        CHECK(r.output_len == 1 && (unsigned char)r.output[0] == 0x5a,
              "guest wrote the exact copied sentinel byte through VSL");
        CHECK(r.symbols_resolved >= 1,
              "versioned GLOB_DAT bound alongside the COPY");

        /* NEG A: resolution OFF -> exact missing-provider diagnostic. */
        vsl_dyn_set_libc_rootfs(0);
        int rc_off = vsl_exec_image(cimg, cn, &cfg, &r);
        printf("[ab] no rootfs provider: rc=%d status=%d reloc_failure=\"%s\"\n",
               rc_off, r.status, r.reloc_failure);
        CHECK(rc_off == VSL_EXEC_ERR_RELOC &&
              strstr(r.reloc_failure, "libc.so.6") != NULL,
              "without rootfs resolution the provider fails closed (libc.so.6)");

        /* NEG B: grow the COPY st_size past the provider's 4-byte object. */
        vsl_dyn_set_libc_rootfs(1);
        if (szoff && szoff + 8 <= cn) {
            size_t mn = 0;
            uint8_t *mut = read_test_file(path, &mn);
            if (mut) {
                uint64_t bigsz = 8;
                memcpy(mut + szoff, &bigsz, 8);
                int rc_big = vsl_exec_image(mut, mn, &cfg, &r);
                printf("[ab] oversized COPY: rc=%d reloc_failure=\"%s\"\n",
                       rc_big, r.reloc_failure);
                CHECK(rc_big == VSL_EXEC_ERR_RELOC &&
                      strstr(r.reloc_failure, "source bound") != NULL,
                      "oversized COPY rejected by the provider source bound");
                free(mut);
            }
        }
        /* NEG C: a REAL glibc libc.so.6 provider is refused, but now at its
         * NEXT exact boundary, not at any ld.so import.  All 17 remaining
         * versioned ld.so imports (_dl_argv, __libc_enable_secure,
         * _dl_allocate_tls, __tunable_get_val, _rtld_global, ...) bind from
         * the bounded host provider; relocation advances to the merged-export
         * cap (512), which glibc's ~3000 defined dynsyms overflow.  realroot
         * is searched FIRST for the provider but has no ld.so, so the
         * interpreter still resolves from the good rootfs. */
        char realroot[512];
        snprintf(realroot, sizeof(realroot), "%s/glibc_rootfs_reallibc", dir);
        const char *real_first[] = { realroot, root, NULL };
        VslImageCfg cfg_real = { nodir, 0, 0, NULL, 0, NULL, real_first };
        vsl_dyn_set_libc_rootfs(1);
        int rc_real = vsl_exec_image(cimg, cn, &cfg_real, &r);
        printf("[ab] real glibc libc.so.6 provider: rc=%d status=%d "
               "reloc_failure=\"%s\"\n",
               rc_real, r.status, r.reloc_failure);
        CHECK(rc_real == VSL_EXEC_ERR_RELOC &&
              strstr(r.reloc_failure, "merged export table cap") != NULL &&
              strstr(r.reloc_failure, "libc.so.6") != NULL,
              "real libc binds ALL ld.so imports, then hits the export-cap boundary");
        CHECK(strstr(r.reloc_failure, "symbol '__libc_enable_secure'") == NULL &&
              strstr(r.reloc_failure, "symbol '_dl_argv'") == NULL,
              "no ld.so import is the boundary any more (all 17 bound)");
        CHECK(strstr(r.reloc_failure, "TLS relocation type 18 symbol 0") == NULL,
              "symbol-less TPOFF64 is no longer the blocking boundary");

        vsl_dyn_set_libc_rootfs(0);
        vsl_dyn_set_interp_bind_main(0);
        vsl_dyn_set_interp(0);
        free(cimg);
    }
}

/* (ac) Raised version-table bounds + REAL-sized provider binding.
 *  The old 32/32 VERDEF/VERNEED caps refused a real glibc libc.so.6 (41
 *  version definitions).  This leg raises them with explicit resource limits
 *  and proves the raised tables still bind COPY/GLOB_DAT/JUMP_SLOT by exact
 *  DT_VERNEED name + vn_file:
 *   - a repo-built minimal libc.so.6 with DT_VERDEFNUM=41 (same shape as the
 *     host's real libc) binds hello-world's JUMP_SLOT puts@GLIBC_2.2.5 and
 *     GLOB_DAT __libc_start_main@GLIBC_2.34, and guest_copy's COPY
 *     optind@GLIBC_2.2.5 -- all above the old cap;
 *   - the REAL host libc.so.6 provider is then attempted for the SAME hello
 *     world.  It parses past the raised caps, applies the symbol-less TPOFF64
 *     relocations against the defined module TLS base, binds ALL 17 remaining
 *     versioned ld.so imports (_dl_argv, __libc_enable_secure,
 *     _dl_allocate_tls, __tunable_get_val, _rtld_global, ...) from the bounded
 *     host provider by exact vn_file, and fails at the NEXT exact boundary --
 *     the 512-entry merged-export cap, which glibc's ~3000 defined dynsyms
 *     overflow -- recorded as a verified negative rather than a silent skip.
 *     Retargeting ONLY the fixture's DT_VERNEED vn_file from
 *     ld-linux-x86-64.so.2 to libc.so.6 restores the _dl_argv fault end-to-end
 *     (DT_NEEDED sonames are left byte-identical). */
{
    static const char hello[] = "VSL-HELLO-DYNAMIC-WORLD";
    size_t bn = 0, cn2 = 0;
    char root[512], bigdir[512], realroot[512];
    snprintf(root,     sizeof(root),     "%s/glibc_rootfs", dir);
    snprintf(bigdir,   sizeof(bigdir),   "%s/glibc_rootfs_bigver", dir);
    snprintf(realroot, sizeof(realroot), "%s/glibc_rootfs_reallibc", dir);
    const char *none[]      = { NULL };
    const char *big_dirs[]  = { bigdir, NULL };
    const char *real_first[] = { realroot, root, NULL };

    snprintf(path, sizeof(path), "%s/guest_hello.elf", dir);
    uint8_t *hbig = read_test_file(path, &bn);
    CHECK(hbig != NULL, "bigver gate: hello fixture readable");
    if (hbig) {
        VslImageCfg cfg_big = { big_dirs, 0, 0, NULL, 0, root, NULL };
        vsl_dyn_set_interp(1);
        vsl_dyn_set_interp_bind_main(1);
        vsl_dyn_set_init_fini(0);
        vsl_dyn_set_libc_rootfs(0);

        int rc_big = vsl_exec_image(hbig, bn, &cfg_big, &r);
        printf("[ac] 41-verdef hello: rc=%d exit=%d deps=%u relocs=%u syms=%u "
               "reason=\"%s\" reloc_failure=\"%s\" out_len=%zu out=\"%.*s\"\n",
               rc_big, r.exit_code, r.deps_loaded, r.relocations_applied,
               r.symbols_resolved, r.interp_reason, r.reloc_failure,
               r.output_len,
               (int)(r.output_len < 64 ? r.output_len : 64), r.output);
        CHECK(rc_big == VSL_EXEC_OK && r.exit_code == 0,
              "41-verdef provider binds hello-world end-to-end (exit=0)");
        CHECK(r.deps_loaded >= 1 && r.symbols_resolved >= 3,
              "GLOB_DAT + JUMP_SLOT bound from 41-verdef provider by vn_file");
        CHECK(r.output_len == sizeof(hello) &&
              memcmp(r.output, hello, sizeof(hello) - 1) == 0 &&
              r.output[sizeof(hello) - 1] == '\n',
              "exact hello bytes written by GUEST puts through VSL");

        /* COPY from the same >32-cap provider. */
        snprintf(path, sizeof(path), "%s/guest_copy.elf", dir);
        uint8_t *cbig = read_test_file(path, &cn2);
        CHECK(cbig != NULL, "bigver gate: COPY fixture readable");
        if (cbig) {
            int rc_copy = vsl_exec_image(cbig, cn2, &cfg_big, &r);
            printf("[ac] 41-verdef COPY: rc=%d exit=%d reloc_failure=\"%s\"\n",
                   rc_copy, r.exit_code, r.reloc_failure);
            CHECK(rc_copy == VSL_EXEC_OK && r.exit_code == 0x5a,
                  "COPY optind@GLIBC_2.2.5 bound from 41-verdef provider");
            free(cbig);
        }

        /* REAL glibc libc.so.6: exact NEXT failure, verified negative.
         * Every versioned ld.so import now binds from the bounded host table;
         * the provider advances past all of them and fails at the 512-entry
         * merged-export cap (glibc has ~3000 defined dynsyms). */
        vsl_dyn_set_libc_rootfs(1);
        VslImageCfg cfg_real = { none, 0, 0, NULL, 0, NULL, real_first };
        int rc_real = vsl_exec_image(hbig, bn, &cfg_real, &r);
        printf("[ac] real libc hello: rc=%d status=%d reloc_failure=\"%s\"\n",
               rc_real, r.status, r.reloc_failure);
        CHECK(rc_real == VSL_EXEC_ERR_RELOC,
              "real glibc libc.so.6 provider still fails closed with ERR_RELOC");
        CHECK(strstr(r.reloc_failure, "merged export table cap") != NULL &&
              strstr(r.reloc_failure, "libc.so.6") != NULL,
              "exact next failure is the bounded merged-export cap, not an ld.so import");
        CHECK(strstr(r.reloc_failure, "symbol '__libc_enable_secure'") == NULL &&
              strstr(r.reloc_failure, "symbol '_dl_argv'") == NULL,
              "no versioned ld.so import blocks the fixture any longer");
        CHECK(strstr(r.reloc_failure, "TLS relocation type 18 symbol 0") == NULL,
              "symbol-less TPOFF64 no longer blocks the real glibc provider");
        CHECK(strstr(r.reloc_failure, "32/32") == NULL,
              "real libc parsed PAST the old 32/32 version-table cap");

        /* END-TO-END DIFFERENTIAL (exact vn_file): retarget ONLY the DT_VERNEED
         * vn_file of a copy of the real libc.so.6 from ld-linux-x86-64.so.2 to
         * libc.so.6.  patch_verneed_file rewrites just the 4-byte vn_file
         * pointer -- DT_NEEDED sonames stay byte-identical.  The bounded host
         * provider must then refuse _dl_argv@GLIBC_PRIVATE and the fault must
         * return to that exact boundary. */
        snprintf(path, sizeof(path),
                 "%s/glibc_rootfs_reallibc/lib64/libc.so.6", dir);
        size_t badsz = 0;
        uint8_t *badlibc = read_test_file(path, &badsz);
        CHECK(badlibc != NULL, "vn_file differential: real libc.so.6 readable");
        if (badlibc) {
            CHECK(patch_verneed_file(badlibc, badsz,
                                     "ld-linux-x86-64.so.2", "libc.so.6") == 1,
                  "vn_file differential: only DT_VERNEED vn_file retargeted");
            char tmpl[] = "/tmp/vsl-dlavg-XXXXXX";
            char *tmpdir = mkdtemp(tmpl);
            CHECK(tmpdir != NULL, "vn_file differential: temp rootfs created");
            if (tmpdir) {
                snprintf(path, sizeof(path), "%s/libc.so.6", tmpdir);
                FILE *fp = fopen(path, "wb");
                CHECK(fp != NULL, "vn_file differential: patched provider opened");
                if (fp) {
                    size_t wrote = fwrite(badlibc, 1, badsz, fp);
                    fclose(fp);
                    CHECK(wrote == badsz,
                          "vn_file differential: full patched provider written");
                    const char *bad_first[] = { tmpdir, root, NULL };
                    VslImageCfg cfg_bad = { none, 0, 0, NULL, 0, NULL, bad_first };
                    vsl_dyn_set_libc_rootfs(1);
                    int rc_bad = vsl_exec_image(hbig, bn, &cfg_bad, &r);
                    printf("[ac] wrong-vn_file libc: rc=%d reloc_failure=\"%s\"\n",
                           rc_bad, r.reloc_failure);
                    CHECK(rc_bad == VSL_EXEC_ERR_RELOC &&
                          strstr(r.reloc_failure, "symbol '_dl_argv'") != NULL &&
                          strstr(r.reloc_failure, "is unresolved") != NULL,
                          "wrong vn_file restores the exact _dl_argv failure");
                    vsl_dyn_set_libc_rootfs(0);
                    unlink(path);
                    rmdir(tmpdir);
                }
            }
            free(badlibc);
        }
        vsl_dyn_set_libc_rootfs(0);
        vsl_dyn_set_interp_bind_main(0);
        vsl_dyn_set_interp(0);
        free(hbig);
    }
}

/* (h) host syscalls still real after all runs (filter ALLOW path) */
    CHECK(getpid() > 0, "host getpid still works after engine runs");

    printf("Results: %d passed, %d failed\n", passed, failed);
    return failed ? 1 : 0;
}
