/*
 * wubu_wdt.c  --  8254 channel-2 watchdog timer (gap E7)
 *
 * The 8254 ch2 runs in mode 0 (one-shot): the OUT line goes high when
 * the count reaches zero. The timer tick feeds (re-arms) it; the panic
 * path checks wdt_expired() so a stalled kernel is caught by hardware,
 * not just by the task watchdog (A4) or the supervisor watchdog (D7).
 * Freestanding C11, opaque-free (plain port I/O).
 */
#include "wubu_wdt.h"

static inline void outb(uint16_t port, uint8_t v)
{
    __asm__ __volatile__("outb %0, %w1" : : "a"(v), "Nd"(port));
}
static inline uint8_t inb(uint16_t port)
{
    uint8_t v;
    __asm__ __volatile__("inb %w1, %0" : "=a"(v) : "Nd"(port));
    return v;
}

static void ch2_load(uint16_t count)
{
    outb(WUBU_WDT_CMD, WUBU_WDT_MODE0);      /* ch2, LSB/MSB, mode 0 */
    outb(WUBU_WDT_CH2, (uint8_t)(count & 0xFF));
    outb(WUBU_WDT_CH2, (uint8_t)(count >> 8));
    /* raise the ch2 gate (0x61 bit 0) so the counter runs */
    uint8_t st = inb(WUBU_WDT_STATUS);
    outb(WUBU_WDT_STATUS, (uint8_t)(st | 0x01));
}

int wdt_arm(uint32_t ms)
{
    ch2_load(wdt_ms_to_count(ms));
    return 0;
}

void wdt_feed(uint32_t ms)
{
    ch2_load(wdt_ms_to_count(ms));
}

int wdt_expired(void)
{
    /* ch2 OUT is 0x61 bit 5; when the one-shot fired it is high. */
    return (inb(WUBU_WDT_STATUS) & 0x20) ? 1 : 0;
}

void wdt_disarm(void)
{
    /* clear the ch2 gate -> the counter stops (no timeout can fire) */
    uint8_t st = inb(WUBU_WDT_STATUS);
    outb(WUBU_WDT_STATUS, (uint8_t)(st & ~0x01u));
}
