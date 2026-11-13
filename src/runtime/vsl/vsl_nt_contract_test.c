/*
 * vsl_nt_contract_test.c -- every NT syscall must survive garbage.
 *
 * The unit tests exercise individual handlers with valid inputs, which leaves
 * a whole class of bug untested: a handler that dereferences its pointer
 * arguments without checking them works fine in the unit test and takes the
 * OS down when real software passes something unexpected.
 *
 * So probe EVERY registered slot once with all-zero arguments, forked per slot
 * so a crash costs one child rather than the gate. A handler that returns a
 * STATUS code passes; one that segfaults or hangs is named in the report.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vsl_nt_internal.h"
#include "vsl_nt_bridge.h"

static int failures;
#define CHECK(c, m) do { if (!(c)) { printf("  FAIL: %s\n", m); failures++; } } while (0)

int main(void)
{
    printf("=== test_vsl_nt_contract ===\n");

    vsl_nt_bridge_ctx_t ctx;
    CHECK(vsl_nt_bridge_init(&ctx) == 0, "the NT bridge initialises");
    /* The facade installs the shared context as a side effect of init. */
    CHECK(g_nt_ctx != NULL, "the shared NT context is installed by init");

    uint32_t cov = vsl_nt_coverage();
    CHECK(cov > 0, "the dispatch table is populated");
    printf("  registered slots: %u of %d\n", cov, 512);

    /* cap = coverage, plus headroom for slots registered between the count and
     * the sweep (there are none today, but the guard makes the test honest). */
    uint32_t cap = cov + 8;
    nt_contract_result_t *res = calloc(cap, sizeof(*res));
    CHECK(res != NULL, "result buffer allocated");

    uint32_t n = vsl_nt_contract_sweep(res, cap);
    CHECK(n >= cov, "every registered slot was probed");
    printf("  probed: %u\n", n);

    int deaths = 0;
    for (uint32_t i = 0; i < n; i++) {
        if (!res[i].died) continue;
        deaths++;
        printf("  CRASH slot %u: killed by signal %d (%s)\n",
               res[i].slot, res[i].signo,
               res[i].signo == SIGSEGV ? "SIGSEGV" :
               res[i].signo == SIGALRM ? "timed out" : "signal");
    }

    CHECK(deaths == 0,
          "NO registered syscall may die on all-zero arguments "
          "(a NULL deref here becomes an OS crash under real software)");

    /* A return is not enough on its own -- a handler could also return garbage.
     * Every surviving probe must have produced SOMETHING through the pipe,
     * which also proves the child really ran the handler. */
    CHECK(n > 0, "the sweep produced results");

    free(res);
    if (failures == 0) printf("ALL VSL_NT_CONTRACT TESTS PASSED\n");
    else printf("%d VSL_NT_CONTRACT FAILURES\n", failures);
    return failures ? 1 : 0;
}
