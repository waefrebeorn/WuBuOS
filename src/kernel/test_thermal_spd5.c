/* test_thermal_spd5.c -- DDR5 SPD5-hub temperature decoder vector gate.
 *
 * Vectors follow the ABLIC S34HTS08AB datasheet (MR36 tables 101-105) and the
 * public Bus Pirate DDR5 SMBus walkthrough.  The old thermal reader had no
 * decoder at all, so this gate cannot link against it: a missing sensor path
 * is a real failure, not a vacuous pass.
 */
#include <stdio.h>
#include "wubu_thermal_read.h"

static int failures;
#define CHECK(got, want, msg) do {                                      \
    int g_ = (got), w_ = (want);                                        \
    if (g_ != w_) { printf("  FAIL: %s (got %d, want %d)\n", msg, g_, w_); \
                    failures++; }                                       \
    else printf("  ok: %s (%d C)\n", msg, g_);                           \
} while (0)

int main(void)
{
    printf("=== DDR5 SPD5 temperature decode ===\n");

    /* Raw MR49/MR50 = 0x0154 (the published DDR5 SMBus example): 21 C both
     * at 9-bit/0.5 C and 10-bit/0.25 C resolution. */
    CHECK(wubu_thermal_spd5_decode(0x00, 0x54, 0x01), 21,
          "9-bit resolution, raw 0x0154");
    CHECK(wubu_thermal_spd5_decode(0x01, 0x54, 0x01), 21,
          "10-bit resolution, raw 0x0154");

    /* Two's-complement sign extension: 0x7FC << 2 = -4 units = -1 C. */
    CHECK(wubu_thermal_spd5_decode(0x01, 0xF0, 0x1F), -1,
          "10-bit negative temperature");
    /* 12-bit: 75 C at 0.0625 C/LSB -> 1200 units -> raw 0x04B0. */
    CHECK(wubu_thermal_spd5_decode(0x03, 0xB0, 0x04), 75,
          "12-bit resolution, 75 C");

    /* Fail closed: unmodeled resolution and out-of-datasheet readings. */
    CHECK(wubu_thermal_spd5_decode(0x04, 0x54, 0x01), -1,
          "unmodeled MR36 resolution");
    CHECK(wubu_thermal_spd5_decode(0x01, 0x20, 0x08), -1,
          "130 C reading rejected (outside datasheet range)");

    printf("=== %s: %d failure(s) ===\n",
           failures ? "SPD5 DECODE GATE FAILED" : "SPD5 DECODE GATE PASS",
           failures);
    return failures ? 1 : 0;
}
