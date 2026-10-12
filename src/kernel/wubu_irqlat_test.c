/*
 * wubu_irqlat_test.c -- per-IRQ interrupt-latency tracker tests (P0 #6).
 */
#include "wubu_irqlat.h"
#include <stdio.h>
#include <string.h>

static int failures = 0;
#define CHECK(cond, msg) do { if (!(cond)) { printf("FAIL: %s\n", msg); failures++; } } while (0)

int main(void) {
    wubu_irqlat_reset();

    /* Record 100 samples for IRQ 8: values 1..100. p99 ~= 99, max=100. */
    for (int i = 1; i <= 100; i++) wubu_irqlat_record(8, (uint32_t)i);
    CHECK(wubu_irqlat_count(8) == 100, "irq8 has 100 samples");
    CHECK(wubu_irqlat_max(8) == 100, "irq8 max is 100");
    CHECK(wubu_irqlat_mean(8) == 50, "irq8 mean is 50 (1..100)");
    uint32_t p99 = wubu_irqlat_p99(8);
    CHECK(p99 >= 98 && p99 <= 100, "irq8 p99 near 99");

    /* Out-of-range IRQ is ignored. */
    wubu_irqlat_record(999, 5);
    wubu_irqlat_record(-1, 5);
    CHECK(wubu_irqlat_count(999) == 0, "out-of-range IRQ ignored");

    /* Different IRQs are independent. */
    wubu_irqlat_record(9, 7);
    wubu_irqlat_record(9, 9);
    CHECK(wubu_irqlat_count(9) == 2, "irq9 has 2 samples");
    CHECK(wubu_irqlat_max(9) == 9, "irq9 max is 9");
    CHECK(wubu_irqlat_count(8) == 100, "irq8 unaffected by irq9");

    /* Ring cap: recording 300 samples caps count at 256. */
    wubu_irqlat_reset();
    for (int i = 0; i < 300; i++) wubu_irqlat_record(3, (uint32_t)(i + 1));
    CHECK(wubu_irqlat_count(3) == 256, "ring caps at 256 samples");

    /* Format payload includes the IRQs with samples. */
    wubu_irqlat_reset();
    wubu_irqlat_record(8, 10);
    wubu_irqlat_record(8, 20);
    wubu_irqlat_record(9, 5);
    char buf[1024];
    wubu_irqlat_format(buf, sizeof(buf));
    CHECK(strstr(buf, "irq=8") != NULL, "format includes irq=8");
    CHECK(strstr(buf, "irq=9") != NULL, "format includes irq=9");
    CHECK(strstr(buf, "p99=") != NULL, "format includes p99 field");

    /* Empty state formats to empty string. */
    wubu_irqlat_reset();
    wubu_irqlat_format(buf, sizeof(buf));
    CHECK(buf[0] == '\0', "empty state formats to empty string");

    if (failures == 0) { printf("ALL IRQLAT TESTS PASSED\n"); return 0; }
    printf("%d FAILURES\n", failures);
    return 1;
}
