/*
 * wubu_mmu.c -- kernel-owned GPU MMU page-table routing.
 *
 * GPU MMU (Memory Management Unit) handles page tables + translation.
 * "Runs on everything" includes correct GPU MMU on every device.
 *
 * MMU:
 *   - page-table: GPU page tables, VMA
 *   - translation: GPU page translation
 *   - fault: page fault handler
 *   - amdgpu: GPUVM, page table, VM context
 *   - i915: GGTT, PPGTT, page tables
 *   - nvidia: GPU page table, fault
 *   - /sys/class/drm/card*: vm, page_table
 *
 * WuBuOS owns this: detect MMU + page-table + fault, route to the
 * right driver, expose the topology.
 *
 * Research (7-hop on the mmu frontier):
 *   -AMDGPU MMU page table
 *   - GPU page table translation
 */
#include "wubu_mmu.h"
#include "wubu_hw_detect.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static int  g_mmu = 0;         /* MMU present */
static int  g_pt = 0;          /* page table */
static int  g_fault = 0;       /* page fault */
static int  g_vma = 0;         /* VMA */
static int  g_ctx = 0;         /* VM context */
static char g_mmu_drv[24] = "";

void wubu_mmu_probe(void)
{
    g_mmu = 0; g_pt = 0; g_fault = 0; g_vma = 0; g_ctx = 0;
    g_mmu_drv[0] = '\0';

#ifdef WUBU_HOSTED
    if (access("/sys/module/amdgpu", R_OK) == 0) {
        g_mmu = 1; g_pt = 1; g_fault = 1; g_vma = 1; g_ctx = 1;
        strcpy(g_mmu_drv, "amdgpu-mmu");
    }
    if (access("/sys/module/i915", R_OK) == 0) {
        g_mmu = 1; g_pt = 1; g_vma = 1; g_ctx = 1;
        if (!g_mmu_drv[0]) strcpy(g_mmu_drv, "i915-mmu");
    }
    if (access("/sys/module/nvidia", R_OK) == 0) {
        g_mmu = 1; g_pt = 1; g_fault = 1;
        if (!g_mmu_drv[0]) strcpy(g_mmu_drv, "nvidia-mmu");
    }
#endif
}

int  wubu_mmu_present(void){ return g_mmu; }
int  wubu_mmu_pagetable(void){ return g_pt; }
int  wubu_mmu_fault(void)   { return g_fault; }
int  wubu_mmu_vma(void)     { return g_vma; }
int  wubu_mmu_ctx(void)     { return g_ctx; }
const char *wubu_mmu_driver(void){ return g_mmu_drv[0] ? g_mmu_drv : NULL; }

const char *wubu_mmu_type_for(const char *t)
{
    if (!t) return NULL;
    if (strstr(t, "pte") || strstr(t, "pt")) return "page-table";
    if (strstr(t, "pde") || strstr(t, "pd")) return "page-directory";
    if (strstr(t, "pde")) return "page-directory";
    if (strstr(t, "vm") || strstr(t, "context")) return "vm-context";
    if (strstr(t, "ggtt")) return "ggtt";
    if (strstr(t, "ppgtt")) return "ppgtt";
    return "page-table";
}

const char *wubu_mmu_fault_for(const char *f)
{
    if (!f) return NULL;
    if (strstr(f, "page")) return "page-fault";
    if (strstr(f, "access")) return "access-fault";
    if (strstr(f, "prot")) return "protection-fault";
    if (strstr(f, "gpu")) return "gpu-page-fault";
    return "page-fault";
}

int wubu_mmu_summary(char *out, size_t cap)
{
    return snprintf(out, cap,
        "mmu[mmu=%d pt=%d fault=%d vma=%d ctx=%d drv=%s]",
        g_mmu, g_pt, g_fault, g_vma, g_ctx,
        wubu_mmu_driver() ? wubu_mmu_driver() : "none");
}
