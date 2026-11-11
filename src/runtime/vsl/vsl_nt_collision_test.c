/*
 * vsl_nt_collision_test.c -- the NT dispatch registration contract.
 *
 * Modules write raw slots into a shared 512-entry table, so a module claiming a
 * slot another module owns SILENTLY WINS.  Registration order was the only
 * thing deciding, and nothing recorded it.
 *
 * This test runs the real registration and asserts that NO module takes a slot
 * another module already claimed.  A failure names both modules and the slot,
 * which is exactly the information needed to resolve it.
 */
#include <stdio.h>
#include <string.h>

#include "vsl_nt_internal.h"
#include "vsl_nt_bridge.h"

static int failures;
#define CHECK(c, m) do { if (!(c)) { printf("  FAIL: %s\n", m); failures++; } } while (0)

int main(void)
{
    printf("=== test_vsl_nt_collision ===\n");

    vsl_nt_bridge_ctx_t ctx;
    CHECK(vsl_nt_bridge_init(&ctx) == 0, "the NT bridge initialises");

    /* The registration ran; the table must be populated. */
    uint32_t cov = vsl_nt_coverage();
    CHECK(cov > 0, "the dispatch table is populated");
    printf("  coverage: %u slots of %d\n", cov, 512);

    /* A real handler replacing a STUB is an intended upgrade. */
    printf("  stub upgrades: %u (a real handler replaced a placeholder)\n",
           vsl_nt_stub_replace_count());

    /* THE CONTRACT: two REAL handlers must never claim the same slot. */
    uint32_t nc = vsl_nt_collision_count();
    for (uint32_t i = 0; i < nc; i++) {
        const nt_collision_t *c = vsl_nt_collision_at(i);
        if (c) printf("  COLLISION slot %d: %s was overwritten by %s\n",
                      c->slot, c->was, c->now);
    }
    CHECK(nc == 0,
          "NO two real handlers claim the same slot "
          "(a silent overwrite means a syscall works only by registration order)");

    /* The report is bounded and safe. */
    CHECK(vsl_nt_collision_at(nc) == NULL, "the collision list ends at its count");
    CHECK(vsl_nt_collision_at(9999) == NULL, "an out-of-range index is safe");

    if (failures == 0) printf("ALL VSL_NT_COLLISION TESTS PASSED\n");
    else printf("%d VSL_NT_COLLISION FAILURES\n", failures);
    return failures ? 1 : 0;
}
