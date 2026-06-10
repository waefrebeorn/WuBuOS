/*
 * interrupt.c — My Seed IDT/PIC Interrupt Controller
 *
 * Full x86_64 IDT implementation with 256 interrupt gates.
 * Assembly ISR stubs (isr_stubs.S) provide vector entry points and address table.
 * C dispatcher routes to registered handlers or preemptive scheduler.
 */

#include "interrupt.h"
#include "tasking.h"  /* For task_timer_tick, g_current */

#include <string.h>
#include <stdint.h>
#include <signal.h>

/* Forward declare InterruptFrame for the dispatcher */
struct InterruptFrame;

/* ──────────────────────────────────────────────────────────────────
 * External Assembly Symbols
 * ────────────────────────────────────────────────────────────────── */

/* ISR entry points from isr_stubs.S */
extern void isr0(void);   extern void isr1(void);   extern void isr2(void);   extern void isr3(void);
extern void isr4(void);   extern void isr5(void);   extern void isr6(void);   extern void isr7(void);
extern void isr8(void);   extern void isr9(void);   extern void isr10(void);  extern void isr11(void);
extern void isr12(void);  extern void isr13(void);  extern void isr14(void);  extern void isr15(void);
extern void isr16(void);  extern void isr17(void);  extern void isr18(void);  extern void isr19(void);
extern void isr20(void);  extern void isr21(void);  extern void isr22(void);  extern void isr23(void);
extern void isr24(void);  extern void isr25(void);  extern void isr26(void);  extern void isr27(void);
extern void isr28(void);  extern void isr29(void);  extern void isr30(void);  extern void isr31(void);
extern void isr32(void);  extern void isr33(void);  extern void isr34(void);  extern void isr35(void);
extern void isr36(void);  extern void isr37(void);  extern void isr38(void);  extern void isr39(void);
extern void isr40(void);  extern void isr41(void);  extern void isr42(void);  extern void isr43(void);
extern void isr44(void);  extern void isr45(void);  extern void isr46(void);  extern void isr47(void);
extern void isr48(void);  extern void isr49(void);  extern void isr50(void);  extern void isr51(void);
extern void isr52(void);  extern void isr53(void);  extern void isr54(void);  extern void isr55(void);
extern void isr56(void);  extern void isr57(void);  extern void isr58(void);  extern void isr59(void);
extern void isr60(void);  extern void isr61(void);  extern void isr62(void);  extern void isr63(void);
extern void isr64(void);  extern void isr65(void);  extern void isr66(void);  extern void isr67(void);
extern void isr68(void);  extern void isr69(void);  extern void isr70(void);  extern void isr71(void);
extern void isr72(void);  extern void isr73(void);  extern void isr74(void);  extern void isr75(void);
extern void isr76(void);  extern void isr77(void);  extern void isr78(void);  extern void isr79(void);
extern void isr80(void);  extern void isr81(void);  extern void isr82(void);  extern void isr83(void);
extern void isr84(void);  extern void isr85(void);  extern void isr86(void);  extern void isr87(void);
extern void isr88(void);  extern void isr89(void);  extern void isr90(void);  extern void isr91(void);
extern void isr92(void);  extern void isr93(void);  extern void isr94(void);  extern void isr95(void);
extern void isr96(void);  extern void isr97(void);  extern void isr98(void);  extern void isr99(void);
extern void isr100(void); extern void isr101(void); extern void isr102(void); extern void isr103(void);
extern void isr104(void); extern void isr105(void); extern void isr106(void); extern void isr107(void);
extern void isr108(void); extern void isr109(void); extern void isr110(void); extern void isr111(void);
extern void isr112(void); extern void isr113(void); extern void isr114(void); extern void isr115(void);
extern void isr116(void); extern void isr117(void); extern void isr118(void); extern void isr119(void);
extern void isr120(void); extern void isr121(void); extern void isr122(void); extern void isr123(void);
extern void isr124(void); extern void isr125(void); extern void isr126(void); extern void isr127(void);
extern void isr128(void); extern void isr129(void); extern void isr130(void); extern void isr131(void);
extern void isr132(void); extern void isr133(void); extern void isr134(void); extern void isr135(void);
extern void isr136(void); extern void isr137(void); extern void isr138(void); extern void isr139(void);
extern void isr140(void); extern void isr141(void); extern void isr142(void); extern void isr143(void);
extern void isr144(void); extern void isr145(void); extern void isr146(void); extern void isr147(void);
extern void isr148(void); extern void isr149(void); extern void isr150(void); extern void isr151(void);
extern void isr152(void); extern void isr153(void); extern void isr154(void); extern void isr155(void);
extern void isr156(void); extern void isr157(void); extern void isr158(void); extern void isr159(void);
extern void isr160(void); extern void isr161(void); extern void isr162(void); extern void isr163(void);
extern void isr164(void); extern void isr165(void); extern void isr166(void); extern void isr167(void);
extern void isr168(void); extern void isr169(void); extern void isr170(void); extern void isr171(void);
extern void isr172(void); extern void isr173(void); extern void isr174(void); extern void isr175(void);
extern void isr176(void); extern void isr177(void); extern void isr178(void); extern void isr179(void);
extern void isr180(void); extern void isr181(void); extern void isr182(void); extern void isr183(void);
extern void isr184(void); extern void isr185(void); extern void isr186(void); extern void isr187(void);
extern void isr188(void); extern void isr189(void); extern void isr190(void); extern void isr191(void);
extern void isr192(void); extern void isr193(void); extern void isr194(void); extern void isr195(void);
extern void isr196(void); extern void isr197(void); extern void isr198(void); extern void isr199(void);
extern void isr200(void); extern void isr201(void); extern void isr202(void); extern void isr203(void);
extern void isr204(void); extern void isr205(void); extern void isr206(void); extern void isr207(void);
extern void isr208(void); extern void isr209(void); extern void isr210(void); extern void isr211(void);
extern void isr212(void); extern void isr213(void); extern void isr214(void); extern void isr215(void);
extern void isr216(void); extern void isr217(void); extern void isr218(void); extern void isr219(void);
extern void isr220(void); extern void isr221(void); extern void isr222(void); extern void isr223(void);
extern void isr224(void); extern void isr225(void); extern void isr226(void); extern void isr227(void);
extern void isr228(void); extern void isr229(void); extern void isr230(void); extern void isr231(void);
extern void isr232(void); extern void isr233(void); extern void isr234(void); extern void isr235(void);
extern void isr236(void); extern void isr237(void); extern void isr238(void); extern void isr239(void);
extern void isr240(void); extern void isr241(void); extern void isr242(void); extern void isr243(void);
extern void isr244(void); extern void isr245(void); extern void isr246(void); extern void isr247(void);
extern void isr248(void); extern void isr249(void); extern void isr250(void); extern void isr251(void);
extern void isr252(void); extern void isr253(void); extern void isr254(void); extern void isr255(void);

