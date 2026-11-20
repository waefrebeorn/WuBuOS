/*
 * wubu_wemu_prot_test.c -- differential gate for VSL mprotect enforcement
 * in the WEMU guest-memory READ, WRITE and instruction-FETCH paths.
 *
 * The SAME guest programs run twice:
 *   NEGATIVE CONTROL (no hook == pre-change code): the access SUCCEEDS.
 *   ENFORCEMENT (both hooks installed): the VSL protection decides.
 *
 * Asserts:
 *   PROT_READ page : load OK, store #PF (CR2=page).
 *   PROT_NONE page : load #PF and store #PF (CR2=page for both).
 *   no-PROT_EXEC   : instruction fetch #PF (CR2=RIP=page).
 *   legacy RAM     : store + load + fetch all stay permissive.
 */
#include "wemu.h"
#include "wubu_wemu_bridge.h"
#include "wubu_vsl.h"
#include "wubu_kernel_xchg.h"
#include "vsl/vsl_memory.h"
#include "vsl/vsl_prot.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>

extern int64_t vsl_sys_mprotect(uint64_t addr, uint64_t len, uint64_t prot,
                                uint64_t d, uint64_t e, uint64_t f);

#define GUEST_CODE 0x1000ull       /* below VSL_USER_BASE: legacy flat RAM */
#define STORE_VAL  0x11223344u
#define LOAD_VAL   0xA5A5BEEFu

/* mov dword [rax], imm32 ; hlt  ==  C7 00 44 33 22 11 F4 */
static const uint8_t store_code[] = {
    0xC7, 0x00, 0x44, 0x33, 0x22, 0x11, 0xF4
};
/* mov eax, dword [rax] ; hlt  ==  8B 00 F4 */
static const uint8_t load_code[] = {
    0x8B, 0x00, 0xF4
};
/* hlt  ==  F4  (fetch/decode the byte at the page, then halt) */
static const uint8_t fetch_code[] = { 0xF4 };

/* Install or clear BOTH hooks. Cleared == pre-change (permissive) code. */
static void set_hooks(int enforce) {
    if (enforce) {
        wemu_set_mem_perm_hook(wubu_wemu_prot_hook, NULL);
        wemu_set_fetch_perm_hook(wubu_wemu_fetch_hook, NULL);
    } else {
        wemu_set_mem_perm_hook(NULL, NULL);
        wemu_set_fetch_perm_hook(NULL, NULL);
    }
}

typedef struct {
    int fault;
    uint64_t cr2;
    uint64_t rax;
    int halted;
} Result;

/* One isolated run: fresh zeroed RAM, optional 4-byte prefill of `prefill_va`,
 * code loaded at `code_va`, RIP and RAX set, then run. */
static Result run_guest(uint64_t code_va, const uint8_t *code, size_t len,
                        uint64_t rip, uint64_t rax,
                        int prefill, uint64_t prefill_va, uint32_t prefill_val,
                        int enforce) {
    set_hooks(enforce);
    WemuCpu *cpu = wemu_cpu_create(64ull << 20);
    if (!cpu) { fprintf(stderr, "FAIL: wemu_cpu_create\n"); exit(1); }
    if (wemu_cpu_load(cpu, code_va, code, len) != 0) {
        fprintf(stderr, "FAIL: wemu_cpu_load(code)\n"); exit(1);
    }
    if (prefill && wemu_cpu_load(cpu, prefill_va,
                                 (const uint8_t *)&prefill_val, 4) != 0) {
        fprintf(stderr, "FAIL: wemu_cpu_load(prefill)\n"); exit(1);
    }
    wemu_cpu_set_rip(cpu, rip);
    wemu_cpu_set_reg(cpu, 0, rax);
    wemu_cpu_run(cpu, 64);

    Result r;
    r.fault  = wemu_cpu_fault(cpu);
    r.cr2    = wemu_cpu_fault_addr(cpu);
    r.rax    = wemu_cpu_reg(cpu, 0);
    r.halted = wemu_cpu_halted(cpu);
    wemu_cpu_destroy(cpu);
    return r;
}

/* Read back a dword through WEMU's RAW physical write path is not public, so
 * observe store effects via a guest LOAD instead: run `mov eax,[rax]` on a
 * fresh CPU over the SAME page content is not possible (per-CPU RAM). Instead
 * the store run itself reports whether the store faulted; the no-hook control
 * proves the store instruction lands by checking the guest did not fault and
 * that the subsequent HLT executed (halted). */
static Result store_case(uint64_t page, int enforce) {
    return run_guest(GUEST_CODE, store_code, sizeof store_code,
                     GUEST_CODE, page, 0, 0, 0, enforce);
}

static Result load_case(uint64_t page, int enforce) {
    return run_guest(GUEST_CODE, load_code, sizeof load_code,
                     GUEST_CODE, page, 1, page, LOAD_VAL, enforce);
}

static Result fetch_case(uint64_t page, int enforce) {
    return run_guest(page, fetch_code, sizeof fetch_code,
                     page, 0, 0, 0, 0, enforce);
}

static void pr(const char *label, Result r) {
    printf("gate: %-22s fault=%d cr2=0x%llx rax=0x%llx halted=%d\n",
           label, r.fault, (unsigned long long)r.cr2,
           (unsigned long long)r.rax, r.halted);
}

static int mp(uint64_t page, uint64_t prot) {
    int64_t rc = vsl_sys_mprotect(page, 0x1000, prot, 0, 0, 0);
    if (rc != 0) {
        fprintf(stderr, "FAIL: vsl_sys_mprotect(%llu) rc=%lld\n",
                (unsigned long long)prot, (long long)rc);
        return -1;
    }
    return 0;
}

