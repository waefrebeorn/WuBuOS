/*
 * WuBuOS -- extracted module (auto-split, C11, opaque-safe)
 */

#include "interrupt.h"
#include "interrupt_apic.h"
#include "tasking.h"
#include "memory.h"
#include "interrupt_io.h"
#include <stdint.h>
#include <string.h>
#include <signal.h>

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
