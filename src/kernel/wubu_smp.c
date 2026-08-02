/*
 * wubu_smp.c  --  SMP: application-processor bring-up (gap I2)
 *
 * AP protocol (Intel SDM): write 0x000C4500 (INIT) to the local APIC
 * ICR, wait 10ms, then two SIPIs (0x000C4608 = SIPI to vector 8 at
 * 0x8000). The trampoline at 0x8000 is 16-bit code that enables the
 * APIC, jumps to 64-bit long mode, and parks bumping its alive counter.
 *
 * The AP's long-mode entry needs the GDT + paging already on; the BSP's
 * tables are shared (the identity + higher-half map). The trampoline is
 * position-independent 16/64-bit asm built by hand.
 */
#include "wubu_smp.h"

#define LAPIC_BASE      0xFEE00000ull
#define LAPIC_ICR_LOW   0x300u
#define LAPIC_ICR_HIGH  0x310u
#define LAPIC_ID        0x020u

/* the AP trampoline (16-bit, copied to 0x8000) + its size */
extern const uint8_t wubu_smp_tramp_start[];
extern const uint8_t wubu_smp_tramp_end[];
#define WUBU_SMP_TRAMP_ADDR 0x8000u

/* the shared control block at 0x7000 (below the trampoline) */
typedef struct {
    uint32_t magic;          /* 0x534D5031 "SMP1" */
    uint32_t cpu_count;      /* BSP writes; the APs read */
    volatile uint32_t alive[WUBU_SMP_MAX_CPU];  /* APs bump their slot */
    uint64_t gdtr_base;      /* the BSP's GDT (for the AP's lgdt) */
    uint64_t cr3_value;      /* the BSP's page tables */
} wubu_smp_ctl_t;

#define WUBU_SMP_CTL_ADDR 0x7000u

static wubu_smp_ctl_t *g_ctl = (wubu_smp_ctl_t *)WUBU_SMP_CTL_ADDR;
static uint32_t g_detected = 1;
static uint32_t g_cpu_count = 1;

static uint32_t lapic_read(uint32_t off)
{
    return *(volatile uint32_t *)(uintptr_t)(LAPIC_BASE + off);
}

static void lapic_write(uint32_t off, uint32_t v)
{
    *(volatile uint32_t *)(uintptr_t)(LAPIC_BASE + off) = v;
}

static void delay_ms(uint32_t ms)
{
    volatile uint32_t n = ms * 10000;
    while (n--) __asm__ __volatile__("pause");
}

uint32_t wubu_smp_detect(void)
{
    /* the LAPIC ID of the BSP; the MP table / ACPI MADT would list the
     * others -- the LAPIC ID register is the observable identity. */
    uint32_t id = lapic_read(LAPIC_ID) >> 24;
    (void)id;
    g_detected = 1;    /* at least the BSP; MADT enumeration is follow-on */
    return g_detected;
}

uint32_t wubu_smp_cpu_count(void) { return g_cpu_count; }

uint32_t wubu_smp_alive(uint32_t cpu)
{
    if (cpu >= WUBU_SMP_MAX_CPU) return 0;
    return g_ctl->alive[cpu];
}

uint32_t wubu_smp_start_aps(void)
{
    /* publish the control block + copy the trampoline */
    g_ctl->magic = 0x534D5031u;
    g_ctl->cpu_count = 1;
    for (int i = 0; i < WUBU_SMP_MAX_CPU; i++) g_ctl->alive[i] = 0;
    g_ctl->cr3_value = 0;
    __asm__ __volatile__("mov %%cr3, %0" : "=r"(g_ctl->cr3_value));

    /* the BSP's GDT as a 6-byte GDTR (limit + physical base) for the
     * AP's 16-bit lgdt: the kernel's GDT VA is 0xffffffff80xxxxxx, its
     * physical base is the low 32 bits minus 0x80000000. */
    uint8_t gdtr[10];
    __asm__ __volatile__("sgdt %0" : "=m"(gdtr) :: "memory");
    uint16_t limit = (uint16_t)(gdtr[0] | (gdtr[1] << 8));
    uint32_t base = (uint32_t)(gdtr[2] | (gdtr[3] << 8) |
                               (gdtr[4] << 16) | (gdtr[5] << 24));
    base -= 0x80000000u;                       /* VA -> physical */
    uint8_t *g = (uint8_t *)&g_ctl->gdtr_base;
    g[0] = (uint8_t)(limit & 0xFF);
    g[1] = (uint8_t)(limit >> 8);
    g[2] = (uint8_t)(base & 0xFF);
    g[3] = (uint8_t)((base >> 8) & 0xFF);
    g[4] = (uint8_t)((base >> 16) & 0xFF);
    g[5] = (uint8_t)((base >> 24) & 0xFF);

    size_t sz = (size_t)(wubu_smp_tramp_end - wubu_smp_tramp_start);
    if (sz > 4096) return 0;
    for (size_t i = 0; i < sz; i++) {
        ((uint8_t *)WUBU_SMP_TRAMP_ADDR)[i] = wubu_smp_tramp_start[i];
    }

    /* INIT: vector 0, assert */
    lapic_write(LAPIC_ICR_HIGH, 0);
    lapic_write(LAPIC_ICR_LOW, 0x000C4500u);
    delay_ms(10);

    /* two SIPIs to vector 8 (0x8000) */
    lapic_write(LAPIC_ICR_HIGH, 0);
    lapic_write(LAPIC_ICR_LOW, 0x000C4608u);
    delay_ms(1);
    lapic_write(LAPIC_ICR_HIGH, 0);
    lapic_write(LAPIC_ICR_LOW, 0x000C4608u);

    /* observe the APs for up to ~100 ms */
    for (uint32_t t = 0; t < 100; t++) {
        uint32_t n = 1;
        for (uint32_t c = 1; c < WUBU_SMP_MAX_CPU; c++) {
            if (g_ctl->alive[c]) n++;
        }
        g_cpu_count = n;
        if (n > 1) break;
        delay_ms(1);
    }
    return g_cpu_count - 1;
}
