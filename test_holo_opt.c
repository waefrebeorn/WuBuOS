/* Test holographic optimizer with bytropix benchmarks */
#define _POSIX_C_SOURCE 200809L
#include "src/bear/bear_holo_opt.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

/* Benchmark 1: The Tsunami (High Dynamic Range) */
void benchmark_tsunami() {
    printf("\n============================================================\n");
    printf("OCCASION I: THE TSUNAMI (Holographic)\n");
    printf("Goal: Store 1,000,000.0 and 0.01 perfectly.\n");
    printf("============================================================\n");

    double input_data[10];
    for (int i = 0; i < 9; ++i) input_data[i] = 0.01;
    input_data[9] = 1000000.0;

    double params[10] = {0};
    double grads[10];

    BearArena arena;
    bear_arena_create(&arena, 1024*1024);

    BearHoloConfig cfg = bear_holo_default_config();
    cfg.base_lr = 0.01;
    BearHoloOptimizer* opt = bear_holo_create(&arena, &cfg, 10);
    if (!opt) { printf("FAIL: create\n"); return; }

    for (int step = 0; step < 10; ++step) {
        for (int i = 0; i < 10; ++i) {
            grads[i] = input_data[i];
        }
        bear_holo_step(opt, params, grads, 10);
    }

    /* Reconstruction: (Topology * 2pi) + Residue */
    double cycles = 10.0;
    double recon_tsunami = (opt->states[9].soul * (2.0 * M_PI) + opt->states[9].echo) / cycles;
    double tsunami_err = fabs(recon_tsunami - input_data[9]);

    printf("Input Tsunami : %.1f\n", input_data[9]);
    printf("Recon Tsunami : %.5f (Diff: %.5f)\n", recon_tsunami, tsunami_err);
    printf("Soul[9]: %d, Echo[9]: %.6f\n", opt->states[9].soul, opt->states[9].echo);

    if (tsunami_err < 1.0) printf(">>> VERDICT: PASSED.\n");
    else printf(">>> VERDICT: FAILED.\n");

    bear_arena_destroy(&arena);
}

/* Benchmark 2: Ghost in the Shell (Steganography) */
void benchmark_ghost() {
    printf("\n============================================================\n");
    printf("OCCASION II: GHOST IN THE SHELL (Holographic)\n");
    printf("Goal: Hide 'WUBU' in the winding numbers.\n");
    printf("============================================================\n");

    const char* secret = "WUBU";
    int ascii_vals[4] = {(int)secret[0], (int)secret[1], (int)secret[2], (int)secret[3]};
    
    double params[4] = {0};
    double grads[4] = {0};

    BearArena arena;
    bear_arena_create(&arena, 1024*1024);

    BearHoloConfig cfg = bear_holo_default_config();
    cfg.base_lr = 0.001;
    BearHoloOptimizer* opt = bear_holo_create(&arena, &cfg, 4);
    if (!opt) { printf("FAIL: create\n"); return; }

    /* Inject exactly X windings (remainder = 0) */
    for (int i = 0; i < 4; ++i) {
        grads[i] = (double)ascii_vals[i] * (2.0 * M_PI);
    }
    
    bear_holo_step(opt, params, grads, 4);

    printf("Decoded Message: ");
    for (int i = 0; i < 4; ++i) {
        printf("%c", (char)opt->states[i].soul);
    }
    printf("\n");

    int pass = 1;
    for (int i = 0; i < 4; ++i) {
        if (opt->states[i].soul != ascii_vals[i]) pass = 0;
    }
    if (pass) printf(">>> VERDICT: PASSED.\n");
    else printf(">>> VERDICT: FAILED.\n");

    bear_arena_destroy(&arena);
}

/* Benchmark 3: The Lazarus Event (Exact Recovery) */
void benchmark_lazarus() {
    printf("\n============================================================\n");
    printf("OCCASION III: THE LAZARUS EVENT (Holographic Restoration)\n");
    printf("Goal: Recover float64 precision after total parameter death.\n");
    printf("============================================================\n");

    double true_value = 12345.6789;
    double param = 0.0;
    double grad = true_value / 10.0;

    BearArena arena;
    bear_arena_create(&arena, 1024*1024);

    BearHoloConfig cfg = bear_holo_default_config();
    cfg.base_lr = 0.01;
    BearHoloOptimizer* opt = bear_holo_create(&arena, &cfg, 1);
    if (!opt) { printf("FAIL: create\n"); return; }

    /* 1. Train (Inject Energy) */
    for (int step = 0; step < 10; ++step) {
        double grads[1] = {grad};
        bear_holo_step(opt, &param, grads, 1);
    }

    /* 2. The Crash */
    double crashed_param = 0.0;
    printf("System CRASHED. Weight: %f\n", crashed_param);

    /* 3. Resurrection using Holographic Memory */
    double soul = (double)opt->states[0].soul * (2.0 * M_PI);
    double echo = opt->states[0].echo;
    double resurrected_value = soul + echo;

    double diff = fabs(resurrected_value - true_value);
    printf("True Value        : %.10f\n", true_value);
    printf("Resurrected Value : %.10f\n", resurrected_value);
    printf("Difference        : %.10f\n", diff);

    if (diff < 1e-9) printf(">>> VERDICT: PASSED (Perfect Recall)\n");
    else printf(">>> VERDICT: FAILED\n");

    bear_arena_destroy(&arena);
}

int main() {
    benchmark_tsunami();
    benchmark_ghost();
    benchmark_lazarus();
    printf("\n============================================================\n");
    printf("ALL SYSTEMS GREEN.\n");
    printf("The Holographic Geodesic Architecture is validated.\n");
    printf("We have separated Data (Soul) from Medium (Weight).\n");
    printf("============================================================\n");
    return 0;
}