extern IDTEntry idt_table[256];
extern IDTPtr idt_ptr;
extern void common_isr_handler;

/* Helper array of ISR addresses for IDT population */
static void (* const isr_entries[256])(void) = {
    isr0, isr1, isr2, isr3, isr4, isr5, isr6, isr7,
    isr8, isr9, isr10, isr11, isr12, isr13, isr14, isr15,
    isr16, isr17, isr18, isr19, isr20, isr21, isr22, isr23,
    isr24, isr25, isr26, isr27, isr28, isr29, isr30, isr31,
    isr32, isr33, isr34, isr35, isr36, isr37, isr38, isr39,
    isr40, isr41, isr42, isr43, isr44, isr45, isr46, isr47,
    isr48, isr49, isr50, isr51, isr52, isr53, isr54, isr55,
    isr56, isr57, isr58, isr59, isr60, isr61, isr62, isr63,
    isr64, isr65, isr66, isr67, isr68, isr69, isr70, isr71,
    isr72, isr73, isr74, isr75, isr76, isr77, isr78, isr79,
    isr80, isr81, isr82, isr83, isr84, isr85, isr86, isr87,
    isr88, isr89, isr90, isr91, isr92, isr93, isr94, isr95,
    isr96, isr97, isr98, isr99, isr100, isr101, isr102, isr103,
    isr104, isr105, isr106, isr107, isr108, isr109, isr110, isr111,
    isr112, isr113, isr114, isr115, isr116, isr117, isr118, isr119,
    isr120, isr121, isr122, isr123, isr124, isr125, isr126, isr127,
    isr128, isr129, isr130, isr131, isr132, isr133, isr134, isr135,
    isr136, isr137, isr138, isr139, isr140, isr141, isr142, isr143,
    isr144, isr145, isr146, isr147, isr148, isr149, isr150, isr151,
    isr152, isr153, isr154, isr155, isr156, isr157, isr158, isr159,
    isr160, isr161, isr162, isr163, isr164, isr165, isr166, isr167,
    isr168, isr169, isr170, isr171, isr172, isr173, isr174, isr175,
    isr176, isr177, isr178, isr179, isr180, isr181, isr182, isr183,
    isr184, isr185, isr186, isr187, isr188, isr189, isr190, isr191,
    isr192, isr193, isr194, isr195, isr196, isr197, isr198, isr199,
    isr200, isr201, isr202, isr203, isr204, isr205, isr206, isr207,
    isr208, isr209, isr210, isr211, isr212, isr213, isr214, isr215,
    isr216, isr217, isr218, isr219, isr220, isr221, isr222, isr223,
    isr224, isr225, isr226, isr227, isr228, isr229, isr230, isr231,
    isr232, isr233, isr234, isr235, isr236, isr237, isr238, isr239,
    isr240, isr241, isr242, isr243, isr244, isr245, isr246, isr247,
    isr248, isr249, isr250, isr251, isr252, isr253, isr254, isr255
};

/* ──────────────────────────────────────────────────────────────────
 * PIC I/O Ports
 * ────────────────────────────────────────────────────────────────── */

#define PIC1_CMD        0x20    /* Master PIC command */
#define PIC1_DATA       0x21    /* Master PIC data */
#define PIC2_CMD        0xA0    /* Slave PIC command */
#define PIC2_DATA       0xA1    /* Slave PIC data */

#define PIC_EOI         0x20    /* End of Interrupt command */

#define ICW1_ICW4       0x01    /* ICW4 needed */
#define ICW1_SINGLE     0x02    /* Single (cascade) mode */
#define ICW1_INTERVAL4  0x04    /* Call address interval 4 (8) */
#define ICW1_LEVEL      0x08    /* Level triggered (edge) */
#define ICW1_INIT       0x10    /* Initialization */

#define ICW4_8086       0x01    /* 8086/88 mode */
#define ICW4_AUTO       0x02    /* Auto EOI */
#define ICW4_BUF_SLAVE  0x08    /* Buffered mode/slave */
#define ICW4_BUF_MASTER 0x0C    /* Buffered mode/master */
#define ICW4_SFNM       0x10    /* Special fully nested mode */

/* ──────────────────────────────────────────────────────────────────
 * PIT I/O Ports
 * ────────────────────────────────────────────────────────────────── */

#define PIT_CH0_DATA    0x40
#define PIT_CMD         0x43

/* ──────────────────────────────────────────────────────────────────
 * APIC Registers (Memory-Mapped)
 * ────────────────────────────────────────────────────────────────── */

#define LAPIC_BASE_MSR      0x1B            /* IA32_APIC_BASE MSR */
#define LAPIC_BASE_DEFAULT  0xFEE00000      /* Default LAPIC base address */

#define LAPIC_ID               0x020
#define LAPIC_VERSION          0x030
#define LAPIC_TPR              0x080
#define LAPIC_APR              0x090
#define LAPIC_PPR              0x0A0
#define LAPIC_EOI              0x0B0
#define LAPIC_RRD              0x0C0
#define LAPIC_LDR              0x0D0
#define LAPIC_DFR              0x0E0
#define LAPIC_SVR              0x0F0
#define LAPIC_ISR              0x100
#define LAPIC_TMR              0x180
#define LAPIC_IRR              0x200
#define LAPIC_ESR              0x280
#define LAPIC_ICR_LOW          0x300
#define LAPIC_ICR_HIGH         0x310
#define LAPIC_LVT_TIMER        0x320
#define LAPIC_LVT_THERMAL      0x330
#define LAPIC_LVT_PERF         0x340
#define LAPIC_LVT_LINT0        0x350
#define LAPIC_LVT_LINT1        0x360
#define LAPIC_LVT_ERROR        0x370
#define LAPIC_TIMER_INIT_CNT   0x380
#define LAPIC_TIMER_CUR_CNT    0x390
#define LAPIC_TIMER_DIV        0x3E0

