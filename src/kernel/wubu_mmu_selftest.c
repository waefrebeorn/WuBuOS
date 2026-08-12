/*
 * wubu_mmu_selftest.c -- verifies GPU MMU routing.
 */
#include "wubu_mmu.h"
#include "wubu_hw_detect.h"
#include <stdio.h>
#include <string.h>

#define CHECK(cond, msg) do { \
    if (!(cond)) { printf("FAIL: %s\n", msg); failures++; } \
    else { printf("  ok: %s\n", msg); passed++; } \
} while(0)

static int failures = 0;
static int passed = 0;

int main(void)
{
    printf("=== wubu_mmu_selftest ===\n\n");
    wubu_hw_detect();
    wubu_mmu_probe();
    printf("  mmu=%d pt=%d fault=%d vma=%d ctx=%d\n",
           wubu_mmu_present(), wubu_mmu_pagetable(), wubu_mmu_fault(),
           wubu_mmu_vma(), wubu_mmu_ctx());

    CHECK(strcmp(wubu_mmu_type_for("pte"), "page-table") == 0,
          "pte -> page-table");
    CHECK(strcmp(wubu_mmu_type_for("pt"), "page-table") == 0,
          "pt -> page-table");
    CHECK(strcmp(wubu_mmu_type_for("pde"), "page-directory") == 0,
          "pde -> page-directory");
    CHECK(strcmp(wubu_mmu_type_for("vm"), "vm-context") == 0,
          "vm -> vm-context");
    CHECK(strcmp(wubu_mmu_type_for("ggtt"), "ggtt") == 0,
          "ggtt -> ggtt");
    CHECK(strcmp(wubu_mmu_type_for("ppgtt"), "ppgtt") == 0,
          "ppgtt -> ppgtt");
    CHECK(strcmp(wubu_mmu_type_for("zzz"), "page-table") == 0,
          "zzz -> page-table fallback");

    CHECK(strcmp(wubu_mmu_fault_for("page"), "page-fault") == 0,
          "page -> page-fault");
    CHECK(strcmp(wubu_mmu_fault_for("access"), "access-fault") == 0,
          "access -> access-fault");
    CHECK(strcmp(wubu_mmu_fault_for("prot"), "protection-fault") == 0,
          "prot -> protection-fault");
    CHECK(strcmp(wubu_mmu_fault_for("gpu"), "gpu-page-fault") == 0,
          "gpu -> gpu-page-fault");
    CHECK(strcmp(wubu_mmu_fault_for("zzz"), "page-fault") == 0,
          "zzz -> page-fault fallback");

    char s[256];
    wubu_mmu_summary(s, sizeof(s));
    printf("  summary: %s\n", s);
    CHECK(s[0] != '\0', "mmu summary generated");

    printf("\n=== MMU TESTS: %d passed, %d failed ===\n", passed, failures);
    return failures ? 1 : 0;
}
