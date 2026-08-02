/*
 * wubu_serial.c  --  WuBuOS Serial Console (interrupt-driven RX, gap E2)
 *
 * COM1 RX via the UART data-ready interrupt. Wiring:
 *   - the 8250 IER bit 0 (RX data available) enabled
 *   - the IOAPIC redirection entry for pin 4 (COM1) -> vector 36
 *   - the 8259 PIC fully masked (APIC mode owns the IRQs)
 *   - vector 36's handler reads the data register + pushes the wubu_sync
 *     FIFO; the console pops it (with a safe poll backup).
 *
 * Freestanding, C11, no malloc.
 */

#include "wubu_serial.h"
#include "wubu_sync.h"

#define COM1_PORT 0x3F8
#define COM1_IER  0x3F9
#define COM1_LSR  0x3FD

#define WUBU_SERIAL_VECTOR 36   /* IRQ4 -> 32 + 4 */

static wubu_fifo_t g_rx_fifo;
static uint32_t    g_irq_count;
static volatile uint32_t g_ioapic_ok;

static inline uint8_t serial_in(uint16_t port)
{
    uint8_t v;
    __asm__ __volatile__("inb %w1, %0" : "=a"(v) : "Nd"(port));
    return v;
}
static inline void serial_out(uint16_t port, uint8_t v)
{
    __asm__ __volatile__("outb %0, %w1" : : "a"(v), "Nd"(port));
}

/* IOAPIC accessors (0xFEC00000, identity-mapped by wubu_apic). */
#define IOAPIC_BASE  0xFEC00000ul
#define IOAPIC_REGSEL 0x00
#define IOAPIC_IOWIN  0x10

static void ioapic_write(uint32_t reg, uint32_t val)
{
    volatile uint32_t *base = (volatile uint32_t *)IOAPIC_BASE;
    base[IOAPIC_REGSEL / 4] = reg;
    base[IOAPIC_IOWIN / 4] = val;
}

static void ioapic_route_irq4(void)
{
    /* Redirection entry for pin 4 (COM1/IRQ4):
     * bits 0-7  vector (36)
     * bits 8-10 delivery mode 0 (fixed)
     * bit 11    dest mode 0 (physical)
     * bits 12-13 polarity 0 (active high)
     * bit 14    trigger 0 (edge)
     * bit 16    mask 0 (unmasked)
     * bits 56-63 dest field 0 (CPU 0) */
    uint32_t entry_lo = WUBU_SERIAL_VECTOR | (0u << 8) | (0u << 16);
    uint32_t entry_hi = 0;   /* physical dest 0 */
    ioapic_write(0x10 + 2 * 4, entry_lo);
    ioapic_write(0x10 + 2 * 4 + 1, entry_hi);
    g_ioapic_ok = 1;
}

void wubu_serial_irq(void)
{
    g_irq_count++;
    /* drain all pending bytes into the FIFO */
    while (serial_in(COM1_LSR) & 0x01) {
        uint8_t c = serial_in(COM1_PORT);
        wubu_fifo_push(&g_rx_fifo, c);
    }
    /* LAPIC EOI */
    volatile uint32_t *lapic = (volatile uint32_t *)0xFEE00000ul;
    lapic[0xB0 / 4] = 0;
}

void wubu_serial_drain(void)
{
    /* backup poll: only when the IRQ path is quiet */
    while (serial_in(COM1_LSR) & 0x01) {
        uint8_t c = serial_in(COM1_PORT);
        wubu_fifo_push(&g_rx_fifo, c);
    }
}

int wubu_serial_pop(uint8_t *out)
{
    uint32_t v;
    if (wubu_fifo_pop(&g_rx_fifo, &v) == 0) {
        if (out) *out = (uint8_t)v;
        return 0;
    }
    return -1;
}

void wubu_serial_init(void)
{
    wubu_fifo_init(&g_rx_fifo, WUBU_FIFO_N);
    g_irq_count = 0;
    g_ioapic_ok = 0;

    /* UART: enable the RX-data-available interrupt */
    serial_out(COM1_IER, (uint8_t)(serial_in(COM1_IER) | 0x01));

    /* Mask the 8259 PIC fully (the APIC owns the IRQs now) */
    serial_out(0x21, 0xFF);
    serial_out(0xA1, 0xFF);

    /* Route IRQ4 -> vector 36 via the IOAPIC */
    ioapic_route_irq4();

    /* Register the handler on vector 36 */
    extern void interrupt_register(uint8_t, void (*)(uint8_t, void *), void *);
    interrupt_register(WUBU_SERIAL_VECTOR, (void (*)(uint8_t, void *))
                       &wubu_serial_irq, 0);
}

uint32_t wubu_serial_fifo_count(void) { return wubu_fifo_count(&g_rx_fifo); }
uint32_t wubu_serial_irq_count(void)  { return g_irq_count; }
