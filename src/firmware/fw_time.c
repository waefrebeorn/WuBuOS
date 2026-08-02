/*
 * fw_time.c  --  WuBuFW timing + CMOS RTC.
 *
 * TSC is calibrated against the PIT (channel 2 gate, no interrupts) so
 * Stall() is real microseconds rather than a spin guess.
 */

#include "fw.h"

static uint64_t g_tsc_per_us = 1000;   /* conservative default: 1GHz */

static uint8_t cmos_get(uint8_t reg) {
    outb(0x70, reg);
    io_wait();
    return inb(0x71);
}

static uint8_t bcd2bin(uint8_t v) { return (uint8_t)((v & 0x0F) + ((v >> 4) * 10)); }

void fw_time_init(void) {
    /* PIT channel 2: one-shot, count 0x4A9 ~= 1ms at 1.193182MHz. */
    const uint16_t ticks = 1193;              /* ~1.000 ms */
    uint8_t p61 = inb(0x61);
    outb(0x61, (uint8_t)((p61 & ~0x02) | 0x01));   /* gate on, speaker off */
    outb(0x43, 0xB0);                              /* ch2, lobyte/hibyte, mode 0 */
    outb(0x42, (uint8_t)(ticks & 0xFF));
    outb(0x42, (uint8_t)(ticks >> 8));

    uint64_t t0 = rdtsc();
    /* Wait for OUT2 (bit 5 of port 0x61) to go high = countdown finished. */
    uint64_t guard = 0;
    while (!(inb(0x61) & 0x20)) { if (++guard > 100000000ULL) break; }
    uint64_t t1 = rdtsc();
    outb(0x61, p61);

    uint64_t per_ms = t1 - t0;
    if (per_ms > 1000 && per_ms < 100000000ULL) g_tsc_per_us = per_ms / 1000;
    if (g_tsc_per_us == 0) g_tsc_per_us = 1000;
}

void fw_stall_us(uint64_t us) {
    uint64_t target = rdtsc() + us * g_tsc_per_us;
    while ((int64_t)(rdtsc() - target) < 0) __asm__ volatile("pause");
}

void fw_rtc_read(EFI_TIME *t) {
    if (!t) return;
    /* Wait out an update-in-progress, then read twice for consistency. */
    unsigned guard = 0;
    while ((cmos_get(0x0A) & 0x80) && ++guard < 1000000u) { }

    uint8_t sec, min, hour, day, mon, yr, statb;
    uint8_t s2, m2, h2, d2, mo2, y2;
    do {
        sec = cmos_get(0x00); min = cmos_get(0x02); hour = cmos_get(0x04);
        day = cmos_get(0x07); mon = cmos_get(0x08); yr = cmos_get(0x09);
        s2 = cmos_get(0x00); m2 = cmos_get(0x02); h2 = cmos_get(0x04);
        d2 = cmos_get(0x07); mo2 = cmos_get(0x08); y2 = cmos_get(0x09);
    } while (sec != s2 || min != m2 || hour != h2 || day != d2 || mon != mo2 || yr != y2);

    statb = cmos_get(0x0B);
    int pm = (!(statb & 0x02)) && (hour & 0x80);
    if (!(statb & 0x04)) {                       /* BCD mode */
        sec = bcd2bin(sec); min = bcd2bin(min);
        hour = bcd2bin((uint8_t)(hour & 0x7F));
        day = bcd2bin(day); mon = bcd2bin(mon); yr = bcd2bin(yr);
    } else {
        hour = (uint8_t)(hour & 0x7F);
    }
    if (pm && hour < 12) hour = (uint8_t)(hour + 12);
    if (!(statb & 0x02) && !pm && hour == 12) hour = 0;

    t->Year       = (UINT16)(2000 + yr);
    t->Month      = mon ? mon : 1;
    t->Day        = day ? day : 1;
    t->Hour       = hour;
    t->Minute     = min;
    t->Second     = sec;
    t->Pad1       = 0;
    t->Nanosecond = 0;
    t->TimeZone   = 0x07FF;      /* EFI_UNSPECIFIED_TIMEZONE */
    t->Daylight   = 0;
    t->Pad2       = 0;
}
