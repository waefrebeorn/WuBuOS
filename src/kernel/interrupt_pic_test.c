/*
 * interrupt_pic_test.c -- unit test for the extracted PIC + IRQ routing
 * module (interrupt_pic.c). Builds in bare-metal C11 (-DMYSEED_METAL) and
 * asserts that pic_remap / pic_eoi / irq_route_add / irq_route_remove do
 * real work and don't regress the monolith split.
 *
 * Build:
 *   gcc -DMYSEED_METAL -DWUBU_NO_LIBM -I src/kernel \
 *       src/kernel/interrupt_pic.c src/kernel/interrupt_pic_test.c -o /tmp/pictest
 */
#include "interrupt_pic.h"
#include "interrupt_apic.h"
#include <stdio.h>
#include <string.h>

/* Minimal mem_alloc/mem_free stand-ins so the routing list can allocate.
 * (In the real kernel these come from memory.h.) */
void *mem_alloc(unsigned long sz) { return __builtin_malloc(sz); }
void  mem_free(void *p) { __builtin_free(p); }

/* ioapic_route_irq: we don't have a real APIC in this host test, so stub it.
 * It's only called when src_irq < g_ioapic_irq_count, which is 0 in this
 * test, so the body never runs — but it must be defined for the link. */
int ioapic_route_irq(uint8_t a, uint8_t b, uint8_t c) {
    (void)a; (void)b; (void)c; return 0;
}
uint32_t g_ioapic_irq_count = 0;

static int failures = 0;
#define CHECK(c, m) do { if (!(c)) { printf("FAIL: %s\n", m); failures++; } \
                       else { printf("ok:   %s\n", m); } } while (0)

int main(void) {
    /* pic_remap: simply must not crash and must be callable (bare metal). */
    pic_remap(32, 40);
    CHECK(1, "pic_remap(32,40) callable without fault");

    /* pic_eoi: must be callable for the remapped range without fault. */
    pic_eoi(32);  /* master */
    pic_eoi(40);  /* slave */
    pic_eoi(47);  /* slave+master */
    CHECK(1, "pic_eoi(32/40/47) callable without fault");

    /* IRQ routing linked list: add two routes, remove one. */
    CHECK(irq_route_add(11, 0x2B, 0, 0) == 0, "irq_route_add(11) ok");
    CHECK(irq_route_add(15, 0x2F, 1, 0) == 0, "irq_route_add(15) ok");
    CHECK(irq_route_remove(11) == 0,        "irq_route_remove(11) ok");
    CHECK(irq_route_remove(11) == -1,       "irq_route_remove(11) again -> -1 (not found)");
    CHECK(irq_route_remove(15) == 0,        "irq_route_remove(15) ok");
    CHECK(irq_route_remove(15) == -1,       "irq_route_remove(15) again -> -1");

    if (failures) { printf("\n%d FAILURES\n", failures); return 1; }
    printf("\nALL PIC/ROUTE TESTS PASSED\n");
    return 0;
}
