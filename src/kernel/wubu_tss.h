/*
 * wubu_tss.h  --  WuBuOS TSS64 + GDT (freestanding)
 *
 * The kernel's GDT (crt0) has only code/data -- no TSS. A TSS is required
 * for (a) any IST wiring, (b) making a stray hardware task-return DEFINED
 * instead of #GP (the tracked preempt bug: a restored rflags with the NT
 * flag set makes the iretq attempt a task-return, and with no TSS the
 * descriptor is garbage -> #GP), and (c) the future ring-3 trampoline.
 *
 * wubu_tss_init() reloads a kernel-owned GDT (same code/data selectors +
 * a TSS64 descriptor) and loads TR. Selectors:
 *   0x08 kernel code  0x10 kernel data  0x18 TSS
 *   0x23 user code    0x2B user data    (future ring-3)
 */
#ifndef WUBU_TSS_H
#define WUBU_TSS_H

/* Reload the kernel GDT (with the TSS descriptor) + load TR. */
void wubu_tss_init(void);

#define WUBU_GDT_TSS     0x18   /* TSS selector */
#define WUBU_GDT_USER_CS 0x23
#define WUBU_GDT_USER_DS 0x2B

#endif /* WUBU_TSS_H */
