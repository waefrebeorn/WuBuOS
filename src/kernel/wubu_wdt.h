/*
 * wubu_wdt.h  --  8254 channel-2 watchdog timer (freestanding)
 *
 * Gap E7: the kernel's only time source was the PIT/LAPIC; the 8254's
 * channel 2 (the classic PC watchdog channel) now doubles as a hardware
 * watchdog. The timer tick FEEDS it; if the kernel ever stalls (task
 * watchdog dead, IRQs masked), the countdown expires and the panic path
 * fires with the A7 ring intact.
 *
 * The 8254 ch2: port 0x43 (mode word), 0x42 (count). Its OUT line is
 * readable via port 0x61 bit 5 (the classic speaker-channel status).
 * The mode word: ch2 + LSB/MSB + mode 0 (one-shot) + binary.
 */
#ifndef WUBU_WDT_H
#define WUBU_WDT_H

#include <stdint.h>

/* 8254 registers */
#define WUBU_WDT_CMD   0x43
#define WUBU_WDT_CH2   0x42
#define WUBU_WDT_STATUS 0x61

/* Mode word: ch2 (bits 7-6 = 10), LSB then MSB (bits 5-4 = 11),
 * mode 0 (bits 3-1 = 000), binary (bit 0 = 0). */
#define WUBU_WDT_MODE0  0xB0

/* Convert a timeout in milliseconds to the 8254 count (PIT base
 * 1,193,180 Hz). Pure helper -- host-testable. */
static inline uint16_t wdt_ms_to_count(uint32_t ms)
{
    uint64_t ticks = (uint64_t)ms * 1193180ull / 1000ull;
    if (ticks == 0) ticks = 1;
    if (ticks > 65535) ticks = 65535;
    return (uint16_t)ticks;
}

/* Hardware window: the 8254 is only a 16-bit liveness timer (~55 ms max).
 * The REAL deadline is counted in ticks by wubu_wdt_logic.c. */
#define WUBU_WDT_HW_WINDOW_MS 50u

/* Pure deadline arithmetic (one owner, host-testable). */
uint32_t wdt_deadline_ticks(uint32_t ms, uint32_t tick_ms);
void wdt_logic_feed(uint32_t *remaining, uint32_t ms, uint32_t tick_ms);
int wdt_logic_tick(uint32_t *remaining);

/* Set the tick period in ms (default 1). 0 is refused. */
void wdt_set_tick_ms(uint32_t tick_ms);

/* Advance the software deadline from the timer ISR. */
void wdt_tick(void);

/* Arm the watchdog (one-shot, ms timeout). 0 on success. */
int wdt_arm(uint32_t ms);

/* Feed (re-arm from the timer tick). */
void wdt_feed(uint32_t ms);

/* Non-zero when the countdown has expired (OUT line latched). */
int wdt_expired(void);

/* Disarm (the clean shutdown). */
void wdt_disarm(void);

#endif