#define LAPIC_SVR_ENABLE       0x100
#define LAPIC_LVT_MASKED       0x10000
#define LAPIC_ICR_DEST_SELF    0x40000
#define LAPIC_ICR_DEST_ALL_INC 0x80000
#define LAPIC_ICR_DEST_ALL_BUT 0xC0000
#define LAPIC_ICR_LEVEL_ASSERT 0x4000
#define LAPIC_ICR_LEVEL_DEASSERT 0x0000
#define LAPIC_ICR_TRIGGER_LEVEL 0x8000
#define LAPIC_ICR_DELIVERY_FIXED 0x0000
#define LAPIC_ICR_DELIVERY_NMI 0x400
#define LAPIC_ICR_DELIVERY_INIT 0x500
#define LAPIC_ICR_DELIVERY_STARTUP 0x600

#define IOAPIC_BASE_DEFAULT    0xFEC00000
#define IOAPIC_ID              0x00
#define IOAPIC_VER             0x01
#define IOAPIC_ARB             0x02
#define IOAPIC_REDTBL          0x10
#define IOAPIC_REDTBL_HIGH     0x11
#define IOAPIC_RED_ENTRIES(v)  (((v) >> 16) & 0xFF)

#define IOAPIC_RED_DELIV_FIXED 0x0
#define IOAPIC_RED_DELIV_LOW_PRI 0x1
#define IOAPIC_RED_DELIV_SMI   0x2
#define IOAPIC_RED_DELIV_NMI   0x4
#define IOAPIC_RED_DELIV_INIT  0x5
#define IOAPIC_RED_DELIV_EXTINT 0x7
#define IOAPIC_RED_DESTMODE_PHYS 0x0
#define IOAPIC_RED_DESTMODE_LOG  0x8
#define IOAPIC_RED_DELIV_STATUS  0x1000
#define IOAPIC_RED_REMOTE_IRR    0x4000
#define IOAPIC_RED_TRIGGER_LEVEL 0x8000
#define IOAPIC_RED_POLARITY_LOW  0x2000
#define IOAPIC_RED_MASKED        0x10000

/* ──────────────────────────────────────────────────────────────────
 * MSR Registers
 * ────────────────────────────────────────────────────────────────── */

#define MSR_IA32_APIC_BASE     0x1B
#define MSR_IA32_STAR          0xC0000081
#define MSR_IA32_LSTAR         0xC0000082
#define MSR_IA32_CSTAR         0xC0000083
#define MSR_IA32_FMASK         0xC0000084
#define MSR_IA32_KERNEL_GS_BASE 0xC0000101

/* ──────────────────────────────────────────────────────────────────
 * TSS / IST
 * ────────────────────────────────────────────────────────────────── */

#define TSS_IST_COUNT          7
#define TSS_SIZE               104    /* 16-byte aligned, includes IO map base */

#define IST_EXCEPTION          1      /* IST for exceptions (DF, NMI, etc.) */
#define IST_NMI                2      /* IST for NMI */
#define IST_DEBUG              3      /* IST for debug */
#define IST_TIMER              4      /* IST for timer */

/* ──────────────────────────────────────────────────────────────────
 * IDT Constants
 * ────────────────────────────────────────────────────────────────── */

#define IDT_GATE_INT      0x8E    /* 64-bit Interrupt Gate: P=1, DPL=0, Type=0xE */
#define IDT_GATE_TRAP     0x8F    /* 64-bit Trap Gate: P=1, DPL=0, Type=0xF */
#define IDT_GATE_TASK     0x85    /* 64-bit Task Gate (legacy, not used) */

/* ──────────────────────────────────────────────────────────────────
 * Global State
 * ────────────────────────────────────────────────────────────────── */

typedef struct {
    void (*handler)(uint8_t irq, void *ctx);
    void *ctx;
} IRQEntry;

static IRQEntry g_irq_table[256];
static uint32_t g_pit_hz = 0;
static int      g_io_priv_ok = 0;
static int      g_idt_initialized = 0;

/* SIGSEGV handler for detecting I/O privilege in user space */
static void sigsegv_handler(int sig, siginfo_t *info, void *ucontext) {
    (void)sig; (void)info; (void)ucontext;
    g_io_priv_ok = 0;
}

/* ──────────────────────────────────────────────────────────────────
 * x86 I/O Port Access (bare metal only)
 * ────────────────────────────────────────────────────────────────── */

#ifdef MYSEED_METAL
static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" :: "a"(val), "Nd"(port) : "memory");
}

static inline uint8_t inb(uint16_t port) {
    uint8_t val;
    __asm__ volatile ("inb %1, %0" : "=a"(val) : "Nd"(port) : "memory");
    return val;
}

static inline void lidt(const IDTPtr *ptr) {
    __asm__ volatile ("lidt %0" :: "m"(*ptr) : "memory");
}
#endif

/* ──────────────────────────────────────────────────────────────────
 * IDT Gate Construction
 * ────────────────────────────────────────────────────────────────── */

static inline void idt_set_gate(IDTEntry *gate, uint64_t handler, uint16_t selector,
                                 uint8_t type_attr, uint8_t ist) {
    gate->offset_low   = (uint16_t)(handler & 0xFFFF);
    gate->selector     = selector;
    gate->ist          = ist;
    gate->type_attr    = type_attr;
    gate->offset_mid   = (uint16_t)((handler >> 16) & 0xFFFF);
    gate->offset_high  = (uint32_t)((handler >> 32) & 0xFFFFFFFF);
    gate->reserved     = 0;
}

/* ──────────────────────────────────────────────────────────────────
 * PIC Remapping
 * ────────────────────────────────────────────────────────────────── */

