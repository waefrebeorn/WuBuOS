/*
 * wubu_wemu_prot_test.c -- differential gate for VSL mprotect enforcement
 * in the WEMU guest-memory write path.
 *
 * A guest maps a VSL arena page read-write, we record PROT_READ via
 * vsl_mprotect(), then a real guest STORE instruction targets that page:
 *
 *   NEGATIVE CONTROL (no hook == pre-change code): the store LANDS.
 *   ENFORCEMENT (wubu_wemu_prot_hook installed): the store FAULTS (#PF),
 *   the page is unchanged, and CR2 == the protected page.
 *
 * Legacy/unmanaged guest RAM is exercised too: a store outside any VSL
 * mapping stays permissive with the hook installed (no regression).
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

#define GUEST_CODE 0x1000ull
#define STORE_VAL  0x11223344u

/* mov dword [rax], imm32 ; hlt  ==  C7 00 44 33 22 11 F4 */
static const uint8_t store_code[] = {
    0xC7, 0x00, 0x44, 0x33, 0x22, 0x11, 0xF4
};

static WemuCpu *make_cpu(uint64_t page) {
    WemuCpu *cpu = wemu_cpu_create(64ull << 20);
    if (!cpu) { fprintf(stderr, "FAIL: wemu_cpu_create\n"); exit(1); }
    if (wemu_cpu_load(cpu, GUEST_CODE, store_code, sizeof store_code) != 0) {
        fprintf(stderr, "FAIL: wemu_cpu_load\n"); exit(1);
    }
    wemu_cpu_set_rip(cpu, GUEST_CODE);
    wemu_cpu_set_reg(cpu, 0, page);   /* RAX = target guest page */
    return cpu;
}

/* Store into a page then report the resulting fault + value. Each run gets
 * a fresh (zeroed) guest RAM so the page starts as 0. */
static void run_store(uint64_t page, int enforce, const char *label,
                      int *out_fault, uint32_t *out_val, uint64_t *out_cr2) {
    WemuCpu *cpu = make_cpu(page);
    if (enforce)
        wemu_set_mem_perm_hook(wubu_wemu_prot_hook, NULL);
    else
        wemu_set_mem_perm_hook(NULL, NULL);   /* pre-change behaviour */
    wemu_cpu_run(cpu, 64);

    uint32_t value = 0xdeadbeefu;
    wemu_mem_read(cpu, page, &value, 4);      /* reads are never denied */
    *out_fault = wemu_cpu_fault(cpu);
    *out_val = value;
    *out_cr2 = wemu_cpu_fault_addr(cpu);
    printf("gate: %-14s fault=%d cr2=0x%llx store_value=0x%08x\n",
           label, *out_fault, (unsigned long long)*out_cr2, value);
    wemu_cpu_destroy(cpu);
}

int main(void) {
    if (vsl_init() != 0) {
        fprintf(stderr, "FAIL: vsl_init\n"); return 1;
    }
    wubu_kernel_select("linux");

    uint64_t page = vsl_mmap(0, 0x1000, PROT_READ | PROT_WRITE,
                             MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (!page) {
        fprintf(stderr, "FAIL: vsl_mmap\n"); return 1;
    }
    printf("gate: VSL arena page = 0x%llx\n", (unsigned long long)page);
    printf("gate: prot before mprotect = %d\n", vsl_prot_query(page));

    int64_t rc = vsl_sys_mprotect(page, 0x1000, PROT_READ, 0, 0, 0);
    if (rc != 0) {
        fprintf(stderr, "FAIL: vsl_sys_mprotect rc=%lld\n", (long long)rc);
        return 1;
    }
    int prot = vsl_prot_query(page);
    printf("gate: prot after mprotect  = %d (READ=%d WRITE=%d)\n",
           prot, VSL_PROT_READ, VSL_PROT_WRITE);
    if (!(prot & VSL_PROT_READ) || (prot & VSL_PROT_WRITE)) {
        fprintf(stderr, "FAIL: vsl_prot_query did not record PROT_READ\n");
        return 1;
    }

    int f_nc, f_en;
    uint32_t v_nc, v_en;
    uint64_t cr2_nc, cr2_en;

    /* ---- negative control: no hook (pre-change code) -> store lands ---- */
    run_store(page, 0, "negative-ctrl", &f_nc, &v_nc, &cr2_nc);
    if (f_nc != 0 || v_nc != STORE_VAL) {
        fprintf(stderr, "FAIL: negative control store did not land "
                        "(fault=%d value=0x%08x)\n", f_nc, v_nc);
        return 1;
    }
    printf("PASS: negative control -- pre-change store SUCCEEDED\n");

    /* ---- enforcement: hook consults VSL prot -> store raises #PF -------- */
    run_store(page, 1, "enforced", &f_en, &v_en, &cr2_en);
    if (f_en != 14 || v_en != 0 || cr2_en != page) {
        fprintf(stderr, "FAIL: enforcement expected #PF(14)/CR2=page/value=0, "
                        "got fault=%d cr2=0x%llx value=0x%08x\n",
                f_en, (unsigned long long)cr2_en, v_en);
        return 1;
    }
    printf("PASS: enforcement -- PROT_READ page rejected the store (#PF)\n");

    /* ---- no regression: legacy guest RAM outside VSL stays writable ----- */
    uint64_t legacy = 0x00300000ull;  /* below VSL_USER_BASE, inside RAM */
    printf("gate: legacy prot query at 0x%llx = %d (permissive=-1)\n",
           (unsigned long long)legacy, vsl_prot_query(legacy));
    WemuCpu *cpu = make_cpu(legacy);
    wemu_set_mem_perm_hook(wubu_wemu_prot_hook, NULL);
    wemu_cpu_run(cpu, 64);
    uint32_t lv = 0;
    wemu_mem_read(cpu, legacy, &lv, 4);
    int lf = wemu_cpu_fault(cpu);
    wemu_cpu_destroy(cpu);
    printf("gate: legacy-page      fault=%d store_value=0x%08x\n", lf, lv);
    if (lf != 0 || lv != STORE_VAL) {
        fprintf(stderr, "FAIL: legacy guest RAM regression (fault=%d value=0x%08x)\n",
                lf, lv);
        return 1;
    }
    wemu_set_mem_perm_hook(NULL, NULL);
    printf("PASS: legacy guest RAM outside VSL remains permissive\n");

    printf("ALL VSL mprotect WEMU ENFORCEMENT GATES PASSED\n");
    return 0;
}
