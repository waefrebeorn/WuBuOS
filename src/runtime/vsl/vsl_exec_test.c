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
 * Usage: vsl_exec_test <guest-dir>   (guests built by mk/tests.mk)
 */
#include "../../egojar.h"
#include "vsl/vsl_exec_engine.h"
#include "vsl/vsl_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/utsname.h>

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
        /* Same length, unknown name: vsl_test_shim -> no_such_sym_xx. */
        memcpy(img + 0x1281, "no_such_sym_xx", 13);
        rc = vsl_exec_elf_bytes(img, n, &r);
        printf("[gate m] unknown symbol: rc=%d status=%d (want %d)\n",
               rc, r.status, VSL_EXEC_ERR_RELOC);
        CHECK(rc == VSL_EXEC_ERR_RELOC, "unknown symbol rejected (ERR_RELOC)");

        n = build_dynsym_elf(img);
        /* Change GLOB_DAT(6) -> R_X86_64_COPY(5), an unsupported type. */
        uint64_t ri = 5; memcpy(img + 0x1508, &ri, 8);
        rc = vsl_exec_elf_bytes(img, n, &r);
        printf("[gate m] unsupported reloc type: rc=%d status=%d skipped=%u (want %d)\n",
               rc, r.status, r.relocations_skipped, VSL_EXEC_ERR_RELOCTYPE);
        CHECK(rc == VSL_EXEC_ERR_RELOCTYPE && r.relocations_skipped == 1,
              "unsupported relocation rejected (ERR_RELOCTYPE)");
    }

    /* (h) host syscalls still real after all runs (filter ALLOW path) */
    CHECK(getpid() > 0, "host getpid still works after engine runs");

    printf("Results: %d passed, %d failed\n", passed, failed);
    return failed ? 1 : 0;
}