static void pic_remap(uint8_t offset1, uint8_t offset2) {
#ifdef MYSEED_METAL
    uint8_t mask1 = inb(PIC1_DATA);  /* Save masks */
    uint8_t mask2 = inb(PIC2_DATA);

    /* ICW1: Initialize PICs */
    outb(PIC1_CMD, ICW1_INIT | ICW1_ICW4);
    outb(PIC2_CMD, ICW1_INIT | ICW1_ICW4);

    /* ICW2: Remap IRQs to vectors offset1/offset2 */
    outb(PIC1_DATA, offset1);   /* Master: IRQ0-7 → vectors offset1..offset1+7 */
    outb(PIC2_DATA, offset2);   /* Slave:  IRQ8-15 → vectors offset2..offset2+7 */

    /* ICW3: Master IRQ2 connects to slave */
    outb(PIC1_DATA, 0x04);      /* Master: slave on IRQ2 */
    outb(PIC2_DATA, 0x02);      /* Slave:  cascade identity 2 */

    /* ICW4: 8086 mode */
    outb(PIC1_DATA, ICW4_8086);
    outb(PIC2_DATA, ICW4_8086);

    /* Restore masks */
    outb(PIC1_DATA, mask1);
    outb(PIC2_DATA, mask2);
#else
    (void)offset1; (void)offset2;
#endif
}

/* ──────────────────────────────────────────────────────────────────
 * ISR Handler Registration (C-level)
 * ────────────────────────────────────────────────────────────────── */

void interrupt_register(uint8_t irq, void (*handler)(uint8_t irq, void *ctx), void *ctx) {
    if (irq < 256) {
        g_irq_table[irq].handler = handler;
        g_irq_table[irq].ctx = ctx;
    }
}

void interrupt_unregister(uint8_t irq) {
    if (irq < 256) {
        g_irq_table[irq].handler = NULL;
        g_irq_table[irq].ctx = NULL;
    }
}

/* ──────────────────────────────────────────────────────────────────
 * PIC EOI
 * ────────────────────────────────────────────────────────────────── */

void interrupt_eoi(uint8_t irq) {
#ifdef MYSEED_METAL
    if (irq >= 40) {           /* IRQ8-15 (remapped to 40-47) */
        outb(PIC2_CMD, PIC_EOI);  /* Slave */
    }
    if (irq >= 32) {           /* IRQ0-7 (remapped to 32-39) */
        outb(PIC1_CMD, PIC_EOI);  /* Master */
    }
#else
    (void)irq;
#endif
}

/* ──────────────────────────────────────────────────────────────────
 * CLI/STI
 * ────────────────────────────────────────────────────────────────── */

void interrupt_disable(void) {
#ifdef MYSEED_METAL
    __asm__ volatile ("cli" ::: "memory");
#endif
}

void interrupt_enable(void) {
#ifdef MYSEED_METAL
    __asm__ volatile ("sti" ::: "memory");
#endif
}

/* ──────────────────────────────────────────────────────────────────
 * Simulated IRQ (Hosted Mode)
 * ────────────────────────────────────────────────────────────────── */

void interrupt_fire(uint8_t irq) {
    if (g_irq_table[irq].handler) {
        g_irq_table[irq].handler(irq, g_irq_table[irq].ctx);
    }
}

/* ──────────────────────────────────────────────────────────────────
 * IDT Pointer Accessor
 * ────────────────────────────────────────────────────────────────── */

const IDTPtr *interrupt_idt_ptr(void) {
    return &idt_ptr;
}

/* ──────────────────────────────────────────────────────────────────
 * Manual IDT Gate Setter (for syscall, etc.)
 * ────────────────────────────────────────────────────────────────── */

void interrupt_set_gate(uint8_t vector, uint64_t handler, uint16_t selector,
                         uint8_t type_attr, uint8_t ist) {
    if (vector < 256) {
        idt_set_gate(&idt_table[vector], handler, selector, type_attr, ist);
    }
}

/* ──────────────────────────────────────────────────────────────────
 * PIT Timer Handler (IRQ0 → vector 32 after remap)
 * ────────────────────────────────────────────────────────────────── */

static void pit_handler(uint8_t irq, void *ctx) {
    (void)irq; (void)ctx;
    task_timer_tick();
    interrupt_eoi(32);  /* IRQ0 remapped to vector 32 */
}

/* ──────────────────────────────────────────────────────────────────
 * PIT Initialization
 * ────────────────────────────────────────────────────────────────── */

int pit_init(uint32_t hz) {
#ifdef MYSEED_METAL
    if (hz == 0 || hz > 1193182) return -1;

    /* Install SIGSEGV handler to detect I/O privilege */
    struct sigaction sa = {0};
    sa.sa_sigaction = sigsegv_handler;
    sa.sa_flags = SA_SIGINFO;
    sigaction(SIGSEGV, &sa, NULL);

    g_io_priv_ok = 1;

    /* Try harmless port read */
    uint8_t test_val;
    __asm__ volatile (
        "inb %1, %0"
        : "=a"(test_val)
        : "Nd"(0x80)
        : "memory"
    );

    signal(SIGSEGV, SIG_DFL);

    if (!g_io_priv_ok) {
        return -1;  /* No I/O privilege */
    }

    g_pit_hz = hz;
    uint32_t divisor = 1193182 / hz;

    /* Configure PIT Channel 0: mode 3 (square wave), binary, lobyte/hibyte */
    outb(PIT_CMD, 0x36);
    outb(PIT_CH0_DATA, divisor & 0xFF);
    outb(PIT_CH0_DATA, (divisor >> 8) & 0xFF);

    /* Register IRQ0 handler (vector 32 after PIC remap) */
    interrupt_register(32, pit_handler, NULL);

    /* Enable IRQ0 on master PIC (unmask bit 0) */
    uint8_t master_mask = inb(PIC1_DATA);
    outb(PIC1_DATA, master_mask & ~0x01);

    return 0;
#else
    (void)hz;
    return -1;
#endif
}