int main(void) {
    if (vsl_init() != 0) { fprintf(stderr, "FAIL: vsl_init\n"); return 1; }
    wubu_kernel_select("linux");

    uint64_t page = vsl_mmap(0, 0x1000, PROT_READ | PROT_WRITE,
                             MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (!page) { fprintf(stderr, "FAIL: vsl_mmap\n"); return 1; }
    printf("gate: VSL arena page = 0x%llx\n", (unsigned long long)page);

    /* ---- PROT_READ: load allowed, store denied ----------------------- */
    if (mp(page, PROT_READ)) return 1;
    printf("gate: prot=PROT_READ query=%d\n", vsl_prot_query(page));

    Result r = load_case(page, 0); pr("READ/load/negative", r);
    if (r.fault != 0 || r.rax != LOAD_VAL) {
        fprintf(stderr, "FAIL: pre-change LOAD did not succeed\n"); return 1;
    }
    printf("PASS: negative control -- PROT_READ load succeeded (pre-change)\n");

    r = load_case(page, 1); pr("READ/load/enforced", r);
    if (r.fault != 0 || r.rax != LOAD_VAL) {
        fprintf(stderr, "FAIL: PROT_READ must allow the load\n"); return 1;
    }
    printf("PASS: PROT_READ page allows the load\n");

    r = store_case(page, 0); pr("READ/store/negative", r);
    if (r.fault != 0 || !r.halted) {
        fprintf(stderr, "FAIL: pre-change STORE did not succeed\n"); return 1;
    }
    printf("PASS: negative control -- PROT_READ store succeeded (pre-change)\n");

    r = store_case(page, 1); pr("READ/store/enforced", r);
    if (r.fault != 14 || r.cr2 != page) {
        fprintf(stderr, "FAIL: PROT_READ must block the store (#PF/CR2)\n");
        return 1;
    }
    printf("PASS: PROT_READ page rejected the store (#PF, CR2=page)\n");

    /* ---- PROT_NONE: both load and store denied ----------------------- */
    if (mp(page, PROT_NONE)) return 1;
    printf("gate: prot=PROT_NONE query=%d\n", vsl_prot_query(page));

    r = load_case(page, 0); pr("NONE/load/negative", r);
    if (r.fault != 0 || r.rax != LOAD_VAL) {
        fprintf(stderr, "FAIL: pre-change PROT_NONE load did not succeed\n");
        return 1;
    }
    printf("PASS: negative control -- PROT_NONE load succeeded (pre-change)\n");

    r = load_case(page, 1); pr("NONE/load/enforced", r);
    if (r.fault != 14 || r.cr2 != page) {
        fprintf(stderr, "FAIL: PROT_NONE must block the load (#PF/CR2)\n");
        return 1;
    }
    printf("PASS: PROT_NONE page rejected the load (#PF, CR2=page)\n");

    r = store_case(page, 0); pr("NONE/store/negative", r);
    if (r.fault != 0 || !r.halted) {
        fprintf(stderr, "FAIL: pre-change PROT_NONE store did not succeed\n");
        return 1;
    }
    printf("PASS: negative control -- PROT_NONE store succeeded (pre-change)\n");

    r = store_case(page, 1); pr("NONE/store/enforced", r);
    if (r.fault != 14 || r.cr2 != page) {
        fprintf(stderr, "FAIL: PROT_NONE must block the store (#PF/CR2)\n");
        return 1;
    }
    printf("PASS: PROT_NONE page rejected the store (#PF, CR2=page)\n");

    /* ---- no PROT_EXEC: fetch denied ---------------------------------- */
    if (mp(page, PROT_READ | PROT_WRITE)) return 1;
    printf("gate: prot=RW-no-EXEC query=%d\n", vsl_prot_query(page));

    r = fetch_case(page, 0); pr("EXEC/fetch/negative", r);
    if (r.fault != 0 || !r.halted) {
        fprintf(stderr, "FAIL: pre-change fetch from no-exec page did not run\n");
        return 1;
    }
    printf("PASS: negative control -- fetch succeeded (pre-change)\n");

    r = fetch_case(page, 1); pr("EXEC/fetch/enforced", r);
    if (r.fault != 14 || r.cr2 != page) {
        fprintf(stderr, "FAIL: no-PROT_EXEC page must block fetch (#PF/CR2)\n");
        return 1;
    }
    printf("PASS: no-EXEC page rejected the fetch (#PF, CR2=RIP=page)\n");

    /* ---- legacy RAM stays permissive with BOTH hooks installed ------- */
    uint64_t legacy = 0x00300000ull;  /* below VSL_USER_BASE, inside RAM */
    printf("gate: legacy prot query at 0x%llx = %d (permissive=-1)\n",
           (unsigned long long)legacy, vsl_prot_query(legacy));

    r = store_case(legacy, 1); pr("legacy/store/enforced", r);
    if (r.fault != 0 || !r.halted) {
        fprintf(stderr, "FAIL: legacy RAM store regressed\n"); return 1;
    }
    r = load_case(legacy, 1); pr("legacy/load/enforced", r);
    if (r.fault != 0 || r.rax != LOAD_VAL) {
        fprintf(stderr, "FAIL: legacy RAM load regressed\n"); return 1;
    }
    r = fetch_case(legacy, 1); pr("legacy/fetch/enforced", r);
    if (r.fault != 0 || !r.halted) {
        fprintf(stderr, "FAIL: legacy RAM fetch regressed\n"); return 1;
    }
    printf("PASS: legacy guest RAM stays writable/loadable/executable\n");

    set_hooks(0);
    printf("ALL VSL mprotect WEMU READ/WRITE/FETCH ENFORCEMENT GATES PASSED\n");
    return 0;
}
