/*
 * jit_loop_test.c -- Verify the loop analysis subsystem (Subsystem B):
 * induction-variable detection (#12), closed-form trip count, loop-invariant
 * detection (#13), and strength-reduction candidates.
 *
 * The DISCRIMINATORS:
 *   - a canonical `while(i<n){ i=i+1; s=s+i; }` yields i as an IV with
 *     stride +1 and a closed-form trip count (n-init).
 *   - a var assigned a constant (not depending on the IV or a written var) is
 *     loop-invariant and is a #13 hoist candidate.
 *   - `s += i*K` with IV striding by 1 gives per_iter_add = K (#12: multiply
 *     replaced by a constant add).
 * These prove the analysis engine is correct before it is wired into codegen.
 */
#include "jit_minic_loop.h"
#include <stdio.h>
#include <string.h>
#include <stdint.h>

static int pass, fail;
#define CHECK(cond, msg) do { if (cond) pass++; else { fail++; printf("FAIL: %s\n", msg); } } while(0)

int main(void) {
    /* --- Loop 1: while(i<n){ i=i+1; s=s+i; } with init i=0, bound 10 --- */
    {
        LoopBody lb; minic_loop_body_init(&lb);
        minic_loop_add_assign(&lb, "s", '=', "s", "", 0);
        minic_loop_add_assign(&lb, "i", '+', "i", "", 1);   /* i = i + 1  (IV stride +1) */
        minic_loop_add_assign(&lb, "s", '+', "s", "i", 0);  /* s = s + i  */
        int64_t trip; char iv[64]; int64_t stride;
        int n = minic_loop_analyze(&lb, 0, '<', 10, &trip, iv, &stride);
        CHECK(n == 1, "Loop1: found an IV");
        CHECK(strcmp(iv, "i") == 0, "Loop1: IV is 'i'");
        CHECK(stride == 1, "Loop1: IV stride +1");
        CHECK(trip == 10, "Loop1: closed-form trip count 10 (i<10 from 0, +1)");
    }

    /* --- Loop 2: while(i>=0){ i=i-1; } init 5 -> 6 iterations --- */
    {
        LoopBody lb; minic_loop_body_init(&lb);
        minic_loop_add_assign(&lb, "i", '-', "i", "", 1);
        int64_t trip; char iv[64]; int64_t stride;
        int n = minic_loop_analyze(&lb, 5, '>', -1, &trip, iv, &stride);
        CHECK(n == 1 && stride == -1, "Loop2: IV stride -1");
        CHECK(trip == 6, "Loop2: i>=-1 from 5 step -1 => 6 iters (5,4,3,2,1,0)");
    }

    /* --- Loop 3: invariant detection. while(...){ c=42; s=s+i; } ---
     * c=42 does not depend on any written var -> loop-invariant (#13 hoist). */
    {
        LoopBody lb; minic_loop_body_init(&lb);
        minic_loop_add_assign(&lb, "c", '=', "", "", 42);   /* c = 42 (invariant) */
        minic_loop_add_assign(&lb, "s", '+', "s", "i", 0);
        int n = minic_loop_invariant_count(&lb);
        CHECK(n >= 1, "Loop3: 'c=42' detected as loop-invariant (#13 hoist candidate)");
    }

    /* --- Loop 4: strength reduction. s += i*3 with i striding +1
     *     => per_iter_add = 1*3 = 3 (multiply replaced by const add). --- */
    {
        LoopBody lb; minic_loop_body_init(&lb);
        minic_loop_add_assign(&lb, "i", '+', "i", "", 1);
        int64_t add;
        int ok = minic_loop_strength_candidate(&lb, "s", "i", 3, &add);
        CHECK(ok && add == 3, "Loop4: s+=i*3 strength-reduces to per-iter add 3");
    }

    /* --- Loop 5: trip count with stride -1 and strict > bound.
     *     i=10 while(i>0) step -1 => 10 iters. --- */
    {
        LoopBody lb; minic_loop_body_init(&lb);
        minic_loop_add_assign(&lb, "i", '-', "i", "", 1);
        int64_t trip; char iv[64]; int64_t stride;
        int n = minic_loop_analyze(&lb, 10, '>', 0, &trip, iv, &stride);
        CHECK(n == 1 && trip == 10, "Loop5: i=10 while(i>0) step -1 => 10 iters");
    }

    printf("=== jit_loop_test: %d passed, %d failed ===\n", pass, fail);
    return fail == 0 ? 0 : 1;
}