void pit_shutdown(void) {
#ifdef MYSEED_METAL
    if (g_pit_hz == 0) return;

    /* Disable PIT */
    outb(PIT_CMD, 0x30);
    outb(PIT_CH0_DATA, 0xFF);

    /* Mask IRQ0 */
    uint8_t master_mask = inb(PIC1_DATA);
    outb(PIC1_DATA, master_mask | 0x01);

    interrupt_unregister(32);
    g_pit_hz = 0;
#else
    /* No-op in hosted mode */
#endif
}

/* ──────────────────────────────────────────────────────────────────
 * C-Level ISR Dispatcher
 * Called from common_isr_handler in isr_stubs.S
 * ────────────────────────────────────────────────────────────────── */

void isr_dispatch(uint8_t vector, struct InterruptFrame *frame) {
    (void)vector; (void)frame;

    /* Handle exceptions (0-31) */
    if (vector < 32) {
        /* For now, just log and halt on exceptions */
        /* In a real kernel: page fault handler, GPF handler, etc. */
        return;
    }

    /* Handle hardware IRQs (32-47 after PIC remap) */
    if (vector >= 32 && vector < 48) {
        /* Call registered handler if any */
        if (g_irq_table[vector].handler) {
            g_irq_table[vector].handler(vector, g_irq_table[vector].ctx);
        } else {
            /* Spurious or unhandled IRQ - send EOI anyway */
            interrupt_eoi(vector);
        }
        return;
    }

    /* Handle syscalls / software interrupts (48-255) */
    if (vector >= 48) {
        if (g_irq_table[vector].handler) {
            g_irq_table[vector].handler(vector, g_irq_table[vector].ctx);
        }
        return;
    }
}

/* ──────────────────────────────────────────────────────────────────
 * IDT Initialization
 * ────────────────────────────────────────────────────────────────── */

int interrupt_init(void) {
    /* Clear IRQ table */
    memset(g_irq_table, 0, sizeof(g_irq_table));
    g_idt_initialized = 0;

#ifdef MYSEED_METAL
    /* ──────────────────────────────────────────────────────────────
     * Bare-Metal: Install full IDT with assembly ISR stubs
     * ────────────────────────────────────────────────────────────── */

    /* Use isr_entries array to populate IDT */
    for (int i = 0; i < 256; i++) {
        uint64_t handler_addr = (uint64_t)isr_entries[i];
        uint8_t gate_type;

        if (i < 32) {
            /* Exceptions: interrupt gates */
            gate_type = IDT_GATE_INT;
        } else if (i < 48) {
            /* Hardware IRQs: interrupt gates */
            gate_type = IDT_GATE_INT;
        } else {
            /* Syscalls/software: trap gates */
            gate_type = IDT_GATE_TRAP;
        }

        idt_set_gate(&idt_table[i], handler_addr, 0x08, gate_type, 0);
    }

    /* ──────────────────────────────────────────────────────────────
     * Remap PIC: IRQ0-7 → 32-39, IRQ8-15 → 40-47
     * ────────────────────────────────────────────────────────────── */
    pic_remap(32, 40);

    /* ──────────────────────────────────────────────────────────────
     * Load IDT
     * ────────────────────────────────────────────────────────────── */
    lidt(&idt_ptr);
    g_idt_initialized = 1;

    return 0;
#else
    /* ──────────────────────────────────────────────────────────────
     * Hosted Mode: No real IDT, just initialize table for simulated IRQs
     * ────────────────────────────────────────────────────────────── */
    g_idt_initialized = 1;
    return 0;
#endif
}

void interrupt_shutdown(void) {
    memset(g_irq_table, 0, sizeof(g_irq_table));

#ifdef MYSEED_METAL
    /* Mask all PIC interrupts */
    outb(PIC1_DATA, 0xFF);
    outb(PIC2_DATA, 0xFF);
    g_idt_initialized = 0;
#endif
}

/* ──────────────────────────────────────────────────────────────────
 * MSR Access Helpers (bare metal only)
 * ────────────────────────────────────────────────────────────────── */

#ifdef MYSEED_METAL
static inline uint64_t rdmsr(uint32_t msr) {
    uint32_t lo, hi;
    __asm__ volatile ("rdmsr" : "=a"(lo), "=d"(hi) : "c"(msr));
    return ((uint64_t)hi << 32) | lo;
}

static inline void wrmsr(uint32_t msr, uint64_t val) {
    uint32_t lo = (uint32_t)val;
    uint32_t hi = (uint32_t)(val >> 32);
    __asm__ volatile ("wrmsr" :: "c"(msr), "a"(lo), "d"(hi) : "memory");
}
#endif

/* ──────────────────────────────────────────────────────────────────
 * LAPIC Access (memory-mapped)
 * ────────────────────────────────────────────────────────────────── */

static volatile uint32_t *g_lapic_base = NULL;
static volatile uint32_t *g_ioapic_base = NULL;
static uint32_t g_ioapic_irq_count = 0;

/* Read LAPIC register */
#ifdef MYSEED_METAL
static inline uint32_t lapic_read(uint32_t reg) {
    if (!g_lapic_base) return 0;
    return g_lapic_base[reg / 4];
}

static inline void lapic_write(uint32_t reg, uint32_t val) {
    if (!g_lapic_base) return;
    g_lapic_base[reg / 4] = val;
}

/* Read I/O APIC register (two-step: write index, read data) */
static inline uint32_t ioapic_read(uint32_t reg) {
    if (!g_ioapic_base) return 0;
    g_ioapic_base[IOAPIC_ID / 4] = reg;
    return g_ioapic_base[IOAPIC_VER / 4];  /* Data at offset 0x10/0x11 */
}

static inline void ioapic_write(uint32_t reg, uint32_t val) {
    if (!g_ioapic_base) return;
    g_ioapic_base[IOAPIC_ID / 4] = reg;
    g_ioapic_base[IOAPIC_VER / 4 + 1] = val;  /* Data register at offset 0x10/0x11 + 4 */
}
#endif

/* ──────────────────────────────────────────────────────────────────
 * TSS / IST Structures
 * ────────────────────────────────────────────────────────────────── */

