/*
 * bear_opt_test.c  --  BearRL Optimizer Closure Test (form≠function)
 *
 * Verifies that bear_optimizer_step() and bear_optimizer_zero_grad()
 * do REAL work (previously they were no-op stubs):
 *   - step() must actually move the registered weights via the Adam update
 *   - zero_grad() must actually clear the per-parameter gradient buffers
 */

#include "bear_arena.h"
#include "bear_opt.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

static int g_pass = 0, g_fail = 0, g_total = 0;
#define TEST(name) printf("  TEST %-50s", name); g_total++
#define PASS() do { printf("✅\n"); g_pass++; } while(0)
#define FAIL(msg) do { printf("❌ %s\n", msg); g_fail++; } while(0)
#define CHECK(cond, msg) do { if (!(cond)) { FAIL(msg); return; } } while(0)

static float sum_abs_diff(const float *a, const float *b, int n) {
    float s = 0.0f;
    for (int i = 0; i < n; ++i) s += fabsf(a[i] - b[i]);
    return s;
}

static void test_step_changes_weights(void) {
    TEST("bear_optimizer_step actually updates Adam weights");
    BearArena arena;
    CHECK(bear_arena_create(&arena, 16 * 1024 * 1024) == 0, "arena create");

    BearOptimizer *opt = bear_optimizer_create(&arena, BEAR_OPT_ADAM, 0.01f);
    CHECK(opt != NULL, "optimizer create");

    BearParam p;
    memset(&p, 0, sizeof(p));
    int ret = bear_param_create(&arena, &p, 4, 3, "w");
    CHECK(ret == 0, "param create");

    float w0[12];
    for (int i = 0; i < 12; ++i) { ((float*)p.weight.data)[i] = (float)(i + 1); w0[i] = (float)(i + 1); }
    for (int i = 0; i < 12; ++i) ((float*)p.grad.data)[i] = 1.0f;

    ret = bear_optimizer_register(opt, &p);
    CHECK(ret == 0, "register");

    bear_optimizer_step(opt);

    float moved = sum_abs_diff((const float*)p.weight.data, w0, 12);
    CHECK(moved > 1e-4f, "weights must move after step()");
    CHECK(moved < 0.5f, "weights should not fly away (sane Adam step)");
    bear_arena_destroy(&arena);
    PASS();
}

static void test_zero_grad_clears(void) {
    TEST("bear_optimizer_zero_grad clears gradients");
    BearArena arena;
    CHECK(bear_arena_create(&arena, 16 * 1024 * 1024) == 0, "arena create");

    BearOptimizer *opt = bear_optimizer_create(&arena, BEAR_OPT_ADAMW, 0.001f);
    CHECK(opt != NULL, "optimizer create");

    BearParam p;
    memset(&p, 0, sizeof(p));
    int ret = bear_param_create(&arena, &p, 2, 2, "z");
    CHECK(ret == 0, "param create");

    for (int i = 0; i < 4; ++i) ((float*)p.grad.data)[i] = 7.0f;

    ret = bear_optimizer_register(opt, &p);
    CHECK(ret == 0, "register");

    bear_optimizer_zero_grad(opt);

    int all_zero = 1;
    for (int i = 0; i < 4; ++i) if (((float*)p.grad.data)[i] != 0.0f) all_zero = 0;
    CHECK(all_zero, "gradients must be zero after zero_grad()");
    bear_arena_destroy(&arena);
    PASS();
}

static void test_step_multiple_params(void) {
    TEST("bear_optimizer_step updates all registered params");
    BearArena arena;
    CHECK(bear_arena_create(&arena, 16 * 1024 * 1024) == 0, "arena create");

    BearOptimizer *opt = bear_optimizer_create(&arena, BEAR_OPT_ADAM, 0.02f);
    CHECK(opt != NULL, "optimizer create");

    BearParam pa, pb;
    memset(&pa, 0, sizeof(pa)); memset(&pb, 0, sizeof(pb));
    CHECK(bear_param_create(&arena, &pa, 3, 2, "a") == 0, "param a");
    CHECK(bear_param_create(&arena, &pb, 2, 4, "b") == 0, "param b");

    float a0[6], b0[8];
    for (int i = 0; i < 6; ++i) { ((float*)pa.weight.data)[i] = (float)(i+1); a0[i]=(float)(i+1); ((float*)pa.grad.data)[i]=1.0f; }
    for (int i = 0; i < 8; ++i) { ((float*)pb.weight.data)[i] = (float)(i+1); b0[i]=(float)(i+1); ((float*)pb.grad.data)[i]=0.5f; }

    CHECK(bear_optimizer_register(opt, &pa) == 0, "register a");
    CHECK(bear_optimizer_register(opt, &pb) == 0, "register b");

    bear_optimizer_step(opt);

    CHECK(sum_abs_diff((const float*)pa.weight.data, a0, 6) > 1e-4f, "param a moved");
    CHECK(sum_abs_diff((const float*)pb.weight.data, b0, 8) > 1e-4f, "param b moved");
    bear_arena_destroy(&arena);
    PASS();
}

static void test_step_sgd_updates(void) {
    TEST("bear_optimizer_step (SGD) updates weights");
    BearArena arena;
    CHECK(bear_arena_create(&arena, 16 * 1024 * 1024) == 0, "arena create");

    BearOptimizer *opt = bear_optimizer_create(&arena, BEAR_OPT_SGD, 0.1f);
    CHECK(opt != NULL, "optimizer create");

    BearParam p;
    memset(&p, 0, sizeof(p));
    CHECK(bear_param_create(&arena, &p, 2, 2, "s") == 0, "param create");

    float w0[4];
    for (int i = 0; i < 4; ++i) { ((float*)p.weight.data)[i] = 1.0f; w0[i]=1.0f; ((float*)p.grad.data)[i]=2.0f; }

    CHECK(bear_optimizer_register(opt, &p) == 0, "register");
    bear_optimizer_step(opt);

    CHECK(sum_abs_diff((const float*)p.weight.data, w0, 4) > 1e-4f, "SGD step moves weights");
    bear_arena_destroy(&arena);
    PASS();
}

int main(void) {
    printf("+========================================================+\n");
    printf("|  BearRL Optimizer Closure Test                        |\n");
    printf("+========================================================+\n\n");

    test_step_changes_weights();
    test_zero_grad_clears();
    test_step_multiple_params();
    test_step_sgd_updates();

    printf("\n========================================================\n");
    printf("  Results: %d/%d passed, %d failed\n", g_pass, g_total, g_fail);
    printf("========================================================\n");

    return g_fail > 0 ? 1 : 0;
}
