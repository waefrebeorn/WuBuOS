/*
 * wubu_wdt.c -- vigilant, non-mad watchdog.
 *
 * The old code programmed the 8254 with wdt_ms_to_count(2000), which clamps
 * to 65535 ticks (~55 ms). A 2 second request therefore fired in 55 ms and
 * had to be re-fed every tick. That is not a watchdog, it is a tripwire.
 *
 * New model:
 *   - hardware: a bounded 50 ms 8254 liveness window, reloaded by each tick;
 *     if the tick stops, its OUT line latches.
 *   - software: a REAL deadline counted in ticks by wubu_wdt_logic.c.
 *   - expiry: either the software deadline elapsed or the hardware OUT
 *     latched. Nothing touches ACPI or /dev/watchdog, so no BIOS reset loops.
 * Freestanding C11, plain port I/O.
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

static uint32_t g_tick_ms = 1u;
static uint32_t g_deadline_ms = 0u;
static uint32_t g_remaining = 0u;
static int g_armed = 0;
static int g_hw_expired = 0;

static void ch2_load(uint16_t count)
{
    outb(WUBU_WDT_CMD, WUBU_WDT_MODE0);
    outb(WUBU_WDT_CH2, (uint8_t)(count & 0xffu));
    outb(WUBU_WDT_CH2, (uint8_t)(count >> 8));
    uint8_t st = inb(WUBU_WDT_STATUS);
    outb(WUBU_WDT_STATUS, (uint8_t)(st | 0x01u));
}

void wdt_set_tick_ms(uint32_t tick_ms)
{
    if (tick_ms) g_tick_ms = tick_ms;
}

static void reload_hw(void)
{
    ch2_load(wdt_ms_to_count(WUBU_WDT_HW_WINDOW_MS));
}

int wdt_arm(uint32_t ms)
{
    if (!ms) return -1;
    g_deadline_ms = ms;
    g_hw_expired = 0;
    g_armed = 1;
    wdt_logic_feed(&g_remaining, ms, g_tick_ms);
    reload_hw();
    return 0;
}

void wdt_feed(uint32_t ms)
{
    if (!g_armed) return;
    if (ms) g_deadline_ms = ms;
    g_hw_expired = 0;
    wdt_logic_feed(&g_remaining, g_deadline_ms, g_tick_ms);
    reload_hw();
}

void wdt_tick(void)
{
    if (!g_armed) return;
    /* The tick ISR reloads the short hardware liveness window; it does NOT
     * feed the real deadline (that would make a stall invisible). */
    reload_hw();
    if (wdt_logic_tick(&g_remaining)) g_hw_expired = 1;
}

int wdt_expired(void)
{
    if (!g_armed) return 0;
    return (g_hw_expired || (inb(WUBU_WDT_STATUS) & 0x20u)) ? 1 : 0;
}

void wdt_disarm(void)
{
    g_armed = 0;
    g_remaining = 0;
    g_hw_expired = 0;
    uint8_t st = inb(WUBU_WDT_STATUS);
    outb(WUBU_WDT_STATUS, (uint8_t)(st & ~0x01u));
}
