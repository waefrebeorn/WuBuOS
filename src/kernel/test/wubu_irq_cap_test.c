/* IRQ_ACQUIRE gate: registration is authority-gated on the real path. */
#include "interrupt.h"
#include <stdio.h>

static int fails;
#define CHECK(c, m) do { if (!(c)) { printf("FAIL: %s\n", m); ++fails; } } while (0)

static void test_isr(uint8_t irq, void *ctx) { (void)irq; (void)ctx; }

int main(void)
{
    interrupt_irq_cap_init();
    CHECK(interrupt_register_ex(200, test_isr, NULL) == 0,
          "granted IRQ registration");
    CHECK(interrupt_handler_installed(200) == 1, "handler installed");

    CHECK(interrupt_irq_cap_revoke() == 0, "revoke IRQ cap");
    CHECK(interrupt_register_ex(201, test_isr, NULL) != 0,
          "revoked IRQ registration refused");
    CHECK(interrupt_handler_installed(201) == 0, "no handler after refusal");

    CHECK(interrupt_irq_cap_grant() == 0, "grant IRQ cap");
    CHECK(interrupt_register_ex(202, test_isr, NULL) == 0,
          "regranted IRQ registration");
    CHECK(interrupt_handler_installed(202) == 1, "handler after regrant");

    if (fails == 0) printf("irq_cap_gate: PASS\n");
    else printf("irq_cap_gate: %d FAILURES\n", fails);
    return fails ? 1 : 0;
}