typedef struct TSS {
    uint32_t reserved0;
    uint64_t rsp[3];
    uint64_t reserved1;
    uint64_t ist[TSS_IST_COUNT];
    uint64_t reserved2;
    uint16_t reserved3;
    uint16_t io_map_base;
} __attribute__((packed)) TSS;

static TSS g_tss = {0};
static uint8_t g_ist_stacks[4][8192] __attribute__((aligned(16)));  /* 8KB per IST stack */

/* ──────────────────────────────────────────────────────────────────
 * APIC Initialization
 * ────────────────────────────────────────────────────────────────── */

int apic_init(void) {
#ifdef MYSEED_METAL
    /* ──────────────────────────────────────────────────────────────
     * Enable LAPIC via MSR
     * ────────────────────────────────────────────────────────────── */
    uint64_t apic_base = rdmsr(MSR_IA32_APIC_BASE);
    apic_base |= 0x800;  /* Set EN bit (bit 11) */
    apic_base &= ~0xFFF; /* Clear base address bits */
    apic_base |= LAPIC_BASE_DEFAULT;
    wrmsr(MSR_IA32_APIC_BASE, apic_base);

    g_lapic_base = (volatile uint32_t *)LAPIC_BASE_DEFAULT;

    /* ──────────────────────────────────────────────────────────────
     * Configure LAPIC
     * ────────────────────────────────────────────────────────────── */

    /* Enable LAPIC: set SVR bit 8 */
    lapic_write(LAPIC_SVR, lapic_read(LAPIC_SVR) | LAPIC_SVR_ENABLE);

    /* Set LAPIC timer to one-shot mode initially (will be configured per-use) */
    lapic_write(LAPIC_LVT_TIMER, LAPIC_LVT_MASKED);
    lapic_write(LAPIC_TIMER_DIV, 0x3);  /* Divide by 16 */

    /* Mask LINT0, LINT1, ERROR LVT entries */
    lapic_write(LAPIC_LVT_LINT0, LAPIC_LVT_MASKED);
    lapic_write(LAPIC_LVT_LINT1, LAPIC_LVT_MASKED);
    lapic_write(LAPIC_LVT_ERROR, LAPIC_LVT_MASKED);

    /* ──────────────────────────────────────────────────────────────
     * Detect and configure I/O APIC
     * ────────────────────────────────────────────────────────────── */
    g_ioapic_base = (volatile uint32_t *)IOAPIC_BASE_DEFAULT;

    /* Read I/O APIC version to get number of IRQ entries */
    g_ioapic_base[IOAPIC_ID / 4] = IOAPIC_VER;
    uint32_t ioapic_ver = g_ioapic_base[IOAPIC_VER / 4 + 1];
    g_ioapic_irq_count = IOAPIC_RED_ENTRIES(ioapic_ver);

    /* Mask all I/O APIC interrupts initially */
    for (uint32_t i = 0; i < g_ioapic_irq_count; i++) {
        ioapic_write(IOAPIC_REDTBL + i * 2, IOAPIC_RED_MASKED);
        ioapic_write(IOAPIC_REDTBL_HIGH + i * 2, 0);
    }

    /* ──────────────────────────────────────────────────────────────
     * Setup TSS / IST for exception handlers
     * ────────────────────────────────────────────────────────────── */

    /* IST1: Exception stack (double fault, NMI, etc.) */
    g_tss.ist[IST_EXCEPTION - 1] = (uint64_t)(g_ist_stacks[0] + sizeof(g_ist_stacks[0]));
    /* IST2: NMI stack */
    g_tss.ist[IST_NMI - 1] = (uint64_t)(g_ist_stacks[1] + sizeof(g_ist_stacks[1]));
    /* IST3: Debug stack */
    g_tss.ist[IST_DEBUG - 1] = (uint64_t)(g_ist_stacks[2] + sizeof(g_ist_stacks[2]));
    /* IST4: Timer stack */
    g_tss.ist[IST_TIMER - 1] = (uint64_t)(g_ist_stacks[3] + sizeof(g_ist_stacks[3]));

    /* Load TSS (kernel will do ltr in GDT setup - this is a stub) */
    __asm__ volatile ("ltr %%ax" :: "a"((uint16_t)0x28) : "memory");

    return 0;
#else
    (void)g_lapic_base;
    (void)g_ioapic_base;
    (void)g_ioapic_irq_count;
    (void)g_tss;
    return -1;
#endif
}

/* ──────────────────────────────────────────────────────────────────
 * I/O APIC IRQ Routing
 * ────────────────────────────────────────────────────────────────── */

int ioapic_route_irq(uint8_t irq, uint8_t vector, uint8_t dest_apic_id) {
#ifdef MYSEED_METAL
    if (irq >= g_ioapic_irq_count) return -1;

    /* Redirection table entry: vector, delivery mode fixed, destination mode physical */
    uint32_t red_low = vector | IOAPIC_RED_DELIV_FIXED | IOAPIC_RED_DESTMODE_PHYS;
    uint32_t red_high = (uint32_t)dest_apic_id << 24;

    ioapic_write(IOAPIC_REDTBL + irq * 2, red_low);
    ioapic_write(IOAPIC_REDTBL_HIGH + irq * 2, red_high);

    return 0;
#else
    (void)irq; (void)vector; (void)dest_apic_id;
    return -1;
#endif
}

int ioapic_mask_irq(uint8_t irq) {
#ifdef MYSEED_METAL
    if (irq >= g_ioapic_irq_count) return -1;
    uint32_t red = ioapic_read(IOAPIC_REDTBL + irq * 2);
    ioapic_write(IOAPIC_REDTBL + irq * 2, red | IOAPIC_RED_MASKED);
    return 0;
#else
    (void)irq;
    return -1;
#endif
}

int ioapic_unmask_irq(uint8_t irq) {
#ifdef MYSEED_METAL
    if (irq >= g_ioapic_irq_count) return -1;
    uint32_t red = ioapic_read(IOAPIC_REDTBL + irq * 2);
    ioapic_write(IOAPIC_REDTBL + irq * 2, red & ~IOAPIC_RED_MASKED);
    return 0;
#else
    (void)irq;
    return -1;
#endif
}

