/*
 * wubu_rtc.c -- CMOS RTC wall clock (gap A17), freestanding C11.
 *
 * Self-contained: the MC146818 RTC through ports 0x70/0x71. The NMI
 * disable bit (0x80) is set while selecting registers, the Update-In-
 * Progress flag is waited out, and BCD/binary + 12h/24h are normalized
 * to a 24h binary tm. The century register (0x32) is read when present;
 * otherwise the year is assumed 2000+ (the 21st century).
 */
#include "wubu_rtc.h"

#define RTC_PORT_IDX 0x70
#define RTC_PORT_DAT 0x71

#define RTC_SEC   0x00
#define RTC_MIN   0x02
#define RTC_HOUR  0x04
#define RTC_DAY   0x07
#define RTC_MON   0x08
#define RTC_YEAR  0x09
#define RTC_STAT_A 0x0A
#define RTC_STAT_B 0x0B
#define RTC_CENT  0x32

#define RTC_UIP   0x80      /* status A: update in progress */
#define RTC_B_BIN 0x04      /* status B: binary (vs BCD) mode */
#define RTC_B_24H 0x02      /* status B: 24-hour mode */

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ __volatile__("outb %0, %1" :: "a"(val), "Nd"(port));
}
static inline uint8_t inb(uint16_t port) {
    uint8_t v;
    __asm__ __volatile__("inb %1, %0" : "=a"(v) : "Nd"(port));
    return v;
}

static uint8_t rtc_read_reg(uint8_t reg)
{
    outb(RTC_PORT_IDX, (uint8_t)(reg | 0x80));   /* NMI off */
    return inb(RTC_PORT_DAT);
}

uint8_t wubu_rtc_bcd_to_bin(uint8_t bcd)
{
    return (uint8_t)((bcd & 0x0F) + ((bcd >> 4) * 10));
}

uint8_t wubu_rtc_12h_to_24h(uint8_t reg)
{
    int pm = (reg & 0x80) != 0;
    uint8_t h = wubu_rtc_bcd_to_bin((uint8_t)(reg & 0x7F));
    if (pm) {
        if (h < 12) h = (uint8_t)(h + 12);
    } else if (h == 12) {
        h = 0;                     /* 12 AM */
    }
    return h;
}

int wubu_rtc_read(wubu_rtc_tm *tm)
{
    if (!tm) return -1;

    /* Wait for an update to settle (the UIP bit in status A). */
    for (int i = 0; i < 100000; i++) {
        uint8_t a = rtc_read_reg(RTC_STAT_A);
        if (!(a & RTC_UIP)) break;
        __asm__ __volatile__("pause");
    }

    uint8_t status_b = rtc_read_reg(RTC_STAT_B);
    int binary = (status_b & RTC_B_BIN) != 0;

    uint8_t sec   = rtc_read_reg(RTC_SEC);
    uint8_t min   = rtc_read_reg(RTC_MIN);
    uint8_t raw_h = rtc_read_reg(RTC_HOUR);
    uint8_t day   = rtc_read_reg(RTC_DAY);
    uint8_t mon   = rtc_read_reg(RTC_MON);
    uint8_t year  = rtc_read_reg(RTC_YEAR);
    uint8_t cent  = rtc_read_reg(RTC_CENT);

    /* 12h -> 24h: the PM bit (bit 7) + BCD are handled inside the
     * helper; only the mode matters here. */
    uint8_t hour;
    if (status_b & RTC_B_24H) {
        hour = raw_h;
        if (!binary) hour = wubu_rtc_bcd_to_bin(hour);
    } else {
        hour = wubu_rtc_12h_to_24h(raw_h);
    }

    if (!binary) {
        sec  = wubu_rtc_bcd_to_bin(sec);
        min  = wubu_rtc_bcd_to_bin(min);
        day  = wubu_rtc_bcd_to_bin(day);
        mon  = wubu_rtc_bcd_to_bin(mon);
        year = wubu_rtc_bcd_to_bin(year);
        if (cent != 0xFF) cent = wubu_rtc_bcd_to_bin(cent);
    }

    /* Normalize: hour sanity, month/day ranges, and the year. */
    if (hour > 23) hour = 0;                   /* garbage guard */
    if (mon < 1 || mon > 12) mon = 1;
    if (day < 1 || day > 31) day = 1;

    uint16_t full_year;
    if (cent != 0xFF && cent != 0)
        full_year = (uint16_t)(cent * 100 + year);
    else if (year >= 69)
        full_year = (uint16_t)(1900 + year);   /* 1970-1999 */
    else
        full_year = (uint16_t)(2000 + year);   /* 2000-2069 */

    tm->sec  = sec;
    tm->min  = min;
    tm->hour = hour;
    tm->day  = day;
    tm->mon  = mon;
    tm->year = full_year;
    return 0;
}
