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
        rc = vsl_exec_elf_bytes(img, n, &r);
        CHECK(rc == VSL_EXEC_ERR_FORMAT, "ET_DYN rejected (static ET_EXEC only)");
        n = build_tiny_elf(img, 0x400000ULL /* below VSL window */, 2, code, sizeof(code));
        rc = vsl_exec_elf_bytes(img, n, &r);
        CHECK(rc == VSL_EXEC_ERR_WINDOW, "out-of-window link address rejected");
        rc = vsl_exec_elf("/nonexistent/vsl-guest.elf", &r);
        CHECK(rc == VSL_EXEC_ERR_ARGS, "missing file rejected cleanly");
    }

    /* (h) host syscalls still real after all runs (filter ALLOW path) */
    CHECK(getpid() > 0, "host getpid still works after engine runs");

    printf("Results: %d passed, %d failed\n", passed, failed);
    return failed ? 1 : 0;
}