/* ──────────────────────────────────────────────────────────────────
 * LAPIC Timer (for per-CPU timer interrupts)
 * ────────────────────────────────────────────────────────────────── */

int lapic_timer_init(uint32_t hz, uint8_t vector) {
#ifdef MYSEED_METAL
    if (!g_lapic_base) return -1;

    /* Mask timer LVT */
    lapic_write(LAPIC_LVT_TIMER, LAPIC_LVT_MASKED);

    /* Configure timer: periodic mode, vector */
    lapic_write(LAPIC_LVT_TIMER, vector | (0x2 << 17));  /* Periodic mode = bit 17 */
    lapic_write(LAPIC_TIMER_DIV, 0x3);  /* Divide by 16 */

    /* Calibrate using PIT (rough approximation) */
    /* For now, use a fixed initial count - real impl would calibrate */
    lapic_write(LAPIC_TIMER_INIT_CNT, 1000000 / hz);

    return 0;
#else
    (void)hz; (void)vector;
    return -1;
#endif
}

void lapic_eoi(void) {
#ifdef MYSEED_METAL
    if (g_lapic_base) {
        lapic_write(LAPIC_EOI, 0);
    }
#endif
}

/* ──────────────────────────────────────────────────────────────────
 * IPI (Inter-Processor Interrupts)
 * ────────────────────────────────────────────────────────────────── */

int lapic_send_ipi(uint32_t dest_apic_id, uint8_t vector, uint8_t delivery_mode) {
#ifdef MYSEED_METAL
    if (!g_lapic_base) return -1;

    uint32_t icr_high = dest_apic_id << 24;
    uint32_t icr_low = vector | delivery_mode | LAPIC_ICR_LEVEL_ASSERT;

    lapic_write(LAPIC_ICR_HIGH, icr_high);
    lapic_write(LAPIC_ICR_LOW, icr_low);

    /* Wait for delivery */
    while (lapic_read(LAPIC_ICR_LOW) & (1 << 12)) {
        /* Busy wait */
    }

    return 0;
#else
    (void)dest_apic_id; (void)vector; (void)delivery_mode;
    return -1;
#endif
}

int lapic_broadcast_ipi(uint8_t vector, uint8_t delivery_mode) {
#ifdef MYSEED_METAL
    if (!g_lapic_base) return -1;

    uint32_t icr_low = vector | delivery_mode | LAPIC_ICR_LEVEL_ASSERT | LAPIC_ICR_DEST_ALL_INC;
    uint32_t icr_high = 0;

    lapic_write(LAPIC_ICR_HIGH, icr_high);
    lapic_write(LAPIC_ICR_LOW, icr_low);

    while (lapic_read(LAPIC_ICR_LOW) & (1 << 12)) {
        /* Busy wait */
    }

    return 0;
#else
    (void)vector; (void)delivery_mode;
    return -1;
#endif
}

/* ──────────────────────────────────────────────────────────────────
 * SYSCALL/SYSRET Fast Path
 * ────────────────────────────────────────────────────────────────── */

extern void syscall_entry(void);  /* Defined in isr_stubs.S */

int syscall_init(void) {
#ifdef MYSEED_METAL
    /* STAR: bits 63:48 = user CS/SS, bits 47:32 = kernel CS/SS */
    uint64_t star = ((uint64_t)0x23 << 48) | ((uint64_t)0x2B << 48 + 16) |
                    ((uint64_t)0x08 << 32) | ((uint64_t)0x10 << 32 + 16);
    /* Simplified: kernel CS=0x08, kernel SS=0x10, user CS=0x23, user SS=0x2B */
    wrmsr(MSR_IA32_STAR, star);

    /* LSTAR: 64-bit syscall entry point */
    wrmsr(MSR_IA32_LSTAR, (uint64_t)syscall_entry);

    /* CSTAR: compat mode syscall entry (not used) */
    wrmsr(MSR_IA32_CSTAR, 0);

    /* FMASK: RFLAGS to clear on syscall (IF, TF, DF, RF, NT) */
    wrmsr(MSR_IA32_FMASK, 0x4700);  /* IF=0x200, TF=0x100, DF=0x400, RF=0x10000, NT=0x4000 */

    /* Enable SYSCALL/SYSRET in EFER */
    uint64_t efer = rdmsr(0xC0000080);
    efer |= 1;  /* SCE bit */
    wrmsr(0xC0000080, efer);

    return 0;
#else
    return -1;
#endif
}

/* ──────────────────────────────────────────────────────────────────
 * IRQ Routing Infrastructure (PCI/MSI)
 * ────────────────────────────────────────────────────────────────── */

typedef struct IRQRoute {
    uint8_t  src_irq;        /* Source IRQ (PCI pin, APIC line) */
    uint8_t  dst_vector;     /* Destination vector (32-255) */
    uint8_t  dest_apic_id;   /* Target APIC ID */
    uint16_t flags;          /* LEVEL/EDGE, ACTIVE_HIGH/LOW */
    struct IRQRoute *next;
} IRQRoute;

static IRQRoute *g_irq_routes = NULL;

int irq_route_add(uint8_t src_irq, uint8_t dst_vector, uint8_t dest_apic_id, uint16_t flags) {
#ifdef MYSEED_METAL
    IRQRoute *route = (IRQRoute *)mem_alloc(sizeof(IRQRoute));
    if (!route) return -1;

    route->src_irq = src_irq;
    route->dst_vector = dst_vector;
    route->dest_apic_id = dest_apic_id;
    route->flags = flags;
    route->next = g_irq_routes;
    g_irq_routes = route;

    /* Program I/O APIC if this is a legacy IRQ */
    if (src_irq < g_ioapic_irq_count) {
        ioapic_route_irq(src_irq, dst_vector, dest_apic_id);
    }

    return 0;
#else
    (void)src_irq; (void)dst_vector; (void)dest_apic_id; (void)flags;
    return -1;
#endif
}

int irq_route_remove(uint8_t src_irq) {
#ifdef MYSEED_METAL
    IRQRoute **pp = &g_irq_routes;
    while (*pp) {
        if ((*pp)->src_irq == src_irq) {
            IRQRoute *tmp = *pp;
            *pp = (*pp)->next;
            mem_free(tmp);
            return 0;
        }
        pp = &(*pp)->next;
    }
    return -1;
#else
    (void)src_irq;
    return -1;
#endif
}

