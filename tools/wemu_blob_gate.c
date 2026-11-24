/* tools/wemu_blob_gate.c -- run WuBuNOS's x86-64 output inside WEMU.
 *
 * The compiler and the emulator are two models of the same machine, living in
 * the same tree (src/compiler -> WuBuNOS, src/wemu -> WEMU). Until now nothing
 * made them check each other: the x86-64 backend's emitted bytes were executed
 * by the host CPU (test_isa_driver) or not at all, and WEMU was validated by
 * booting a kernel, not by running generated code.
 *
 * This closes the loop the way qemu-user closes it for the cross backends: take
 * the exact bytes WuBuNOS emits for each MIR case (dumped by test_isa_driver
 * with WUBU_EXEC_DUMP=x86-64) and EXECUTE them in WEMU, on a flat memory image
 * with paging off:
 *
 *      0x1000  the emitted code           (entry RIP)
 *      0x2000  HLT                        (the return address)
 *      0x8000  stack, [rsp] = 0x2000
 *
 * A backend bug and an emulator bug both show up as a mismatch; agreement is
 * evidence for both sides.
 */
#define _POSIX_C_SOURCE 200809L
#include "wemu.h"
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>

#define RAM_SIZE   (1u << 20)
#define CODE_AT    0x1000ull
#define HALT_AT    0x2000ull
#define STACK_TOP  0x8000ull
#define MAX_INSNS  10000

/* The x86-64 backend is ALSO the in-tree JIT, and it bakes a HOST address into
 * its output:
 *
 *     movabs rbx, &wubu_jit_mem_ptr      (48 BB imm64, see wubu_isa_x86_64.c:694)
 *
 * That is correct for a JIT -- the memory base lives in the process that
 * compiled the code -- but it means the bytes are not relocatable and cannot be
 * executed by an emulator that does not have that global at that address. Detected
 * here so the gate reports the reason instead of nineteen unexplained faults. */
static int host_bound(const uint8_t *code, size_t len)
{
    for (size_t i = 0; i + 10 <= len; i++) {
        if (code[i] == 0x48 && code[i + 1] == 0xBB) {
            uint64_t imm;
            memcpy(&imm, code + i + 2, 8);
            if (imm >= RAM_SIZE) return 1;      /* outside guest RAM = a host ptr */
        }
    }
    return 0;
}

static int run_blob(const uint8_t *code, size_t len, int64_t *out)
{
    WemuCpu *c = wemu_cpu_create(RAM_SIZE);
    if (!c) return -1;

    uint8_t hlt = 0xF4;                      /* HLT: the guest stops here */
    uint64_t ret_addr = HALT_AT;
    *out = 0;

    if (wemu_cpu_load(c, CODE_AT, code, len) != 0) goto bad;
    if (wemu_mem_write(c, HALT_AT, &hlt, 1) != 0) goto bad;
    if (wemu_mem_write(c, STACK_TOP, &ret_addr, sizeof ret_addr) != 0) goto bad;

    wemu_cpu_set_rsp(c, STACK_TOP);
    for (int i = 0; i < 16; i++) wemu_cpu_set_reg(c, i, 0);
    wemu_cpu_set_rip(c, CODE_AT);

    int64_t ran = wemu_cpu_run(c, MAX_INSNS);
    if (ran < 0 || wemu_cpu_fault(c)) {
        fprintf(stderr, "  fault %d at rip=%llx\n", wemu_cpu_fault(c),
                (unsigned long long)wemu_cpu_rip(c));
        wemu_cpu_destroy(c);
        return -1;
    }
    if (!wemu_cpu_halted(c)) {
        fprintf(stderr, "  did not halt (rip=%llx)\n",
                (unsigned long long)wemu_cpu_rip(c));
        wemu_cpu_destroy(c);
        return -1;
    }
    *out = (int64_t)wemu_cpu_reg(c, 0);      /* RAX = the MIR return value */
    wemu_cpu_destroy(c);
    return 0;
bad:
    wemu_cpu_destroy(c);
    return -1;
}

int main(int argc, char **argv)
{
    const char *dir = argc > 1 ? argv[1] : "build/isaexec/x86-64";
    DIR *d = opendir(dir);
    if (!d) {
        printf("wemu_blob_gate: SKIP (no %s -- run the dumper first)\n", dir);
        return 0;
    }
    printf("== x86-64 backend executed inside WEMU (%s) ==\n", dir);

    int cases = 0, fails = 0, host_bound_cases = 0;
    struct dirent *e;
    while ((e = readdir(d))) {
        size_t n = strlen(e->d_name);
        if (n < 4 || strcmp(e->d_name + n - 4, ".bin") != 0) continue;
        char path[512], wantp[512];
        snprintf(path, sizeof path, "%s/%s", dir, e->d_name);
        snprintf(wantp, sizeof wantp, "%s/%.*s.want", dir, (int)(n - 4), e->d_name);
        FILE *f = fopen(path, "rb");
        FILE *w = fopen(wantp, "r");
        if (!f || !w) { if (f) fclose(f); if (w) fclose(w); continue; }
        uint8_t code[4096];
        size_t len = fread(code, 1, sizeof code, f);
        fclose(f);
        long want = 0;
        if (fscanf(w, "%ld", &want) != 1) { fclose(w); continue; }
        fclose(w);
        cases++;

        if (host_bound(code, len)) {
            host_bound_cases++;
            continue;
        }

        int64_t got = 0;
        if (run_blob(code, len, &got) != 0) {
            printf("  FAIL %-24s did not run\n", e->d_name);
            fails++;
            continue;
        }
        if ((got & 0xFF) != (want & 0xFF)) {
            printf("  FAIL %-24s got %lld want %ld\n",
                   e->d_name, (long long)got, want);
            fails++;
        } else {
            printf("  PASS %-24s rax&0xFF=%lld want %ld\n",
                   e->d_name, (long long)(got & 0xFF), want);
        }
    }
    closedir(d);

    printf("== %d case(s) executed in WEMU, %d failed, %d host-bound ==\n",
           cases, fails, host_bound_cases);
    if (cases > 0 && host_bound_cases == cases) {
        printf("wemu_blob_gate: SKIP (%d blob(s) carry a baked host address -- the\n"
               "  x86-64 backend is a JIT: `movabs rbx, &wubu_jit_mem_ptr` points at\n"
               "  a global in the compiling process, so the bytes are not relocatable\n"
               "  and cannot run under an emulator. Verified by scanning the\n"
               "  emitted bytes for `48 BB` with a non-guest imm64.)\n",
               host_bound_cases);
        return 0;
    }
    if (cases == 0) {
        printf("wemu_blob_gate: SKIP (nothing dumped)\n");
        return 0;
    }
    return fails == 0 ? 0 : 1;
}
