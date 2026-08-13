/*
 * test_rtc.c -- host test for wubu_rtc's pure conversion helpers
 * (the CMOS port reads are metal-only; the BCD + 12h conversions are
 * the host-testable contract).
 */
#include "wubu_rtc.h"

#include <stdio.h>

static int fails;

static void check_u8(const char *what, uint8_t got, uint8_t want)
{
    if (got != want) {
        printf("FAIL: %s = %u (want %u)\n", what, (unsigned)got, (unsigned)want);
        fails++;
    }
}

int main(void)
{
    /* BCD -> binary */
    check_u8("bcd 00", wubu_rtc_bcd_to_bin(0x00), 0);
    check_u8("bcd 07", wubu_rtc_bcd_to_bin(0x07), 7);
    check_u8("bcd 59", wubu_rtc_bcd_to_bin(0x59), 59);
    check_u8("bcd 12", wubu_rtc_bcd_to_bin(0x12), 12);
    check_u8("bcd 99", wubu_rtc_bcd_to_bin(0x99), 99);

    /* 12h -> 24h (bit 7 = PM, the low bits are the BCD hour) */
    check_u8("12h 12am",  wubu_rtc_12h_to_24h(0x12), 0);   /* 12 AM */
    check_u8("12h 11am",  wubu_rtc_12h_to_24h(0x11), 11);
    check_u8("12h 1pm",   wubu_rtc_12h_to_24h(0x81), 13);  /* PM */
    check_u8("12h 12pm",  wubu_rtc_12h_to_24h(0x92), 12);  /* 12 PM */
    check_u8("12h 11pm",  wubu_rtc_12h_to_24h(0x91), 23);  /* 11 PM */
    check_u8("12h 2pm",   wubu_rtc_12h_to_24h(0x82), 14);
    check_u8("12h 9am",   wubu_rtc_12h_to_24h(0x09), 9);
    check_u8("12h 10pm",  wubu_rtc_12h_to_24h(0x90), 22);

    if (fails == 0) {
        printf("ALL RTC TESTS PASSED\n");
        return 0;
    }
    printf("RTC FAILURES: %d\n", fails);
    return 1;
}