/* ──────────────────────────────────────────────────────────────────
 * Interrupt / Exception Handlers with IST
 * ────────────────────────────────────────────────────────────────── */

/* Double fault handler (uses IST1) */
void handle_double_fault(InterruptFrame *frame) {
    (void)frame;
    /* In a real kernel: log fault, kill current task, panic */
    while (1) { __asm__ volatile ("hlt"); }
}

/* NMI handler (uses IST2) */
void handle_nmi(InterruptFrame *frame) {
    (void)frame;
    /* In a real kernel: log hardware error, attempt recovery */
    while (1) { __asm__ volatile ("hlt"); }
}

/* Page fault handler (uses IST1) */
void handle_page_fault(InterruptFrame *frame) {
    uint64_t cr2;
    __asm__ volatile ("mov %%cr2, %0" : "=r"(cr2));
    (void)frame; (void)cr2;
    /* In a real kernel: demand paging, COW, etc. */
    while (1) { __asm__ volatile ("hlt"); }
}

/* General protection fault handler (uses IST1) */
void handle_gpf(InterruptFrame *frame) {
    (void)frame;
    while (1) { __asm__ volatile ("hlt"); }
}

/* ──────────────────────────────────────────────────────────────────
 * Enhanced IDT Initialization with IST and SYSCALL
 * ────────────────────────────────────────────────────────────────── */

int interrupt_init_full(void) {
    if (interrupt_init() != 0) return -1;

#ifdef MYSEED_METAL
    /* Initialize APIC subsystem */
    if (apic_init() != 0) return -1;

    /* Initialize SYSCALL/SYSRET fast path */
    if (syscall_init() != 0) return -1;

    /* Initialize LAPIC timer at 100Hz on vector 0xF0 (240) */
    if (lapic_timer_init(100, 240) != 0) return -1;

    /* Set up exception handlers with IST */
    interrupt_set_gate(8,  (uint64_t)handle_double_fault, 0x08, IDT_GATE_INT, IST_EXCEPTION);  /* #DF */
    interrupt_set_gate(2,  (uint64_t)handle_nmi,            0x08, IDT_GATE_INT, IST_NMI);       /* NMI */
    interrupt_set_gate(14, (uint64_t)handle_page_fault,     0x08, IDT_GATE_INT, IST_EXCEPTION); /* #PF */
    interrupt_set_gate(13, (uint64_t)handle_gpf,            0x08, IDT_GATE_INT, IST_EXCEPTION); /* #GP */

    /* Reload IDT with updated gates */
    lidt(&idt_ptr);
#endif

    return 0;
}

/* ──────────────────────────────────────────────────────────────────
 * Timer Calibration (PIT vs HPET vs TSC)
 * ────────────────────────────────────────────────────────────────── */

uint64_t timer_calibrate_tsc(void) {
#ifdef MYSEED_METAL
    /* Read TSC, wait 10ms via PIT, read TSC again */
    uint64_t tsc_start, tsc_end;

    __asm__ volatile ("rdtsc" : "=a"(tsc_start), "=d"(tsc_end));
    tsc_start = ((uint64_t)tsc_end << 32) | tsc_start;

    /* Wait ~10ms using PIT */
    for (volatile int i = 0; i < 1000000; i++) { __asm__ volatile ("pause"); }

    __asm__ volatile ("rdtsc" : "=a"(tsc_start), "=d"(tsc_end));
    tsc_end = ((uint64_t)tsc_end << 32) | tsc_start;

    return tsc_end - tsc_start;  /* TSC ticks per ~10ms */
#else
    return 0;
#endif
}

int timer_init_deadline(uint64_t ns) {
#ifdef MYSEED_METAL
    if (!g_lapic_base) return -1;
    /* Set LAPIC timer to one-shot with deadline */
    lapic_write(LAPIC_LVT_TIMER, 240 | (0x0 << 17));  /* One-shot mode */
    lapic_write(LAPIC_TIMER_INIT_CNT, (uint32_t)(ns / 1000));  /* Rough */
    return 0;
#else
    (void)ns;
    return -1;
#endif
}

/* ──────────────────────────────────────────────────────────────────
 * Interrupt Statistics / Debug
 * ────────────────────────────────────────────────────────────────── */

static uint64_t g_irq_counts[256] = {0};

void interrupt_count(uint8_t irq) {
    if (irq < 256) g_irq_counts[irq]++;
}

uint64_t interrupt_get_count(uint8_t irq) {
    if (irq < 256) return g_irq_counts[irq];
    return 0;
}

/* ──────────────────────────────────────────────────────────────────
 * SYSCALL Dispatcher (C-level)
 * ────────────────────────────────────────────────────────────────── */

/* Syscall function pointer type */
typedef int64_t (*syscall_fn_t)(int64_t, int64_t, int64_t, int64_t, int64_t, int64_t);

/* Syscall table (up to 512 syscalls) */
#define MAX_SYSCALLS 512
static syscall_fn_t g_syscall_table[MAX_SYSCALLS] = {0};

/* Register a syscall handler */
int syscall_register(uint32_t num, syscall_fn_t handler) {
    if (num >= MAX_SYSCALLS) return -1;
    g_syscall_table[num] = handler;
    return 0;
}

/* Syscall dispatcher - called from syscall_entry assembly stub */
void syscall_handler(InterruptFrame *frame, uint64_t num) {
    (void)frame;
    
    if (num < MAX_SYSCALLS && g_syscall_table[num]) {
        /* Extract args from frame (RDI, RSI, RDX, R10, R8, R9) */
        int64_t arg1 = frame->rdi;
        int64_t arg2 = frame->rsi;
        int64_t arg3 = frame->rdx;
        int64_t arg4 = frame->r10;
        int64_t arg5 = frame->r8;
        int64_t arg6 = frame->r9;
        
        frame->rax = g_syscall_table[num](arg1, arg2, arg3, arg4, arg5, arg6);
    } else {
        frame->rax = -1;  /* ENOSYS */
    }
}