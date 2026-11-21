/* wubu_wdt_logic.c -- pure, host-testable watchdog deadline arithmetic.
 *
 * No port I/O here.  The metal owner reloads a short 8254 window; this
 * module counts how many ticks make up the REAL deadline.  Keeping the
 * arithmetic in one owner lets the host gate prove the 2 s watchdog is not
 * secretly a 55 ms watchdog (the old code clamped the 16-bit count).
 */
#include "wubu_wdt.h"

uint32_t wdt_deadline_ticks(uint32_t ms, uint32_t tick_ms)
{
    if (tick_ms == 0) return 1;
    uint64_t ticks = ((uint64_t)ms + tick_ms - 1u) / tick_ms;
    if (ticks == 0) ticks = 1;
    if (ticks > 0xffffffffull) ticks = 0xffffffffull;
    return (uint32_t)ticks;
}

void wdt_logic_feed(uint32_t *remaining, uint32_t ms, uint32_t tick_ms)
{
    if (!remaining) return;
    *remaining = wdt_deadline_ticks(ms, tick_ms);
}

int wdt_logic_tick(uint32_t *remaining)
{
    if (!remaining || *remaining == 0) return 1;
    (*remaining)--;
    return *remaining == 0;
}
