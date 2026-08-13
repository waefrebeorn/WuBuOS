/* test_wdt.c -- host tests for the 8254 watchdog helpers (gap E7).
 * The port I/O is metal-only; the pure conversion logic + the mode word
 * are verified here. */
#include <stdio.h>
#include <stdint.h>

#include "wubu_wdt.h"

static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { \
    printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); failures++; \
} } while (0)

int main(void)
{
    printf("wubu_wdt tests (gap E7)\n");

    /* the count for 1ms: 1193.18 -> 1193 */
    CHECK(wdt_ms_to_count(1) == 1193);
    /* 1000ms: 1193180 -> clamped to 65535 */
    CHECK(wdt_ms_to_count(1000) == 65535);
    /* 0ms -> 1 (never zero: a zero count is a 65536-tick window) */
    CHECK(wdt_ms_to_count(0) == 1);
    /* monotonicity of the short range */
    CHECK(wdt_ms_to_count(2) > wdt_ms_to_count(1));
    CHECK(wdt_ms_to_count(1) == (uint16_t)(1193180ull / 1000ull));

    /* the mode word: ch2 (10), LSB/MSB (11), mode 0, binary = 0xB0 */
    CHECK(WUBU_WDT_MODE0 == 0xB0);

    if (failures == 0) printf("test_wdt: ALL PASS\n");
    else printf("test_wdt: %d FAILURES\n", failures);
    return failures ? 1 : 0;
}
