/*
 * test_cross_exec.c — Cross-architecture execution test.
 *
 * Compiles the same C source on x86-64, ARM64, and RISC-V backends.
 * x86-64 executes directly; ARM64/RISC-V via qemu-user-static on
 * minimal ELF wrappers we build with build_elf.py.
 */
#include "jit_codegen.h"
#include "jit.h"
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>

int jit_minic_compile_cg(CodeGen *cg, const char *src);
const uint8_t *jit_minic_get_code(CodeGen *cg, size_t *size);

extern CodeGen *cg_create_rv64(void);

static int64_t exec_x86(const uint8_t *code, size_t sz, int64_t arg0, int64_t arg1) {
    void *mem = mmap(NULL, sz + 4096, PROT_READ | PROT_WRITE | PROT_EXEC,
                     MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (mem == MAP_FAILED) return -999;
    memcpy(mem, code, sz);
    int64_t (*f)(int64_t, int64_t) = (int64_t(*)(int64_t, int64_t))mem;
    int64_t result = f(arg0, arg1);
    munmap(mem, sz + 4096);
    return result;
}

static int64_t exec_via_qemu(const char *arch, const uint8_t *code, size_t sz,
                              int64_t arg0, int64_t arg1) {
    char bin_path[256], elf_path[256], qemu_path[256];
    snprintf(bin_path, sizeof(bin_path), "/tmp/cross_%s.bin", arch);
    snprintf(elf_path, sizeof(elf_path), "/tmp/cross_%s.elf", arch);
    snprintf(qemu_path, sizeof(qemu_path), "/tmp/qemu-%s-static", arch);

    /* Write raw code */
    FILE *f = fopen(bin_path, "wb");
    if (!f) return -998;
    fwrite(code, 1, sz, f);
    fclose(f);

    /* Build ELF with args */
    char cmd[512];
    snprintf(cmd, sizeof(cmd),
             "python3 /tmp/build_elf.py %s %s %s %lld %lld && chmod 755 %s 2>&1",
             arch, bin_path, elf_path, (long long)arg0, (long long)arg1, elf_path);
    if (system(cmd) != 0) return -997;

    /* Run via qemu */
    snprintf(cmd, sizeof(cmd), "%s %s 2>&1", qemu_path, elf_path);
    FILE *p = popen(cmd, "r");
    if (!p) return -996;

    /* Read output (if any) and exit code */
    char buf[1024];
    fgets(buf, sizeof(buf), p);
    int rc = pclose(p);
    if (WIFEXITED(rc)) {
        return WEXITSTATUS(rc);
    }
    return -995;
}

int main(void) {
    int pass = 0, fail = 0, total = 0;

    struct {
        const char *src;
        int64_t arg0, arg1;
        int64_t expect;
        const char *desc;
    } tests[] = {
        {"return 42;",          0,  0,  42,       "constant"},
        {"return a + b;",       10, 32, 42,       "add"},
        {"return 1 + 2 * 3;",   0,  0,  7,        "precedence"},
        {"return a < b;",       3,  5,  1,        "compare_lt"},
        {"return a * b;",       6,  7,  42,       "multiply"},
        {"return 17 % 5;",      0,  0,  2,        "modulo"},
        {NULL, 0, 0, 0, NULL}
    };

    /* --- x86-64 direct --- */
    printf("=== x86-64 direct execution ===\n");
    for (int t = 0; tests[t].src; t++) {
        total++;
        CodeGen *cg = cg_create_x86();
        jit_minic_compile_cg(cg, tests[t].src);
        size_t sz;
        const uint8_t *code = jit_minic_get_code(cg, &sz);
        int64_t result = exec_x86(code, sz, tests[t].arg0, tests[t].arg1);
        if (result == tests[t].expect) {
            pass++;
            printf("  PASS: %-12s %2ld\n", tests[t].desc, (long)result);
        } else {
            fail++;
            printf("  FAIL: %-12s got=%ld expect=%ld\n", tests[t].desc, (long)result, (long)tests[t].expect);
        }
        cg_destroy(cg);
    }

    /* --- ARM64 via qemu --- */
    printf("\n=== ARM64 via qemu-aarch64 ===\n");
    int arm_pass = 0, arm_total = 0;
    for (int t = 0; tests[t].src; t++) {
        arm_total++;
        CodeGen *cg = cg_create_arm64();
        jit_minic_compile_cg(cg, tests[t].src);
        size_t sz;
        const uint8_t *code = jit_minic_get_code(cg, &sz);
        int64_t result = exec_via_qemu("aarch64", code, sz, tests[t].arg0, tests[t].arg1);
        if (result == tests[t].expect) {
            arm_pass++;
            printf("  PASS: %-12s %2ld\n", tests[t].desc, (long)result);
        } else {
            printf("  FAIL: %-12s got=%ld expect=%ld (code=%zuB)\n",
                   tests[t].desc, (long)result, (long)tests[t].expect, sz);
        }
        cg_destroy(cg);
    }

    /* --- RISC-V via qemu --- */
    printf("\n=== RISC-V via qemu-riscv64 ===\n");
    int rv_pass = 0, rv_total = 0;
    for (int t = 0; tests[t].src; t++) {
        rv_total++;
        CodeGen *cg = cg_create_rv64();
        jit_minic_compile_cg(cg, tests[t].src);
        size_t sz;
        const uint8_t *code = jit_minic_get_code(cg, &sz);
        int64_t result = exec_via_qemu("riscv64", code, sz, tests[t].arg0, tests[t].arg1);
        if (result == tests[t].expect) {
            rv_pass++;
            printf("  PASS: %-12s %2ld\n", tests[t].desc, (long)result);
        } else {
            printf("  FAIL: %-12s got=%ld expect=%ld (code=%zuB)\n",
                   tests[t].desc, (long)result, (long)tests[t].expect, sz);
        }
        cg_destroy(cg);
    }

    printf("\n=== SUMMARY ===\n");
    printf("  x86-64:  %d/%d passed\n", pass, total);
    printf("  ARM64:   %d/%d passed\n", arm_pass, arm_total);
    printf("  RISC-V:  %d/%d passed\n", rv_pass, rv_total);
    return (pass == total && arm_pass == arm_total && rv_pass == rv_total) ? 0 : 1;
}
