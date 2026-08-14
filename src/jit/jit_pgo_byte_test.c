#include "jit.h"
#include "jit_branch_profile.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

int main(void) {
    if (!getenv("WUBU_JIT_PGO") || getenv("WUBU_JIT_PGO")[0] != '1') {
        printf("SKIP: set WUBU_JIT_PGO=1\n"); return 0;
    }
    JITContext *ctx = jit_init();
    JITFunc fn; JITResult r;
    /* if(x<100) return 1; else return 2; — 1 conditional => 1 counter site */
    const char *src =
        "long f(long x){"
        "  if (x < 100) { return 1; }"
        "  else { return 2; }"
        "}";
    r = jit_compile(ctx, src, JIT_LANG_C, "f", &fn);
    if (r != 0) { printf("FAIL: rc=%d\n", r); jit_free(ctx); return 1; }

    typedef int64_t (*fp)(int64_t);
    fp f = (fp)fn.code;

    /* Call twice: once takes the if (x=50), once takes the else (x=200) */
    int64_t v1 = f(50);   /* if taken => not-taken counter NOT incremented */
    int64_t v2 = f(200);  /* if not taken => not-taken counter incremented */

    printf("f(50)=%ld f(200)=%ld\n", (long)v1, (long)v2);

    /* The profile counter for this branch (id=0): not_taken should be 1
     * (the x=200 call fell through the jcc). */
    int64_t taken = *jbp_counter_taken(0);
    int64_t not_taken = *jbp_counter_not_taken(0);
    printf("branch0: taken=%ld not_taken=%ld\n", (long)taken, (long)not_taken);

    int ok = (v1 == 1 && v2 == 2 && not_taken >= 1);
    printf("=== pgo_counter_test: %s ===\n", ok ? "PASS" : "FAIL");
    jit_free(ctx);
    return ok ? 0 : 1;
}
