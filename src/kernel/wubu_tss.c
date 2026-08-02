/*
 * wubu_tss.c  --  WuBuOS TSS64 + GDT (freestanding)
 *
 * See wubu_tss.h. The GDT is rebuilt at runtime so the TSS base (a
 * higher-half BSS address) is computed, not linked.
 */

#include "wubu_tss.h"

#include <stdint.h>
#include <stddef.h>

static uint64_t g_gdt[8];        /* 0:null 8:code 16:data 24:tss 32:u-code 40:u-data */
static uint8_t  g_tss[104];      /* the TSS64 (minimum 104 bytes) */
static uint8_t  g_ist1_stack[8192] __attribute__((aligned(16)));
/* IST1 top (the TSS stores the TOP; the stack grows down) */
#define WUBU_IST1_TOP ((uintptr_t)g_ist1_stack + sizeof(g_ist1_stack))

static void gdt_set_tss64(uint64_t *gdt, int idx, uintptr_t base)
{
    /* TSS64 descriptor (16 bytes at gdt[idx], gdt[idx+1]).
     * Limit 0x67 = 103 bytes (the minimum TSS). Type 0x89 = present,
     * available 64-bit TSS. */
    uint64_t low = 0x67u
                 | ((uint64_t)(base & 0xFFFFFFu) << 16)
                 | ((uint64_t)0x89u << 40)
                 | ((uint64_t)((base >> 24) & 0xFFu) << 56);
    uint64_t high = (uint64_t)(base >> 32);
    gdt[idx]     = low;
    gdt[idx + 1] = high;
}

void wubu_tss_init(void)
{
    g_gdt[0] = 0;                                   /* null */
    g_gdt[1] = 0x00af9b000000ffffu;                 /* kernel code  */
    g_gdt[2] = 0x00cf93000000ffffu;                 /* kernel data  */
    gdt_set_tss64(g_gdt, 3, (uintptr_t)g_tss);
    g_gdt[5] = 0x00affb000000ffffu;                 /* user code (future) */
    g_gdt[6] = 0x00cff3000000ffffu;                 /* user data (future) */

    /* Zero the TSS; IO-map-base at offset 102 = 0xFFFF (no IO bitmap). */
    for (int i = 0; i < 104; i++)
        g_tss[i] = 0;
    g_tss[102] = 0xFF;
    g_tss[103] = 0xFF;

    /* IST1 (gap A2): a DEDICATED double-fault stack. When the CPU detects
     * a #DF while servicing an exception it switches to IST1 -- a stack
     * overflow in a task can no longer triple-fault (the #DF handler gets
     * clean stack + a real dump). TSS64 layout: IST1 = offset 36, 8 bytes.
     * The IST field holds the TOP of the stack. */
    *(uint64_t *)(g_tss + 36) = (uint64_t)WUBU_IST1_TOP;

    /* Load the new GDT (the old selectors stay valid: same entries) */
    struct { uint16_t limit; uint64_t base; } __attribute__((packed)) gdtr;
    gdtr.limit = (uint16_t)(sizeof(g_gdt) - 1);
    gdtr.base  = (uint64_t)g_gdt;
    __asm__ __volatile__("lgdt %0" : : "m"(gdtr) : "memory");

    /* Load TR (0x18 = the TSS descriptor). */
    __asm__ __volatile__("ltr %w0" : : "r"((uint16_t)WUBU_GDT_TSS) : "memory");
}
