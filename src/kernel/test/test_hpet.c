/* test_hpet.c -- host tests for the HPET helpers (gap A19).
 * The MMIO probe is metal-only; the pure conversion + register offsets
 * are verified here. */
#include <stdio.h>
#include <stdint.h>

#include "wubu_hpet.h"

static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { \
    printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); failures++; \
} } while (0)

int main(void)
{
    printf("wubu_hpet tests (gap A19)\n");

    /* a 10 fs period: 1000 ticks = 10,000,000 fs = 10 ns */
    CHECK(hpet_ticks_to_ns(1000, 10000) == 10);
    /* 1 tick at 1 fs = 0 ns */
    CHECK(hpet_ticks_to_ns(1, 1) == 0);
    /* 1e6 ticks at 1e6 fs (1 ns per tick) = 1e6 ns */
    CHECK(hpet_ticks_to_ns(1000000, 1000000) == 1000000);
    /* monotonic: more ticks -> more ns */
    CHECK(hpet_ticks_to_ns(2000, 10000) == 20);

    /* the register offsets (the MMIO layout is fixed by the spec) */
    CHECK(WUBU_HPET_GCAP_ID == 0x00);
    CHECK(WUBU_HPET_GCFG == 0x10);
    CHECK(WUBU_HPET_MAIN_CNT == 0xF0);
    CHECK(WUBU_HPET_CNT_EN == 0x1u);

    if (failures == 0) printf("test_hpet: ALL PASS\n");
    else printf("test_hpet: %d FAILURES\n", failures);
    return failures ? 1 : 0;
}
