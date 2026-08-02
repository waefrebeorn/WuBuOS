/*
 * wubu_rtc.h -- WuBuOS kernel-side CMOS RTC wall clock (gap A17).
 *
 * Freestanding C11: no malloc, no hosted APIs. Reads the RTC through
 * the CMOS ports (0x70/0x71); the BCD/binary + 24h conversions are
 * pure functions (host-testable).
 */
#ifndef WUBU_RTC_H
#define WUBU_RTC_H

#include <stdint.h>

typedef struct wubu_rtc_tm {
    uint8_t  sec;      /* 0-59 */
    uint8_t  min;      /* 0-59 */
    uint8_t  hour;     /* 0-23 (24h always) */
    uint8_t  day;      /* 1-31 */
    uint8_t  mon;      /* 1-12 */
    uint16_t year;     /* full year, e.g. 2026 */
} wubu_rtc_tm;

/* Read the current wall clock. Returns 0 on success, -1 if the RTC is
 * not responding (e.g. no CMOS on the host). */
int wubu_rtc_read(wubu_rtc_tm *tm);

/* Pure conversion helpers (host-testable). */
uint8_t wubu_rtc_bcd_to_bin(uint8_t bcd);
/* Convert a RAW 12-hour-mode hour register (BCD low bits + bit 7 = PM)
 * to a 24-hour binary hour. */
uint8_t wubu_rtc_12h_to_24h(uint8_t reg);

#endif /* WUBU_RTC_H */
