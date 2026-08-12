/*
 * wubu_mmu.h -- kernel-owned GPU MMU routing.
 */
#ifndef WUBU_MMU_H
#define WUBU_MMU_H

#include <stddef.h>

void wubu_mmu_probe(void);
int  wubu_mmu_present(void);
int  wubu_mmu_pagetable(void);
int  wubu_mmu_fault(void);
int  wubu_mmu_vma(void);
int  wubu_mmu_ctx(void);
const char *wubu_mmu_driver(void);
const char *wubu_mmu_type_for(const char *t);
const char *wubu_mmu_fault_for(const char *f);
int wubu_mmu_summary(char *out, size_t cap);

#endif
